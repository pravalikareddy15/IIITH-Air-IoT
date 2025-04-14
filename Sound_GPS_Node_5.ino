#include <ThingSpeak.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi Configuration
const char* WIFI_SSID = "JioFi3_259D0E";
const char* WIFI_PASSWORD = "k61m12m0u5";

// ThingSpeak Configuration
// const long CHANNEL_ID = 1424384;  // 1
// const char* CHANNEL_API_KEY = "9KYYQDUOQB6CTTE4";

const long CHANNEL_ID = 1662300;//2
const char* CHANNEL_API_KEY = "LJ3VIGQXLOUX6K2T";

int pm25 = 0;
int pm10 = 0;
byte command_frame[9] = { 0xAA, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x67, 0xBB };
byte received_data[9];
unsigned long prev_time = 0;
int sum = 0;

#include <Adafruit_AHT10.h>
Adafruit_AHT10 aht;
Adafruit_Sensor* aht_humidity;
Adafruit_Sensor* aht_temp;
float temp;
float humd;


#define SoundSensorPin 36
#define VREF 5.0
float voltageValue, dbValue;

// CO2 Sensor Configuration
const int CO2_PIN = 13;
unsigned long duration, th, tl;
int ppm;

#include <RTClib.h>
RTC_DS3231 rtc;
String rtc_date_time;

#include <SD.h>
#include <Wire.h>
#include <SPI.h>
File dataFile;
// const char* SD_FILENAME = "/Node_1/Node_1.csv";//1
const char* SD_FILENAME = "/Node_2/Node_2.csv";//2

WiFiClient client;

unsigned long previousMillis = 0;
const unsigned long RECONNECT_INTERVAL = 300000;
const unsigned long DELAY_INTERVAL = 1000;


unsigned long previousMillisSD = 0;       // Variable to track previous SD card save time
unsigned long previousMillisTS = 0;       // Variable to track previous ThingSpeak update time
const unsigned long SD_INTERVAL = 1000;   // Save data to SD every second
const unsigned long TS_INTERVAL = 20000;  // Update ThingSpeak every 20 seconds



void setup() {
  Serial.begin(9600);
  initWiFi();
  Wire.begin();
  pm_setup();
  aht_setup();
  rtc_setup();
  sd_setup();
  ThingSpeak.begin(client);
  Serial.println("Starting ESP32...");
  delay(10000);
}

void loop() {

  unsigned long currentMillis = millis();  // Get current time in milliseconds

  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= RECONNECT_INTERVAL)) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(DELAY_INTERVAL);
    }
    Serial.println(" WiFi reconnected");
    previousMillis = currentMillis;
  }

  // Save data to SD every second
  if (currentMillis - previousMillisSD >= SD_INTERVAL) {
    previousMillisSD = currentMillis;  // Update previous SD save time
    pm_loop();
    aht_loop();
    sound_loop();
    rtc_loop();
    sd_final();
  }

  // Update ThingSpeak every 20 seconds
  if (currentMillis - previousMillisTS >= TS_INTERVAL) {
    previousMillisTS = currentMillis;                     // Update previous ThingSpeak update time
    ThingSpeak.writeFields(CHANNEL_ID, CHANNEL_API_KEY);  // Update ThingSpeak channel
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
    // Serial.println("\nConnected to WiFi");
    Serial.println("\nConnected to " + String(WIFI_SSID) + " network");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Mac Address: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Signal strength (RSSI): ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.print("Mac Address: ");
    Serial.println(WiFi.macAddress());
    Serial.println("\nFailed to connect to WiFi within 30 seconds. Restarting...");
    ESP.restart();
  }
}

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
  sum = int(received_data[0]) + int(received_data[1]) + int(received_data[2]) + int(received_data[3]) + int(received_data[4]) + int(received_data[5]) + int(received_data[8]);
  return sum == ((int(received_data[6]) * 256) + int(received_data[7]));
}

void calculatePM() {
  pm25 = int(received_data[4]) * 256 + int(received_data[5]);
  delay(2000);
  pm10 = int(received_data[2]) * 256 + int(received_data[3]);
  Serial.print("pm25.5: ");
  Serial.println(pm25);
  Serial.print("PM10: ");
  Serial.println(pm10);

  ThingSpeak.setField(3, pm25);
  ThingSpeak.setField(4, pm10);

  dataFile = SD.open(SD_FILENAME, FILE_APPEND);
  if (dataFile) {
    dataFile.close();
  } else {
    Serial.println("-------------- Error updating pm25.5, PM10 ----------------");
  }
}

