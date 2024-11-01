


/*
Software für ESP-01 Füllstand Zisterne zu MQTT

https://github.com/rapola/RP_HomeCAN/blob/main/Basis-Module/ESP-System/BASE-WEMOSminiD1_x92.V1/Software/WemosD1min-U1_Hackschn_V0.2/WemosD1min-U1_Hackschn_V0.2.ino


*/

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "src/ElegantOTA_RP/AsyncElegantOTA_RP.h"         		//OTA

#include "src/RP_Arduino-SerialCommand/SerialCommand.h"	  		//Serial Command

#include "src/MQTT_RP/MQTT.h"




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

const char* topic_lastWill      = "trock/offline";        		//publish tested: trock/offline Trockner1
//const char* topic_master_hb     = "node-red/C0A80103/hb"; //receive, to get to know if server and node-red is running
const char* topic_sw            = "zisterne/sw";          		//publish every 2s the Software Version
const char* topic_freq          = "zisterne/freq";        		//publish frequenz füllstand
const char* topic_err           = "zisterne/err";         		//publish error, payload is error reason

const char* topic_cmd_get_freq  = "zisterne/cmd/get_freq";  	//subscribe to get freq

String cmd_true = "on";											//if command is received, do something

/*
anpassen
uint8_t mqtt_pub_val_counter  = 0;
uint8_t max_pub_vals = 7;

bool is_connected_to_mosqitto = false;                    //true if connection to raspi mosquitto exists
uint32_t mqtt_reconnect_timeout = 30000;                  //if no mqtt connection, retry after this time 30s
uint32_t mqtt_rcv_timeout = 3000;                         //alles aus, wenn diese Zeit überschritten wird              
uint32_t mqtt_rcv_lastms = 0;
*/

char SWVERSION[] = "OTA_ESP8266-01_002";                  		//current software version 
const char* ssid = "KellerHorst";
const char* password = "BIRP7913";

//const char* SW = "Software V0.2.2";

//---------------------------------------------------------------------
// webserver
AsyncWebServer server(80);
WiFiClient net;

//####################################################################
void setup(void) {
  
  Serial.begin(115200);
  //---------------------------------------------------------------------
  // setup callbacks for serial commands
  sdata.addCommand("#SW", SEND_SWVERSION);          			//send software version
  sdata.setDefaultHandler(unrecognized);            			//handling of not matching commands, send what?
  delay(10);
  
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

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP8266, by Ralf.");
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
  
  client.begin(ip_of_mqtt_broker, net);
  client.onMessage(mqtt_messageReceived);
  mqtt_connect();
}

//####################################################################
void loop(void) {
  //call frequently to handle serial data communication
  sdata.readSerial(); 
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability
  static uint32_t last_ms;
  
  if(millis() - last_ms > 2000){
    last_ms = millis();
    MQTTpub_topic(topic_sw, SWVERSION);
  }
}


//####################################################################
//--------------------------------------------------------------------
//send software version
void SEND_SWVERSION() {
  Serial.print("SW: ");
  Serial.println (SWVERSION);
}

//-------------------------------------------------------------------
// non usable data received
void unrecognized(const char *command) {
  Serial.println("What?");
}


//####################################################################
//--------------------------------------------------------------------
//MQTT conntect, internal timeout in lib is 5s
void mqtt_connect(){
  client.setKeepAlive(mqtt_alive_time);
  client.setWill(topic_lastWill, my_mqtt_clientID);
  client.connect(my_mqtt_clientID, my_mqtt_username, my_mqtt_password);

  mqtt_subscribe(topic_cmd_get_freq);
  //mqtt_subscribe(topic_cmd_out1);
  //mqtt_subscribe(topic_master_hb);
  //mqtt_subscribe(topic_cmd_rst);
}

//-------------------------------------------
//MQTT subscribe to topic
void mqtt_subscribe(const char topic[]){
  client.subscribe(topic);
}

//-------------------------------------------
//MQTT publish topic
void MQTTpub_topic(const char topic[], String val){
  client.publish(topic, val);
}

//-------------------------------------------
//MQTT message empfangen
void mqtt_messageReceived(String &topic, String &payload){
  Serial.println("incoming: " + topic + " - " + payload);

  //if(topic == String(topic_master_hb)) mqtt_rcv_lastms = millis();

  if(topic == String(topic_cmd_get_freq)){
    Serial.println("line 169");
	//if(payload == cmd_on) out0.rcv_val = 0;                      //Relais an (ist LOW aktiv)
    //if(payload == cmd_off) out0.rcv_val = 1;                      //Relais aus (ist LOW aktiv) 
  }
/*
  if(topic == String(topic_cmd_out1)){
    if(payload == cmd_on) out1.rcv_val = 0;                      //Relais an (ist LOW aktiv)
    if(payload == cmd_off) out1.rcv_val = 1;                      //Relais aus (ist LOW aktiv) 
  }

  if(topic == String(topic_cmd_rst)){
    SenLuft.error = 0;
    SenWasser.error = 0;
    //PCA_err = 0;
  }
*/ 
  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}