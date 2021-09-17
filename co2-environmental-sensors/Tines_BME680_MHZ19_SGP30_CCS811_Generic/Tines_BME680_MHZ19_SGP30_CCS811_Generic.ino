/*
**************************************************************************

Copyright Conor O'Neill 2021. conor@conoroneill.com
License Apache 2.0

This is a combination of a bunch of sample code from various libraries for the various sensors.

The output is as follows, once every 15 minutes:

SGP30 (More expensive Pimoroni sensor)
CO2: 400 ppm
TVOC: 14 ppb

BME680 (General Temperature, Pressure, Humidity sensor. Temperature at least closer to reality)
Temperature: 25.75 *C
Pressure: 1026.67 hPa
Humidity: 59.08 %
Gas: 12.85 KOhms
Approx. Altitude: -111.30 m

MHZ19 (Most expensive Chinese sensor. Suspect it's fake. Temperature also at least 10C too high
CO2: 1405 ppm
Temperature: 33 *C

CCS811 (Cheap generic Chinese sensor but seems reasonable)
CO2: 683 ppm
TVOC: 43



 **************************************************************************
 */

#include "SparkFun_SGP30_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_SGP30
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <Arduino.h>
#include "MHZ19.h"
#include "Adafruit_Conor_CCS811.h" // Conor's copy of the Adafruit library with configurable I2C pin numbers for ESP32
#include <HardwareSerial.h>
#include <TaskScheduler.h> // Click here to get the library: http://librarymanager/All#TaskScheduler


#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "MYWIFIAP";
const char* password = "password";

//Your Domain name with URL path or IP address with path
const char* serverName = "https://insert-you-tines-webhook-url-here.com";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
unsigned long timerDelay = 600000;

#define RXD2 16
#define TXD2 17

#define BAUDRATE 9600                                      // Device to MH-Z19 Serial baudrate (should not be changed)

MHZ19 myMHZ19;
int MHZ19_CO2; 
int8_t MHZ19_Temp;

HardwareSerial MySerial(1);

unsigned long getDataTimer = 0;

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme; // I2C
//Adafruit_BME680 bme(BME_CS); // hardware SPI
//Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO,  BME_SCK);

SGP30 mySGP30Sensor; //create an object of the SGP30 class

// CCS811 CO2 Module
Adafruit_Conor_CCS811 ccs;
int CCS_eCO2;
int CCS_TVOC;

// Remember WAK pin on the cheap Chinese CCS811 must be pulled to ground too!
#define CCS_ADDRESS 0x5a
#define CCS_SDA 21
#define CCS_SCL 22

void t1Callback(void);
bool t1Results = false;
//run task t1Callback every 1000ms (1s) forever
Task t1(1000, TASK_FOREVER, &t1Callback);

// Scheduled task for everything else
void t2Callback(void);
bool t2Results = false;

//run task t2Callback every 900000ms (15 mins) forever
Task t2(900000, TASK_FOREVER, &t2Callback);

Scheduler runner; //create an object of the Scheduler class


// Amazon Root CA doesn't expire until 2038. So should be safe enough to use :-)
const char* root_ca= \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n" \
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
"b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n" \
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n" \
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n" \
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n" \
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n" \
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n" \
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n" \
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n" \
"jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n" \
"AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n" \
"A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n" \
"U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n" \
"N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n" \
"o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n" \
"5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n" \
"rqXRfboQnoZsG4q5WTP468SQvvG5\n" \
"-----END CERTIFICATE-----\n";

StaticJsonDocument<300> tinesJSON;

void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println(F("Sensors for Tines"));

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
 
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");


  if (!ccs.begin(CCS_ADDRESS, CCS_SDA, CCS_SCL)) {
    Serial.println("Failed to start CCS811 sensor! Please check your wiring.");
    while(1);
  }

  // Wait for the sensor to be ready
  while(!ccs.available());
  
  Wire.begin();
  //Initialize sensor
  if (mySGP30Sensor.begin() == false) {
    Serial.println("No SGP30 Detected. Check connections.");
    while (1);
  }

  //initialize scheduler
  runner.init();
  
  //add task t1 to the schedule
  runner.addTask(t1);
  //add task t2 to the schedule
  runner.addTask(t2);

  
  //measureAirQuality should be called in one second increments after a call to initAirQuality
  mySGP30Sensor.initAirQuality();

  
  //enable t1 to run
  t1.enable();
  //enable t2 to run
  t2.enable();

  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }

  MySerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  myMHZ19.begin(MySerial);                                // *Serial(Stream) refence must be passed to library begin(). 
  myMHZ19.autoCalibration();                              // Turn auto calibration ON (OFF autoCalibration(false))

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
}



