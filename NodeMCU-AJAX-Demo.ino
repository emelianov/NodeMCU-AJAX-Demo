/*--------------------------------------------------------------
  NodeMCU AJAX Demo
  Original code author is
                W.A. Smith, http://startingelectronics.com
  Program:      eth_websrv_SD_Ajax_in_out

  Description:  NodeMCU web server that displays analog input,
                the state of 5 switches and controls 2 outputs
                and 2 on-board LEDs 2 using checkboxes and 2 
                buttons. The web page is stored as file on 
                internal flash file system.
  
  Hardware:     NodeMCU board. Should work with other ESP8266
                boards.
                A0 Analog input
                D0 NodeMCU LED
                D1 Input
                D2 Input
                D3 Input. Pull-up
                D4 ESP-12 LED
                D5 Output
                D6 Output
                D7 Input
                D8 Input. Pull-down
                
  Software:     Developed using Arduino 1.8.1 software
                ESP8266/Arduino 2.3.0
                Should be compatible with Arduino 1.0 + and
                newer ESP8266/Arduino releases if any
                Internal flash File system should contain web
                page called /index.htm. Use ESP8266FS Tool
                to upload File system contents via Arduino IDE.
  
  References:   - WebServer example by David A. Mellis and 
                  modified by Tom Igoe
                - ESP8266/Arduino bundeled examples
                
  Date:         4 April 2013
  Modified:     19 June 2013
                - removed use of the String class
                17 March 2017
                - Rewrite to be used with NodeMCU
 
  Author:       W.A. Smith, http://startingelectronics.com
  Modifed:      a.m.emelianov@gmail.com https://gitbub.com/emelianov
--------------------------------------------------------------*/

#include <ESP8266WiFi.h>          // ESP8266 base Wi-Fi library 
#include <WiFiClient.h>           // Implementation of standatd Arduino network routines
#include <FS.h>                   // SFIFFS local flash storge File system. Implements the same API as SD card library

#define SSID "SSID"               // Wi-Fi Access point SSID
#define PASSWORD "PASSWORD"       // Wi-Fi password

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   400

IPAddress ip(192, 168, 30, 129);  // IP address
IPAddress mask(255, 255, 255, 0); // Netmask
IPAddress gw(192, 168, 30, 4);    // Default gateway
IPAddress dns(192, 168, 30, 4);   // DNS
WiFiServer server(80);            // create a server at port 80
File webFile;                     // the web page file
char HTTP_req[REQ_BUF_SZ] = {0};  // buffered HTTP request stored as null terminated string
char req_index = 0;               // index into HTTP_req buffer
boolean LED_state[4] = {0};       // stores the states of the LEDs

void setup()
{
    
    Serial.begin(9600);           // for debugging
    
    // initialize File system
    Serial.println("Initializing File system...");
    if (!SPIFFS.begin()) {
        Serial.println("ERROR - File system initialization failed!");
        return;    // init failed
    }
    Serial.println("SUCCESS - File system initialized.");
    // check for /index.htm file
    // Note each file must begins with slash.
    if (!SPIFFS.exists("/index.htm")) {
        Serial.println("ERROR - Can't find /index.htm file!");
        return;                 // can't find index file
    }
    Serial.println("SUCCESS - Found /index.htm file.");
    // switches on pins D1, D2, D3, D7 and D8
    pinMode(D1, INPUT);
    pinMode(D2, INPUT);
    pinMode(D3, INPUT);         // Pull-Up. Flash button switches pin to LOW.
    pinMode(D7, INPUT);
    pinMode(D8, INPUT);         // Pull-Down
    // LEDs
    pinMode(D4, OUTPUT);        // ESP8266 LED
    pinMode(D5, OUTPUT);
    pinMode(D6, OUTPUT);
    pinMode(D0, OUTPUT);        // NodeMCU LED
    // LEDs is on in LOW state that is default when we switching pin to OUTPUT. So we have to switch thay off.
    digitalWrite(D4, HIGH);
    digitalWrite(D0, HIGH);
  
    WiFi.mode(WIFI_STA);        // Initialize Wi-Fi in STAtion mode. NodeMCU will act as Client.
    //WiFi.mode(WIFI_AP);       // Initialize Wi-Fi in AP mode. Node MCU will act as Access Point. So You can
                                // to it directly. Address of AP is 192.168.4.1. No password. AP is open.
    //WiFi.config(ip, mask, gw, dns); // Use static IP settings  
    WiFi.begin(SSID, PASSWORD); // Start connecting to AP with ssid and password
    Serial.print("Connecting Wi-Fi.");
    // Wait until connection succesfull
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
    }
    Serial.println();
    Serial.print("SUCCESS - Local IP: ");
    Serial.println(WiFi.localIP());     // Prints IP address got
    
    server.begin();             // start to listen for clients
}

