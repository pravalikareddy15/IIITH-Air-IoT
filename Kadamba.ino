#include <HardwareSerial.h>
#include "SdsDustSensor.h"
#include <WiFi.h>
#include "ThingSpeak.h"

// Wi-Fi credentials
#define SSID "network"
#define PASSWORD "123654789"


// ThingSpeak credentials
unsigned long myChannelNumber = 2634864;
const char* myWriteAPIKey = "LU9XOREDSJ5FFBS8";

// Define SDS011 pins
#define SDS_RX_PIN 16 // RX pin of ESP32 connected to TX of SDS011
#define SDS_TX_PIN 17  // TX pin of ESP32 connected to RX of SDS011

// Create HardwareSerial instance for SDS011 (using UART2)
HardwareSerial sdsSerial(1);  // UART2
SdsDustSensor sds(sdsSerial);
float y_1;
float y_2;
float m1= 1.44679902;
float c1= -1.794950591328643;
float m2= 1.31303841;
float c2= 4.398478766578194;
// Wi-Fi client
WiFiClient client;

// Variables for storing PM2.5 and PM10 values
float pm25, pm10;

void setup() {
  // Start serial communication for debugging
  Serial.begin(115200);

  // Start hardware serial for SDS011
  sdsSerial.begin(9600, SERIAL_8N1, SDS_RX_PIN, SDS_TX_PIN);

  // Initialize SDS011 sensor
  sds.begin();
  Serial.println("SDS011 sensor initialized.");

  // Connect to Wi-Fi
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize ThingSpeak
  ThingSpeak.begin(client);
}

void loop() {
  // Query the SDS011 sensor for PM2.5 and PM10 values
  PmResult pm = sds.queryPm();

  if (pm.isOk()) {
    pm25 = pm.pm25;  // PM2.5 value
    pm10 = pm.pm10;  // PM10 value

    Serial.print("PM2.5: ");
    Serial.print(pm25);
    Serial.print(" µg/m³, PM10: ");
    Serial.print(pm10);
    Serial.println(" µg/m³");
    y_1=m1*pm25+c1;
    y_2=m2*pm10+c2;
    Serial.print("Calib value pm2.5: ");
    Serial.print(y_1);
    Serial.print("Calib Value pm10 :");
    Serial.print(y_2);
    // Push data to ThingSpeak
    ThingSpeak.setField(1, y_1);  // Field 1 for PM2.5
    ThingSpeak.setField(2, y_2);  // Field 2 for PM10
    ThingSpeak.setField(3, pm25);
    ThingSpeak.setField(4, pm10);


    int response = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (response == 200) {
      Serial.println("Data sent to ThingSpeak successfully.");
    } else {
      Serial.println("Problem sending data. HTTP error code: " + String(response));
    }
  } else {
    Serial.println("Failed to read from SDS011 sensor.");
  }

  delay(2000);  // Wait for 20 seconds before taking the next reading
}