void loop() {

  runner.execute();
  
  // Only print data every 15 minutes even tho SGP30 logging every second
  if (t2Results == true) {
    Serial.println();

    Serial.println("SGP30 Data");
    Serial.print("CO2: ");
    Serial.print(mySGP30Sensor.CO2);
    tinesJSON["SGP30CO2"] = mySGP30Sensor.CO2;
     
    Serial.println(" ppm");
    Serial.print("TVOC: ");
    Serial.print(mySGP30Sensor.TVOC);
    tinesJSON["SGP30TVOC"] = mySGP30Sensor.TVOC;
    
    Serial.println(" ppb");

    Serial.println();

    Serial.println("BME680 Data");
    Serial.print("Temperature: ");
    Serial.print(bme.temperature);
    Serial.println(" *C");
    tinesJSON["BME680Temp"] = bme.temperature;
  
    Serial.print("Pressure: ");
    Serial.print(bme.pressure / 100.0);
    Serial.println(" hPa");
    tinesJSON["BME680Pressure"] = bme.pressure / 100.0;
  
    Serial.print("Humidity: ");
    Serial.print(bme.humidity);
    Serial.println(" %");
    tinesJSON["BME680Humidity"] = bme.humidity;
  
    Serial.print("Gas: ");
    Serial.print(bme.gas_resistance / 1000.0);
    Serial.println(" KOhms");
    tinesJSON["BME680Gas"] = bme.gas_resistance / 1000.0;
  
    Serial.print("Approx. Altitude: ");
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(" m");
    tinesJSON["BME680Altitude"] = bme.readAltitude(SEALEVELPRESSURE_HPA);

    Serial.println();
  
    Serial.println("MHZ19 Data");
    Serial.print("CO2: ");                      
    Serial.print(MHZ19_CO2);                                
    Serial.println(" ppm");                                
    tinesJSON["MHZ19CO2"] = MHZ19_CO2;
  
    Serial.print("Temperature: ");                  
    Serial.print(MHZ19_Temp);                               
    Serial.println(" *C");                                 
    tinesJSON["MHZ19Temp"] = MHZ19_Temp;
    
    Serial.println();
  
    Serial.println("CCS811 Data");
    Serial.print("CO2: ");
    Serial.print(CCS_eCO2);
    Serial.println(" ppm");
    tinesJSON["CCS811CO2"] = CCS_eCO2;
    
    Serial.print("TVOC: ");
    Serial.println(CCS_TVOC);
    tinesJSON["CCS811TVOC"] = CCS_TVOC;
    
    Serial.println();
    Serial.println();

    // Post on the Webhook
    postToTines();
    
    
    t1Results = false;
    t2Results = false;
  }
}

void t1Callback(void) {
  mySGP30Sensor.measureAirQuality();
  t1Results = true;
}

void t2Callback(void) {
  bme.performReading();
  MHZ19_Temp = myMHZ19.getTemperature();                     // Request Temperature (as Celsius)
  MHZ19_CO2 = myMHZ19.getCO2();                             // Request CO2 (as ppm)
  ccs.readData(); 

  CCS_eCO2 =  ccs.geteCO2();
  CCS_TVOC = ccs.getTVOC();
  
  t2Results = true;
}

void postToTines(void){

  //Check WiFi connection status
  if(WiFi.status()== WL_CONNECTED){
    HTTPClient http;
  
    // Your Domain name with URL path or IP address with path
    http.begin(serverName, root_ca);

   // Serialize the JSON
   String json;
   serializeJson(tinesJSON, json);
  
    // If you need an HTTP request with a content type: application/json, use the following:
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(json);
  
    if (httpResponseCode > 0) { //Check for the returning code
     
       String payload = http.getString();
       Serial.println(httpResponseCode);
       Serial.println(payload);
    }
     
    else {
       Serial.println("Error on HTTP request");
    }
   
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
      
    // Free resources
    http.end();
  }
}
