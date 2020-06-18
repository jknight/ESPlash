/****************************************************************************
 *                                  Aknowledments                           * 
 *                                  by LucioPGN                             * 
 ****************************************************************************/
/*  Up to this date: 07th of June 2020 I don't consider myself a programer 
 *  so I need to stand on top of giants sholders for my programing projects:
 *  A Portion of this code was based on Rui Santos Code; 
 *  Codes from Rui Santos mixed toghether:
 *  https://randomnerdtutorials.com/esp32-web-server-spiffs-spi-flash-file-system/
 *  https://randomnerdtutorials.com/esp32-relay-module-ac-web-server/
 *  https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
 *  From Techtutorialsx.com
 *  https://techtutorialsx.com/2017/12/01/esp32-arduino-asynchronous-http-webserver/
 *  A Portion of this code was based on Shakeels code for ESP8266 ;
 *  My contributions: 
 *     -So far I made it work on platformio :), that took me quite a lot of time
 *     -That means:
 *        +created a new project;
 *        +created a folder named data under the main folder (fabfarm_irrigation)
 *        +linked the platformio.ini to the folder of the project + the data folder
 *        +linked the needed libraries to their github repo in the platformio.ini
 *        +found a conflict with time library and A
 *  Things I still want to program for my final project:
 *    -so far I ported Shakeels code into ESP32;
 *    -separate functions from shakeels code into files;
 *    -separated functions from Rui Santos code into files.
 *    -simplify the code creating more functions;
 *    -try to separate the HTML part for a cleaner code;
 *    -Improve the appearance/Interface of the code
 *    -Add readings to HTML
 *    -Add a log of occurrences like over current
 *    -Add more safety for the equipment
 *    -Add a phone interface (APP)
 *    -Add function to set current time
 *    -Add renaming function to each relay so one can relate the relay to the area of interest or at least rename relays to actual areas of the farm.
 * 
 ****************************************************************************/


//Required Libraries
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "AsyncTCP.h"
#include "Adafruit_Sensor.h"
#include "DHT.h"
//Documentation here --> https://github.com/PaulStoffregen/Time
//#include "time.h"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

// Network Credentials
const char* ssid = "rato";
const char* password = "imakestuff";

//Start the Async Web Server listening on port 80
AsyncWebServer server(80);

// Set to true to define Relay as Normally Open (NO)
#define relayNO false

//TODO: these should be loaded from json not hard coded:
#define zones 3
int relayGPIOs[zones] = {26, 25, 33};
int pumpGpio = 27;

// Digital pin connected to the DHT sensor
#define DHTPIN 32     

// Uncomment the type of sensor in use:
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

//Send the pin and type of sensor
DHT dht(DHTPIN, DHTTYPE);

// TODO: get rid of these
const char* PARAM_INPUT_1 = "relay";  
const char* PARAM_INPUT_2 = "state";

// TODO: get rid of this
String timerSliderValue = "60";

