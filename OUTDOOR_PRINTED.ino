#include <WiFi.h>
#include <ThingSpeak.h>  // For ThingSpeak communication
#include <SoftwareSerial.h>
#include <Adafruit_AHT10.h>  // For AHT10 sensor (Temperature and Humidity)
#include <SDS011.h>
#include <RTClib.h>  // RTC library
#include <SD.h>      // SD card library
#include <Wire.h>    // For I2C communication (used by sensors)
#include <SPI.h>     // For SPI communication (SD card usage)

// WiFi Configuration
const char* WIFI_SSID = "sahil";       // WiFi SSID
const char* WIFI_PASSWORD = "88888888";  // WiFi Password

// SDS011 Sensor Configuration
float p10, p25;
int error;
SDS011 my_sds;
#define tx2 17  // GPIO17 for TX2 on ESP32
#define rx2 3   // GPIO16 for RX2 on ESP32

// ThingSpeak Configuration
const long CHANNEL_ID = 2757106;  // Block N Aparna sarovar
const char* CHANNEL_API_KEY = "SMEIMS8E45P0P8H3";

// Sound Sensor Configuration
#define SoundSensorPin 36     // GPIO pin for sound sensor
#define VREF 3.3              // Reference voltage for sound sensor
float voltageValue, dbValue;  // Variables to store sensor readings

// AHT10 Sensor Configuration
Adafruit_AHT10 aht;
Adafruit_Sensor* aht_humidity;
Adafruit_Sensor* aht_temp;
float t, h;

// WiFi client for ThingSpeak
WiFiClient client;

// Time tracking variables
unsigned long previousMillis = 0;
const unsigned long RECONNECT_INTERVAL = 10000;  // Interval for WiFi reconnect attempts
const unsigned long DELAY_INTERVAL = 1000;       // Delay between WiFi reconnect attempts
unsigned long previousMillisTS = 0;              // Time tracking for ThingSpeak updates
const unsigned long TS_INTERVAL = 2000;          // ThingSpeak update interval

// RTC Setup
RTC_DS3231 rtc;
String rtc_date_time;

// SD Card Configuration
const char* SD_FOLDER = "/Node_2";         // Folder Name
const char* SD_FILENAME = "/Node_2/Node_2.csv";  // File path inside folder
File dataFile;

// Function to initialize WiFi connection
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to WiFi...");
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    Serial.print('.');
    delay(1000);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi within 10 seconds. Restarting...");
    ESP.restart();
  }
}

// Function to initialize the AHT10 sensor
void aht_setup() {
  if (!aht.begin()) {
    Serial.println("Failed to find AHT10 chip");
    while (1) { delay(10); }
  }
  Serial.println("AHT10 Found!");
  aht_temp = aht.getTemperatureSensor();
  aht_humidity = aht.getHumiditySensor();
}

// Function to get AHT10 sensor readings
void aht_loop() {
  sensors_event_t humidity, temp;
  aht_humidity->getEvent(&humidity);
  aht_temp->getEvent(&temp);
  t = temp.temperature;
  h = humidity.relative_humidity;
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" °C\t");
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.println("% rH");

  ThingSpeak.setField(1, t);
  ThingSpeak.setField(2, h);
}

// Function to read SDS011 sensor values
void SDS_loop() {
  error = my_sds.read(&p25, &p10);
  if (!error) {
    Serial.println("PM2.5: " + String(p25));
    Serial.println("PM10:  " + String(p10));
  }

  ThingSpeak.setField(3, p25);
  ThingSpeak.setField(4, p10);
  delay(100);
}

// Function to monitor sound levels
void noise_loop() {
  voltageValue = analogRead(SoundSensorPin) / 4096.0 * VREF;
  dbValue = voltageValue * 50.0;
  Serial.print("Noise: ");
  Serial.print(dbValue, 2);
  Serial.println(" dBA");

  ThingSpeak.setField(6, dbValue);
}

// Function to initialize ThingSpeak
void ThingSpeak_setup() {
  ThingSpeak.begin(client);
  Serial.println("ThingSpeak Initialized!");
}

// Function to initialize SD card and create folder if needed
void SD_setup() {
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  // Create folder if it doesn't exist
  if (!SD.exists(SD_FOLDER)) {
    Serial.println("Creating folder: Node_2");
    SD.mkdir(SD_FOLDER);
  }

  // Check if file exists, if not create it
  if (!SD.exists(SD_FILENAME)) {
    dataFile = SD.open(SD_FILENAME, FILE_WRITE);
    if (dataFile) {
      dataFile.println("created_at,Temperature,Humidity,PM2.5,PM10,dB");
      dataFile.close();
      Serial.println("CSV file created successfully!");
    } else {
      Serial.println("Error creating CSV file");
    }
  } else {
    Serial.println("File exists, appending data.");
  }
}

// Function to initialize RTC module
void rtc_setup() {
  if (!rtc.begin() || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println("RTC Found!");
}

// Function to read RTC timestamp
void rtc_loop() {
  Serial.print("RTC Time: ");
  DateTime now = rtc.now();
  rtc_date_time = String(now.day(), DEC) + "-" + String(now.month(), DEC) + "-" + String(now.year(), DEC) + " " + 
                  String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC);
  Serial.println(rtc_date_time);
}

// Function to save sensor data to SD card
void SD_final() {
  dataFile = SD.open(SD_FILENAME, FILE_APPEND);
  if (dataFile) {
    dataFile.print(rtc_date_time);
    dataFile.print(",");
    dataFile.print(t);
    dataFile.print(",");
    dataFile.print(h);
    dataFile.print(",");
    Serial.println("AHT10 updated successfully!");

    dataFile.print(p25);
    dataFile.print(",");
    dataFile.print(p10);
    dataFile.print(",");
    Serial.println("SDS updated successfully!");

    dataFile.print(dbValue);
    dataFile.print(",");
    Serial.println("Noise updated successfully!");

    dataFile.println("");  // Move to next line
    Serial.println("Data updated successfully!");
    dataFile.close();
  } else {
    Serial.println("Error updating SD card.");
  }

  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
}

void setup() {
  Serial.begin(9600);
  initWiFi();
  Wire.begin();
  rtc_setup();
  SD_setup();
  aht_setup();
  my_sds.begin(rx2, tx2);
  pinMode(SoundSensorPin, INPUT);
  ThingSpeak_setup();
  Serial.println("Starting ESP32...");
  delay(3000);
}

void loop() {
  unsigned long currentMillis = millis();

  // Reconnect WiFi if disconnected
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= RECONNECT_INTERVAL)) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    previousMillis = currentMillis;
  }

  aht_loop();
  rtc_loop();
  SDS_loop();
  noise_loop();
  SD_final();

  if (currentMillis - previousMillisTS >= TS_INTERVAL) {
    previousMillisTS = currentMillis;
    ThingSpeak.writeFields(CHANNEL_ID, CHANNEL_API_KEY);
    Serial.println("Updated to ThingSpeak");
  }
}