void loop()
{
    WiFiClient client = server.available();  // try to get client

    if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                // limit the size of the stored received HTTP request
                // buffer first part of HTTP request in HTTP_req array (string)
                // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                if (req_index < (REQ_BUF_SZ - 1)) {
                    HTTP_req[req_index] = c;          // save HTTP request character
                    req_index++;
                }
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    // remainder of header follows below, depending on if
                    // web page or XML page is requested
                    // Ajax request - send XML file
                    if (StrContains(HTTP_req, "ajax_inputs")) {
                        // send rest of HTTP header
                        client.println("Content-Type: text/xml");
                        client.println("Connection: keep-alive");
                        client.println();
                        SetLEDs();
                        // send XML file containing input states
                        XML_response(client);
                    }
                    else {  // web page request
                        // send rest of HTTP header
                        client.println("Content-Type: text/html");
                        client.println("Connection: close");
                        // In original sketch was connection argument as below. But it's wrong.
                        // client.println("Connection: keep-alive");
                        client.println();
                        // send web page
                        // open web page file read only. Second parameter "r"/"w" is required for SPIFFS but can be skiped for SD.
                        webFile = SPIFFS.open("/index.htm", "r");
                        if (webFile) {
                            while(webFile.available()) {
                                client.write(webFile.read()); // send web page to client
                            }
                            webFile.close();
                        }
                        
                    }
                    // display received HTTP request on serial port
                    Serial.print(HTTP_req);
                    // reset buffer index and all buffer elements to 0
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}

// checks if received HTTP request is switching on/off LEDs
// also saves the state of the LEDs
void SetLEDs(void)
{
    // LED 1 (pin D4)
    if (StrContains(HTTP_req, "LED1=1")) {
        LED_state[0] = 1;       // save LED state
        digitalWrite(D4, LOW);  // For D4 and D0 use LOW output for 'on' state  
    }
    else if (StrContains(HTTP_req, "LED1=0")) {
        LED_state[0] = 0;       // save LED state
        digitalWrite(D4, HIGH); // and HIGH for 'off' state
    }
    // LED 2 (pin D5)
    if (StrContains(HTTP_req, "LED2=1")) {
        LED_state[1] = 1;       // save LED state
        digitalWrite(D5, HIGH);
    }
    else if (StrContains(HTTP_req, "LED2=0")) {
        LED_state[1] = 0;       // save LED state
        digitalWrite(D5, LOW);
    }
    // LED 3 (pin D6)
    if (StrContains(HTTP_req, "LED3=1")) {
        LED_state[2] = 1;       // save LED state
        digitalWrite(D6, HIGH);
    }
    else if (StrContains(HTTP_req, "LED3=0")) {
        LED_state[2] = 0;       // save LED state
        digitalWrite(D6, LOW);
    }
    // LED 4 (pin D0)
    if (StrContains(HTTP_req, "LED4=1")) {
        LED_state[3] = 1;       // save LED state
        digitalWrite(D0, LOW);
    }
    else if (StrContains(HTTP_req, "LED4=0")) {
        LED_state[3] = 0;       // save LED state
        digitalWrite(D0, HIGH);
    }
}

// send the XML file with analog values, switch status
//  and LED status
void XML_response(WiFiClient cl)
{
    int analog_val;                     // stores value read from analog inputs
    int sw_arr[] = {D1, D2, D3, D7, D8};// pins interfaced to switches
    
    cl.print("<?xml version = \"1.0\" ?>");
    cl.print("<inputs>");
    // read analog input
    // ESP8266 has only one Analog input. It's 10bit (1024 values) Measuring range is 0-1V.
        analog_val = analogRead(A0);
        cl.print("<analog>");
        cl.print(analog_val);
        cl.println("</analog>");
    // read switches
    for (int count = 0; count < sizeof(sw_arr)/sizeof(sw_arr[0]); count++) {
        cl.print("<switch>");
        if (digitalRead(sw_arr[count])) {
            cl.print("ON");
        }
        else {
            cl.print("OFF");
        }
        cl.println("</switch>");
    }
    // checkbox LED states
    // LED1
    cl.print("<LED>");
    if (LED_state[0]) {
        cl.print("checked");
    }
    else {
        cl.print("unchecked");
    }
    cl.println("</LED>");
    // LED2
    cl.print("<LED>");
    if (LED_state[1]) {
        cl.print("checked");
    }
    else {
        cl.print("unchecked");
    }
     cl.println("</LED>");
    // button LED states
    // LED3
    cl.print("<LED>");
    if (LED_state[2]) {
        cl.print("on");
    }
    else {
        cl.print("off");
    }
    cl.println("</LED>");
    // LED4
    cl.print("<LED>");
    if (LED_state[3]) {
        cl.print("on");
    }
    else {
        cl.print("off");
    }
    cl.println("</LED>");
    
    cl.print("</inputs>");
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}