// Main entry point / start up routine
void setup(){ // 

    // Serial port for debugging purposes
    Serial.begin(9600);

    // Function to initialize Spiffs file system
    getSpiffGoing();

    // TODO: read data file here ...
    // readDataFile(); 

    // Set all relays to off when the program starts - if set to Normally Open (NO), the relay is off when you set the relay to HIGH
    turnRelaysToOff();

    //Function to start the wifi softap and client going
    getWifiGoing();

    // Init and get the time from ntpServer
    // some info on https://lastminuteengineers.com/esp32-ntp-server-date-time-tutorial/
    // and here https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
    // struct tm info: http://www.cplusplus.com/reference/ctime/tm/
    // Still about Struct https://learn.adafruit.com/circuit-playground-simple-simon/the-structure-of-struct
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    //Print time in different ways to Serial monitor
    testPrintTimeWays();

    /*
     *Now we are going to configure the route where server will be listening for incoming HTTP requests 
     and a function that will be executed when a request is received on that route.
     We specify this by calling the "on" method on the server object. With server.on(){}; 
     As first input, this method receives a string with the path where it will be listening. 
     We are going to set it to listen for requests on the “/” route. This could be anything. 
     It is basically what you write after the ip adress when in the browser or an APP.
     This website has a great explanation of the ESP32 Arduino: Asynchronous HTTP web server
https://techtutorialsx.com/2017/12/01/esp32-arduino-asynchronous-http-webserver/
So... 
- First parameter here is: "/" thats the root directory.
- Second parameter is HTTP_GET thats an enum of type WebRequestMethod a method defined in the library here --> https://github.com/me-no-dev/ESPAsyncWebServer/blob/63b5303880023f17e1bca517ac593d8a33955e94/src/ESPAsyncWebServer.h
- Third parameter is a the function AsyncWebServerRequest
So there is this c++ lambda function used here. My litle understanding is that they are locally declared unamed function this means they dont have a name and are declared locally :-)
I don't grasp the concept fully haha.
the syntax is [captures](params){body} where in here [] is empity
     */

    // Route for root / web page

    // Serve up index.html 
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", String(), false, processor);
    });

    server.on("/logo", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/logo.jpeg", "image/jpeg");
    });

    //server.on("/mashup/indexold.html", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send(SPIFFS, "/mashup/indexold.html", String(), false, processor);
    //});

    server.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "application/json", getStats().c_str());
    });

    // fetch current settings as json (temp, humidity, etc)
    server.on("/getSettings", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send_P(200, "application/json", getSettings().c_str());
    });
 
    // stop a currently running valve by pin number. url: [ESP_IP]/stop?pin={pin}
    server.on("/stop", HTTP_GET, [] (AsyncWebServerRequest *request) {
        //TODO: Pull json from body

        // GET input1 value on <ESP_IP>/update?relay=<inputMessage>
        if (request->hasParam("pin") ) { 
            // grab the pin # that we want to stop from the url 
            String pin = request->getParam("pin")->value();
            //TODO: stop valve by pin here
        } else {
            Serial.println("Did not receive a pin on the stop request");
        }

        //This method receives as first input the HTTP response code, which will be 200 in our case.  This is the HTTP response code for “OK”.
        // NOTE: we can update this to return 200 ok vs 300 fail later 
        request->send_P(200, "text/plain", "OK");

    });
       
    // commit settings to file. TODO: we want to get the json from post body
    server.on("/saveSettings", HTTP_POST, [] (AsyncWebServerRequest *request) {
    
         //TODO: Pull json from body

        // GET input1 value on <ESP_IP>/update?relay=<inputMessage>
        if (request->hasParam("json") ) { 
            // save this json to disk (simple)
            // TODO: could break out the save() to a separate function but probably overkill
        } else {
            Serial.println("Did not receive json to save");
        }

        //This method receives as first input the HTTP response code, which will be 200 in our case.  This is the HTTP response code for “OK”.
        // NOTE: we can update this to return 200 ok vs 300 fail later 
        request->send_P(200, "text/plain", "OK");
    });

    // Kick off async server - it'll run in the backround listening for requests 
    server.begin();
}

// This method is invoked by the microcontroller
void loop(){ }

// get temp, humidity, etc and return as json 
String getSettings() {
    // Read temperature as Celsius (the default)
    // Read temperature as Fahrenheit (isFahrenheit = true)
    //float t = dht.readTemperature(true);
    // Check if any reads failed and exit early (to try again).

    float t = dht.readTemperature();
    if (isnan(t)) {    
        Serial.println("Failed to read from DHT sensor!");
    }
    String temp = String(t);

    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float h = dht.readHumidity();
    if (isnan(h)) {
        Serial.println("Failed to read from DHT sensor!");
    }
    String humidity = String(h);

    //Function to print full Date and local time
    //this function was found here https://arduino.stackexchange.com/questions/52676/how-do-you-convert-a-formatted-print-statement-into-a-string-variable
    time_t rawtime;
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    char timeStringBuff[50]; //50 chars should be enough
    strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    //print like "const char*"
    //Serial.println(timeStringBuff);
    String longTime = String timeAsAString(timeStringBuff);

    strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
    String shortTime = String timeAsAString(timeStringBuff);

    //Construct to create the String object 
    String timeAsAString(timeStringBuff);

    //TODO: include 'zone state' in this json (maybe everything goes in one main json) 
    //TODO: plug in real values
    String s = "{ \"temp\": 55, \"humidity\": 68, \"time\": \"20200101 10:20:00\" }";
}

//TODO: get rid of this function - do this in js / json
/*
String processor(const String& var){
    //Serial.println(var);
    if(var == "BUTTONPLACEHOLDER"){
        String buttons ="";
        for(int i=1; i<=zones; i++){
            String zoneStateValue = zoneState(i);
            //Here parts of the HTML will be parsed to index.html like Relay # followed by its value in variable for the GPIO numbers
            buttons+= "<span class=\"w3-hide-small\"><h4>Zone " + String(i) 
                + "</h4><h4>Valve (relay) #" + String(i) + " - GPIO " 
                + relayGPIOs[i-1] + "</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" 
                + String(i) + "\" " 
                + zoneStateValue +
                "><span class=\"slider\"></span></label></span>";
        }
        return buttons;
    }
    else if(var == "TIMERVALUE"){
        return timerSliderValue;
    }
    else if(var == "FARMTIMENOW"){
        return printShortFarmTime();
    }
    else if(var == "TEMPERATURE"){
        return readDHTTemperature();
    }
    else if(var == "HUMIDITY"){
        return readDHTHumidity();
    }
    return String();
}
*/