void aht_setup() {
  if (!aht.begin()) {
    Serial.println("Failed to find AHT10 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("AHT10 Found!");
  aht_temp = aht.getTemperatureSensor();
  aht_humidity = aht.getHumiditySensor();
}

void aht_loop() {
  sensors_event_t humidity_event, temp_event;
  aht.getEvent(&humidity_event, &temp_event);  // populate temp_event and humidity_event with fresh data

  temp = temp_event.temperature;            // Extract the temperature from temp_event
  humd = humidity_event.relative_humidity;  // Extract the humidity from humidity_event

  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.println(" degrees C");
  Serial.print("Humidity: ");
  Serial.print(humd);
  Serial.println("% rH");
  ThingSpeak.setField(1, temp);
  ThingSpeak.setField(2, humd);
  dataFile = SD.open(SD_FILENAME, FILE_APPEND);
  if (dataFile) {
    dataFile.close();
  } else {
    Serial.println("-------------- Error updating AHT data ----------------");
  }
}

void sound_loop() {
  voltageValue = analogRead(SoundSensorPin) / 4096.0 * VREF;
  dbValue = voltageValue * 50.0;
  Serial.print(dbValue, 1);
  Serial.println(" dBA");
  ThingSpeak.setField(5, dbValue);
  dataFile = SD.open(SD_FILENAME, FILE_APPEND);
  if (dataFile) {
    dataFile.close();
  } else {
    Serial.println("-------------- Error updating Noise data ----------------");
  }
}

void co2_loop() {
  th = pulseIn(CO2_PIN, HIGH, 2008000) / 1000;
  tl = 1004 - th;
  ppm = 2000 * (th - 2) / (th + tl - 4);
  Serial.print("CO2 Concentration: ");
  Serial.println(ppm);
  ThingSpeak.setField(6, ppm);

  dataFile = SD.open(SD_FILENAME, FILE_APPEND);
  if (dataFile) {
    dataFile.close();
  } else {
    Serial.println("-------------- Error updating Co2 levels ----------------");
  }
}
void rtc_setup() {
  if (!rtc.begin() || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println("RTC Found!");
}

void rtc_loop() {
  Serial.print("RTC Time: ");
  DateTime now = rtc.now();
  rtc_date_time = String(now.day(), DEC) + "-" + String(now.month(), DEC) + "-" + String(now.year(), DEC) + " " + String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC);
  Serial.println(rtc_date_time);
  dataFile = SD.open(SD_FILENAME, FILE_APPEND);
  if (dataFile) {
    dataFile.close();
  } else {
    Serial.println("-------------- Error updating RTC data ----------------");
  }
}

void sd_setup() {
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  if (!SD.exists(SD_FILENAME)) {
    dataFile = SD.open(SD_FILENAME, FILE_WRITE);
    if (dataFile) {
      dataFile.println("created_at,Temperature,Humidity,PM2.5,PM10,dbValue, Co2");
      dataFile.close();
      Serial.println("CSV file created successfully!");
    } else {
      Serial.println();
      Serial.println("Error creating CSV file");
    }
  } else {
    Serial.println();
    Serial.println("File exists, Appending to existing file");
  }
}

void sd_final() {
  dataFile = SD.open(SD_FILENAME, FILE_APPEND);
  if (dataFile) {
    dataFile.print(rtc_date_time);
    dataFile.print(",");
    Serial.println("Timestamp data updated successfully!");
    dataFile.print(temp);
    dataFile.print(",");
    dataFile.print(humd);
    dataFile.print(",");
    Serial.println("---------- AHT updated successfully! -------------");
    dataFile.print(pm25);
    dataFile.print(",");
    dataFile.print(pm10);
    dataFile.print(",");
    Serial.println("---------- PM updated successfully! -------------");
    dataFile.print(dbValue);
    dataFile.print(",");
    Serial.println("---------- Noise updated successfully! -------------");
    dataFile.print(ppm);
    dataFile.print(",");
    Serial.println("---------- CO2 updated successfully! -------------");
    dataFile.println("");
    Serial.println("---------- Updated successfully! -------------");
    dataFile.close();
  } else {
    Serial.println("-------------- Error updating ----------------");
  }
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
}
