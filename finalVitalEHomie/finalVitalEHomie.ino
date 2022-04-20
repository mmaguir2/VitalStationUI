/* 
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-web-server-websocket-sliders/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include "ESPAsyncWebServer.h"
#include <esp_task_wdt.h>
#include <Wire.h> // library for I2C communication

#include <Adafruit_MLX90614.h>  // library made for the temperature sensor

#include "algorithm_by_RF.h"
#include "max30102.h"
#include "algorithm.h" 


#define SDA_temp 33//sda line for second i2c bus
#define SCL_temp 32//scl line for second i2c bus

//--POX--
// Interrupt pin
const byte oxiInt = 27; // pin connected to MAX30102 INT

uint32_t aun_ir_buffer[BUFFER_SIZE]; //infrared LED sensor data
uint32_t aun_red_buffer[BUFFER_SIZE];  //red LED sensor data
  float n_spo2_maxim;  //SPO2 value
  int8_t ch_spo2_valid_maxim;  //indicator to show if the SPO2 calculation is valid
  int32_t n_heart_rate_maxim; //heart rate value
  int8_t  ch_hr_valid_maxim;  //indicator to show if the heart rate calculation is valid
 
//--temp--
Adafruit_MLX90614 mlx = Adafruit_MLX90614();  // object

// Replace with your network credentials
const char* ssid = "Weefee";
const char* password = "mediumbonus189";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object
AsyncWebSocket ws("/ws");

String message = "";
String SPO2Data = "";
String PRbpm = "";
String temperature = "";
String tempOrSPO2 = "";

//Json Variable to Hold sensor data Values
JSONVar dataValues;

//Get Data
//creates JSON string with current slider values
//These are the values that are being sent to the website. They are all strings.
String getData()
{
  dataValues["SPO2Data"] = String(SPO2Data);
  dataValues["PRbpm"] = String(PRbpm);
  dataValues["temperature"] = String(temperature);
  dataValues["tempOrSPO2"] = String(tempOrSPO2);
  String jsonString = JSON.stringify(dataValues);
  return jsonString;
}

// Initialize SPIFFS
void initFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
   Serial.println("SPIFFS mounted successfully");
  }
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}
//send webpage sensor data
//call notifyClients when sensor is done retreiving data
void notifyClients(String dataValues) {
  ws.textAll(dataValues);
}

//what to do with "getTemp" or "getSPO2" message
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
  {
    data[len] = 0;
    //strcmp compares strings, 0 means strings are equal
    if (strcmp((char*)data, "getTemp") == 0) 
    {
      Serial.println("getTemp");
      tempOrSPO2 = "temp";
      temperature = String(mlx.readObjectTempF(), 2);//converting temp value (double) to a string and storing in "temperature"
      Serial.println(getData());//display on console for debugging
      notifyClients(getData());//send data to web
    }
    if (strcmp((char*)data, "getSPO2") == 0) 
    {
      Serial.println("getSPO2");
      tempOrSPO2 = "SPO2";
      SPO2Data = "...";
      PRbpm = "...";
      notifyClients(getData());
      
      tempOrSPO2 = "SPO2";
      checkIfValidPOX();
      SPO2Data = String(n_spo2_maxim); //SPO2 value to send to web
      //PRbpm = "60";
      PRbpm = String(n_heart_rate_maxim, DEC);
      Serial.println(getData());//display on console for debugging
      notifyClients(getData());//send data to web
      Serial.println("Sent Data to Web");
    }
  }
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}
void setupTemp() {
  uint32_t freq = 100000;
  Wire1.begin(SDA_temp, SCL_temp, freq);
  mlx.begin();  // initilizing the sensor 
}
void initPOX(){
  pinMode(oxiInt, INPUT);  //pin 27 connects to the interrupt output pin of the MAX30102
  maxim_max30102_init();  //initialize the MAX30102

  }
//Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every ST seconds
void calcHRSPO2() { 
  //buffer length of BUFFER_SIZE stores ST seconds of samples running at FS sps
  //read BUFFER_SIZE samples, and determine the signal range
  for(int32_t i=0;i<BUFFER_SIZE;i++)
  {
    while(digitalRead(oxiInt)==1);  //wait until the interrupt pin asserts
    maxim_max30102_read_fifo((aun_red_buffer+i), (aun_ir_buffer+i));  //read from MAX30102 FIFO
  }
   //calculate heart rate and SpO2 after BUFFER_SIZE samples (ST seconds of samples) using MAXIM's method
   maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, BUFFER_SIZE, aun_red_buffer, &n_spo2_maxim, &ch_spo2_valid_maxim, &n_heart_rate_maxim, &ch_hr_valid_maxim);  
}
void checkIfValidPOX(){
  calcHRSPO2();
  //if they heart rate and SPO2 are both not valid, recalculate
    while(!(ch_hr_valid_maxim && ch_spo2_valid_maxim)){
      calcHRSPO2();
    }
    Serial.println(F("SpO2_MX\tHR_MX"));
    Serial.print(n_spo2_maxim);
    Serial.print("\t");
    Serial.print(n_heart_rate_maxim, DEC);
    Serial.print("\t");
}  
void setup() {
  Serial.begin(115200);
  esp_task_wdt_init(120, true);//make watchdog timout time 2 minutes

  initFS();
  initWiFi();
  initWebSocket();
  initPOX();
  setupTemp();//set up the temperature sensor

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });
  server.serveStatic("/", SPIFFS, "/");

  // Start server
  server.begin();

}

void loop() {
  ws.cleanupClients();
}