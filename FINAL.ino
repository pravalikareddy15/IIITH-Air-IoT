#include <ThingSpeak.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_AHT10.h>
#include <RTClib.h>
#include <SD.h>
#include <Wire.h>
#include <SPI.h>

// WiFi Configuration
const char* WIFI_SSID = "Jagan";
const char* WIFI_PASSWORD = "123456789";

// ThingSpeak Configuration
const long CHANNEL_ID = 2386661;
const char* CHANNEL_API_KEY = "B0XLN4A65AQ74BYP"; 

// Variables for PM sensor
int pm25 = 0;
int pm10 = 0;
byte command_frame[9] = {0xAA, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x67, 0xBB};
byte received_data[9];
unsigned long prev_time = 0;
int sum = 0;

// Sensor and Data Variables
Adafruit_AHT10 aht;
Adafruit_Sensor* aht_humidity;
Adafruit_Sensor* aht_temp;
float temp;
float humd;
RTC_DS3231 rtc;
String rtc_date_time;

// SD Card
File dataFile;
const char* SD_FILENAME = "/Node_2/Node_2.csv"; // Changed filename for node 2

// CO2 Sensor Configuration
const int CO2_PIN = 13;  // Pin connected to the CO2 sensor
unsigned long th, tl;    // Variables for timing
int ppm;                 // Variable to store CO2 concentration in ppm

// WiFi Client and Timing Variables
WiFiClient client;
unsigned long previousMillis = 0;
const unsigned long RECONNECT_INTERVAL = 300000;
const unsigned long DELAY_INTERVAL = 1000;
unsigned long previousMillisSD = 0;
unsigned long previousMillisTS = 0;
const unsigned long SD_INTERVAL = 1000;
const unsigned long TS_INTERVAL = 20000;

void setup() {
  Serial.begin(9600);
  initWiFi();
  Wire.begin();
  pm_setup();
  aht_setup();
  rtc_setup();
  sd_setup();
  ThingSpeak.begin(client);
  pinMode(CO2_PIN, INPUT);
  Serial.println("Starting ESP32...");
  delay(10000);
}

void loop() {
  unsigned long currentMillis = millis();

  // Handle WiFi reconnection
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= RECONNECT_INTERVAL)) {
    reconnectWiFi();
    previousMillis = currentMillis;
  }

  // Save data to SD every second
  if (currentMillis - previousMillisSD >= SD_INTERVAL) {
    previousMillisSD = currentMillis;
    pm_loop();
    aht_loop();
    co2_loop();
    rtc_loop();
    sd_final();
  }

  // Update ThingSpeak every 20 seconds
  if (currentMillis - previousMillisTS >= TS_INTERVAL) {
    previousMillisTS = currentMillis;
    ThingSpeak.writeFields(CHANNEL_ID, CHANNEL_API_KEY);
    Serial.println("*****************Update ThingSpeak channel*****************");
  }
}

// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to WiFi");
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
    Serial.print('.');
    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.println("IP Address: " + WiFi.localIP().toString());
    Serial.println("Signal strength (RSSI): " + String(WiFi.RSSI()));
  } else {
    Serial.println("Failed to connect to WiFi. Restarting...");
    ESP.restart();
  }
}

// Reconnect WiFi
void reconnectWiFi() {
  Serial.println("Reconnecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(DELAY_INTERVAL);
  }
  Serial.println(" WiFi reconnected");
}

// PM Sensor Setup
void pm_setup() {
  sendCommand(0x01);
  Serial.println("PM Found!");
}

void pm_loop() {
  if (millis() - prev_time > 5000) {
    sendCommand(0x02);
    prev_time = millis();
  }
  if (Serial.available()) {
    Serial.readBytes(received_data, 9);
    if (checksum()) {
      calculatePM();
    }
  }
}

void sendCommand(byte command) {
  command_frame[1] = command;
  sum = command_frame[0] + command_frame[1] + command_frame[2] + command_frame[3] + command_frame[4] + command_frame[5] + command_frame[8];
  int rem = sum % 256;
  command_frame[6] = (sum - rem) / 256;
  command_frame[7] = rem;
  delay(1000);
  Serial.write(command_frame, 9);
}

bool checksum() {
  sum = 0;
  for (int i = 0; i < 6; i++) sum += int(received_data[i]);
  sum += int(received_data[8]);
  return sum == ((int(received_data[6]) * 256) + int(received_data[7]));
}

void calculatePM() {
  pm25 = int(received_data[4]) * 256 + int(received_data[5]);
  pm10 = int(received_data[2]) * 256 + int(received_data[3]);
  Serial.println("PM2.5: " + String(pm25) + " PM10: " + String(pm10));
  
  ThingSpeak.setField(3, pm25);
  ThingSpeak.setField(4, pm10);
}

// AHT Sensor Setup
void aht_setup() {
  if (!aht.begin()) {
    Serial.println("Failed to find AHT10 chip");
    while (1) delay(10);
  }
  Serial.println("AHT10 Found!");
  aht_temp = aht.getTemperatureSensor();
  aht_humidity = aht.getHumiditySensor();
}

void aht_loop() {
  sensors_event_t humidity_event, temp_event;
  aht.getEvent(&humidity_event, &temp_event);
  
  temp = temp_event.temperature;
  humd = humidity_event.relative_humidity;
  
  Serial.println("Temperature: " + String(temp) + "C, Humidity: " + String(humd) + "%");
  ThingSpeak.setField(1, temp);
  ThingSpeak.setField(2, humd);
}

// CO2 Sensor Monitoring
void co2_loop() {
  th = pulseIn(CO2_PIN, HIGH, 2008000) / 1000;  // Time high in milliseconds
  tl = 1004 - th;  // Time low in milliseconds
  ppm = 2000 * (th - 2) / (th + tl - 4);  // Calculate ppm

  Serial.println("CO2 Concentration: " + String(ppm) + " ppm");
  ThingSpeak.setField(6, ppm);
}

// RTC Setup
void rtc_setup() {
  if (!rtc.begin() || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println("RTC Found!");
}

void rtc_loop() {
  DateTime now = rtc.now();
  rtc_date_time = String(now.day()) + "-" + String(now.month()) + "-" + String(now.year()) + " " + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
  Serial.println("RTC Time: " + rtc_date_time);
}

// SD Card Setup
void sd_setup() {
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  if (!SD.exists(SD_FILENAME)) {
    dataFile = SD.open(SD_FILENAME, FILE_WRITE);
    if (dataFile) {
      dataFile.println("created_at,Temperature,Humidity,PM2.5,PM10,CO2");
      dataFile.close();
      Serial.println("CSV file created successfully!");
    } else {
      Serial.println("Error creating CSV file");
    }
  } else {
    Serial.println("File exists, Appending to existing file");
  }
}

// SD Card Final Write
void sd_final() {
  dataFile = SD.open(SD_FILENAME, FILE_APPEND);
  if (dataFile) {
    dataFile.print(rtc_date_time + ",");
    dataFile.print(String(temp) + ",");
    dataFile.print(String(humd) + ",");
    dataFile.print(String(pm25) + ",");
    dataFile.print(String(pm10) + ",");
    dataFile.print(String(ppm) + "\n");
    dataFile.close();
    Serial.println("Data written to SD card successfully!");
  } else {
    Serial.println("Error updating SD card");
  }
}
