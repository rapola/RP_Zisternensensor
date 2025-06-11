/*
Software für ESP-01 Füllstand Zisterne zu MQTT
Arduino 1.8.19
https://github.com/rapola/RP_Zisternensensor/blob/main/ESP_01_MQTT/Software

- mqtt beispiel
https://github.com/rapola/RP_HomeCAN/blob/main/Basis-Module/ESP-System/BASE-WEMOSminiD1_x92.V1/Software/WemosD1min-U1_Hackschn_V0.2/WemosD1min-U1_Hackschn_V0.2.ino
- publish Softwarversion every 2s
- listens to command "zisterne/cmd/get_freq", if payload "true" is received, send serial response to sensor (mega8) and wait for answer
- Serial answer from mega8 will be "ICP1val: xx123xx" (value or timeout message)
- publish "ICP1val:" on MQTT, topic "zisterne/freq"
- corresponding software: "Mega8", always use same Vx.x !!
*/

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "src/ElegantOTA_RP/AsyncElegantOTA_RP.h"         		//OTA

#include "src/RP_Arduino-SerialCommand/SerialCommand.h"	  		//Serial Command

#include "src/MQTT_RP/MQTT.h"

//-------------------------------------------------------
//global variables / constants
char SWVERSION[] = "ESP8266-V1.0";                  		      //current software version 
//const char* ssid = "KellerHorst";
//const char* password = "BIRP7913";

const char* ssid = "Taschner_HausNordOst";
const char* password = "birp7913-14042020";

const uint8_t Rdy_Pin        = 0;                             //GPIO0; set low to enable Mega8 connection

//---------------------------------------------------------------------
// create serial command object
SerialCommand sdata;

//---------------------------------------------------------------------
// MQTT
MQTTClient client;

const char* ip_of_mqtt_broker = "192.168.0.30";           		//mqtt borker ip, change to desired ip
const char* my_mqtt_clientID = "zisterne1";               		//unique, do not use name a second time in mqtt-mesh
const char* my_mqtt_username = "zisterne";                		//configured user in raspi mosquitto credentials
const char* my_mqtt_password = "zisterne";                		//configured password in raspi mosquitto credentials
const int mqtt_alive_time = 2;

const char* topic_lastWill      = "zisterne/offline";         //publish tested
const char* topic_swOwn         = "zisterne/swown";           //publish every 2s the Software Version
const char* topic_freq          = "zisterne/freq";        		//publish frequenz füllstand oder error
const char* topic_swOf_Mega8    = "zisterne/swsen";           //publish Software Version of Sensor (mega8)
const char* topic_cmd_get_freq  = "zisterne/cmd/get_freq";  	//subscribe to get freq
const char* topic_cmd_get_senSW = "zisterne/cmd/senSW";       //subscribe to get softwareversion of mega8

String cmd_true = "true";											                //if command is received, do something

//---------------------------------------------------------------------
// webserver
AsyncWebServer server(80);
WiFiClient net;

//####################################################################
void setup(void) {
  //-------------------------------------------------------
  // Pins
  pinMode(Rdy_Pin, OUTPUT);  
  
  Serial.begin(115200);
  //---------------------------------------------------------------------
  // setup callbacks for serial commands
  sdata.addCommand("#SW", SEND_SWVERSION);          			    //send software version
  sdata.addCommand("ICP1val:", READ_Serial_ICP1val);          //handle incomeing ICP1 values
  sdata.addCommand("ICP1err:", READ_Serial_ICP1err);          //handle incomeing ICP1 error
  sdata.addCommand("SW_Vers:", READ_Serial_SW);          		  //handle incomeing Softwareversion
  sdata.setDefaultHandler(unrecognized);            			    //handling of not matching commands, send what?
  delay(10);

  //---------------------------------------------------------------------
  // setup wifi
  IPAddress ip(192, 168, 0, 35);
  IPAddress dns(192, 168, 0, 1);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 254, 0);
  
  WiFi.config(ip, dns, gateway, subnet);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //---------------------------------------------------------------------
  // setup OTA
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP8266, by Ralf.");
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
  
  //---------------------------------------------------------------------
  // setup MQTT  
  client.begin(ip_of_mqtt_broker, net);
  client.onMessage(mqtt_messageReceived);
  mqtt_connect();
}