//Function to set zone State (relay pin State)
//TODO: return this with the stats data
String zoneState(int valveRelayNum){
    if(relayNO){
        if(digitalRead(relayGPIOs[valveRelayNum-1])){
            return "";
        }
        else {
            return "checked";
        }
    }
    else {
        if(digitalRead(relayGPIOs[valveRelayNum-1])){
            return "checked";
        }
        else {
            return "";
        }
    }
    return "";
}

//I did a minor change so instead of a void function it now returns a string to be used to show time in the webinterface
//Modified print local time function
void modifiedPrintLocalTime()
    // Function based on post in the https://forum.arduino.cc/index.php?topic=536464.0 Arduino Forum by user Abhay
{ 
    int OnlyYear;
    int onlyMonth;
    int onlyDay;
    int onlyHour;
    int onlyMin;
    int onlySec;

    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return;
    }
    //Serial.println(&timeinfo, "%m %d %Y / %H:%M:%S");
    //scanf(&timeinfo, "%m %d %Y / %H:%M:%S")
    onlyHour = timeinfo.tm_hour;
    onlyMin  = timeinfo.tm_min;
    onlySec  = timeinfo.tm_sec;
    onlyDay = timeinfo.tm_mday;
    onlyMonth = timeinfo.tm_mon + 1;
    OnlyYear = timeinfo.tm_year +1900;

    //test
    Serial.print("Print only Hour and Minutes...");
    Serial.print(onlyHour);
    Serial.print(":");
    Serial.print(onlyMin);
}

// Set all relays to off when the program starts - if set to Normally Open (NO), the relay is off when you set the relay to HIGH
void turnRelaysToOff(){
    for(int i=1; i<=zones; i++){
        pinMode(relayGPIOs[i-1], OUTPUT);
        if(relayNO){
            digitalWrite(relayGPIOs[i-1], HIGH);
        }
        else{
            digitalWrite(relayGPIOs[i-1], LOW);
        }
    }
}

//Function to Start/Stop irrigation Zones
int zoneStartStop (int zone, int startStop){
    int waitTime = 1000;
    if (startStop == 1){
        digitalWrite(relayGPIOs[zone], startStop);
        Serial.print("*** Zone Valve");
        Serial.print(zone);
        Serial.println("ON ***");
        delay(waitTime);
        digitalWrite(pumpGpio, startStop);
        Serial.print("*** Pump turned ON ***");
    } else{
        digitalWrite(relayGPIOs[zone], startStop);
        Serial.print("*** Zone Valve");
        Serial.print(zone);
        Serial.println("OFF ***");
        delay(waitTime);
        digitalWrite(pumpGpio, startStop);
        Serial.print("*** Pump turned ON ***");
    }
}

//Function to initialize the Wifi
void getWifiGoing(){
    /*
    // Connect the ESP to the Wi-Fi using the credentials entered before
    //with WiFi.mode(WIFI_STA) besides the wifi client we will have a access point
    WiFi.mode(WIFI_AP_STA);// looks like this is not really needed, I need to investigate better how wifi works.
    //So far the behaviour is that it creates a soft access point and also connect to the network thru access point 
    //Here is how to start the soft access point :  WiFi.softAP("softap", "imakestuff");
    //This part of the code was taken from https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html
    Serial.print("Setting soft-AP ... ");
    boolean result = WiFi.softAP("softap", "imakestuff");
    if(result == true)
    {
    Serial.println("Soft Access Point Started");
    IPAddress mySoftIP = WiFi.softAPIP();
    Serial.print("Soft Acess Point IP address: ");
    Serial.println(mySoftIP);
    }
    else
    {
    Serial.println("Soft Access Point Failed!");
    }
     */
    WiFi.softAP("softap", "imakestuff");
    IPAddress IP = WiFi.softAPIP();

    //here  start wifi sessions as a client.
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi..");
    }

    // Print ESP32 Local IP Address
    Serial.print("The Fabfarm Irrigation system network IP is:");
    Serial.println(WiFi.localIP());
    //Serial.print("The gateway IP is:")
    //Serial.println(WiFi.gatewayIP());

}

//Function to initialize Spiffs
void getSpiffGoing(){
    // Initialize SPIFFS 
    //That is the file system.PARAM_INPUT_1
    if(!SPIFFS.begin(true)){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
}

