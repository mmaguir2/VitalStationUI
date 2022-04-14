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

#include <Adafruit_MLX90614.h>  // library made for the sensor

#include "MAX30105.h"
#include "spo2_algorithm.h"

#include "heartRate.h"

#define SDA_temp 33//sda line for second i2c bus
#define SCL_temp 32//scl line for second i2c bus

MAX30105 particleSensor;

#define MAX_BRIGHTNESS 255

uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data

int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

Adafruit_MLX90614 mlx = Adafruit_MLX90614();  // object

//HR stuff---------------
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;
long irValue;
long delta;
//-----------------------

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
      runSPO2();
      SPO2Data = String(spo2); //SPO2 value to send to web
      //PRbpm = "60";
      calcHR();
      PRbpm = String(beatAvg); // PRbpm value to send to web
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
  Wire1.begin(SDA_temp, SCL_temp, 100000);
  mlx.begin();  // initilizing the sensor 
}
void initPOX(){
  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println(F("MAX30105 was not found. Please check wiring/power. Any further data is incorrect"));
  }

  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
  }

void runSPO2(){
  //esp_task_wdt_init(60, true);//make timout time 2 minutes

  bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

  //read the first 100 samples, and determine the signal range
  for (byte i = 0 ; i < bufferLength ; i++)
  {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample
  }
  
  //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  //Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
  
    //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = 25; i < 100; i++)
    {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25] = irBuffer[i];
    }
  
    //take 25 sets of samples before calculating the heart rate.
    for (byte i = 75; i < 100; i++)
    {
      while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data

      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample(); //We're finished with this sample so move to next sample
    }

    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
  Serial.println("FinalSPO2:");
  Serial.println(spo2);
}

void calcHR()
{

  for(int i = 0; i<900;i++){
  irValue = particleSensor.getIR();
  if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }
  }
  Serial.print("Avg BPM=");
  Serial.println(beatAvg);
  
}

void setup() {
  Serial.begin(115200);
  esp_task_wdt_init(120, true);//make timout time 3 minutes

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