//####################################################################
void loop(void) {
  digitalWrite(Rdy_Pin, LOW);                                 //Enable Mega8
  
  //---------------------------------------------------------------------
  //call frequently to handle serial data communication
  sdata.readSerial(); 
  client.loop();    //MQTT
  delay(10);  // <- fixes some issues with WiFi stability
  
  if (!client.connected()) {
    mqtt_connect();
  }
  
  //---------------------------------------------------------------------
  // MQTT publish softwareversion
  static uint32_t last_ms;
  if(millis() - last_ms > 2000){
    last_ms = millis();
    MQTTpub_topic(topic_swOwn, SWVERSION);
  }

}

//### Serial #########################################################
//--------------------------------------------------------------------
//send software version
void SEND_SWVERSION() {
  Serial.printf ("SW: %s \r\n", SWVERSION);
}

//--------------------------------------------------------------------
//handle incomeing ICP1val, publish MQTT
void READ_Serial_ICP1val(){
  int hVal = 0;
  int lVal = 0;
  uint32_t Val = 0;
  char *arg;

  arg = sdata.next();
  if (arg != NULL) {
    hVal = atoi(arg);                                         // Converts a char string to an integer
  }

  arg = sdata.next();
  if (arg != NULL) {
    lVal = atol(arg);
  }

  Val = (hVal << 16 ) + lVal;
  String stringVal = String(Val);
  MQTTpub_topic(topic_freq, stringVal);                       // MQTT publish data
}

//--------------------------------------------------------------------
//handle incomeing ICP1err, publish MQTT
void READ_Serial_ICP1err(){
  char *arg;
  arg = sdata.next();                                         // Get the next argument from the SerialCommand object buffer
  if (arg != NULL) {                                          // As long as it existed, take it
    MQTTpub_topic(topic_freq, arg);                           // MQTT publish data
  }
}

//--------------------------------------------------------------------
//handle incomeing softwareversion, publish MQTT
void READ_Serial_SW(){
  char *arg;
  arg = sdata.next();                                         // Get the next argument from the SerialCommand object buffer
  if (arg != NULL) {                                          // As long as it existed, take it
    //Serial.printf("176 %s \r\n", arg);
    MQTTpub_topic(topic_swOf_Mega8, arg);                     // MQTT publish data
  } 
}

//-------------------------------------------------------------------
// non usable data received
void unrecognized(const char *command) {
  //Serial.println("What?");                                  //debug only
}

//### MQTT ###########################################################
//--------------------------------------------------------------------
//MQTT conntect, internal timeout in lib is 5s
void mqtt_connect(){
  client.setKeepAlive(mqtt_alive_time);
  client.setWill(topic_lastWill, my_mqtt_clientID);
  client.connect(my_mqtt_clientID, my_mqtt_username, my_mqtt_password);

  mqtt_subscribe(topic_cmd_get_freq);
  mqtt_subscribe(topic_cmd_get_senSW);
}

//--------------------------------------------------------------------
//MQTT subscribe to topic
void mqtt_subscribe(const char topic[]){
  client.subscribe(topic);
}

//--------------------------------------------------------------------
//MQTT publish topic
void MQTTpub_topic(const char topic[], String val){
  client.publish(topic, val);
}

//-------------------------------------------
//MQTT message empfangen
void mqtt_messageReceived(String &topic, String &payload){
  //Serial.println("incoming: " + topic + " - " + payload);   //uncomment for debug only!   
  if(topic == String(topic_cmd_get_freq)){
    if(payload == cmd_true){
      Serial.printf("GETF\r\n");                              //send serial command to get the value
    }      
  }
  else if(topic == String(topic_cmd_get_senSW)){
    if(payload == cmd_true){
      Serial.printf("#SW\r\n");                               //send serial command to get the softwareversion
    }      
  }
}
