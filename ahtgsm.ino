#include <Wire.h>
#include <Adafruit_AHT10.h>
#include <SoftwareSerial.h>
#include <HardwareSerial.h>

// GSM and SDS011 Configuration
SoftwareSerial myGSM(4, 2);
HardwareSerial sdsSerial(2);

// SDS011 sensor values
uint16_t pm25Value, pm10Value;

// AHT10 sensor instance
Adafruit_AHT10 aht;
float temperature, humidity;

unsigned long previousMillis = 0;
const unsigned long RECONNECT_INTERVAL = 30000;

void sendDataToThingSpeak() {
  if (myGSM.available())
    Serial.write(myGSM.read());
  myGSM.println("AT");
  delay(1000);
  
  myGSM.println("AT+CPIN?");
  delay(1000);
  myGSM.println("AT+CREG?");
  delay(1000);
  myGSM.println("AT+CGATT=1");
  delay(1000);
  myGSM.println("AT+CIPSHUT");
  delay(1000);
  myGSM.println("AT+CIPSTATUS");
  delay(2000);
  ShowSerialData();
  myGSM.println("AT+CSTT=\"bsnlnet\"");  // start task and setting the APN
  delay(1000);
  ShowSerialData();
  myGSM.println("AT+CIICR");  // bring up wireless connection
  delay(3000);
  ShowSerialData();
  myGSM.println("AT+CIFSR");  // get local IP address
  delay(2000);
  ShowSerialData();
  myGSM.println("AT+CIPSPRT=0");
  delay(3000);
  ShowSerialData();
  myGSM.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"");  // start up the connection
  delay(6000);
  ShowSerialData();
  myGSM.println("AT+CIPSEND");  // begin sending data to remote server
  delay(4000);
  ShowSerialData();
  String str = "GET https://api.thingspeak.com/update?api_key=K77NDJPLOMUZHR6F&field1=" + String(pm25Value) + "&field2=" + String(pm10Value) + "&field3=" + String(temperature) + "&field4=" + String(humidity);
  Serial.println(str);
  myGSM.println(str);  // send data to remote server
  delay(4000);
  ShowSerialData();
  myGSM.println((char)26);  // sending
  delay(5000);  // waiting for a reply
  myGSM.println();
  ShowSerialData();
  myGSM.println("AT+CIPSHUT");  // close the connection
  delay(100);
  ShowSerialData();
  delay(1000);
}

void ShowSerialData() {
  while (myGSM.available() != 0)
    Serial.write(myGSM.read());
  delay(1000);
}

void sds_aqi_loop() {
  if (sdsSerial.available() >= 10) {
    uint8_t buffer[10];  // Buffer to hold the 10-byte packet
    
    // Read the 10-byte packet
    for (int i = 0; i < 10; i++) {
      buffer[i] = sdsSerial.read();
    }
    
    // Check for start characters and valid packet
    if (buffer[0] == 0xAA && buffer[1] == 0xC0 && buffer[9] == 0xAB) {
      // Extract PM2.5 and PM10 values
      pm25Value = (buffer[2] | (buffer[3] << 8));  // PM2.5 in µg/m³
      pm10Value = (buffer[4] | (buffer[5] << 8));  // PM10 in µg/m³
      
      // Print PM2.5 and PM10 values
      Serial.print("PM2.5: ");
      Serial.print(pm25Value);
      Serial.print(" µg/m³, ");
      Serial.print("PM10: ");
      Serial.print(pm10Value);
      Serial.println(" µg/m³");
    }
  }
}

void aht_loop() {
  sensors_event_t humidity_event, temp_event;
  aht.getEvent(&humidity_event, &temp_event);  // Get the temperature and humidity values

  temperature = temp_event.temperature;  // Extract temperature
  humidity = humidity_event.relative_humidity;  // Extract humidity

  // Print temperature and humidity values
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" % rH");
}

void setup() {
  Serial.begin(9600);
  Serial.println("Starting ESP32...");
  Wire.begin();

  // Initialize AHT10
  if (!aht.begin()) {
    Serial.println("Failed to find AHT10 chip");
    while (1) delay(10);
  }
  Serial.println("AHT10 Found!");

  // Initialize SDS011
  sdsSerial.begin(9600, SERIAL_8N1, 16, 17);
  
  // Initialize GSM
  delay(30000);
  myGSM.begin(9600);
  delay(1000);
  Serial.println("SIM800L Initialized");
}

void loop() {
  unsigned long currentMillis = millis();

  // Read SDS011 and AHT10 sensor data
  sds_aqi_loop();
  aht_loop();

  // Send data to ThingSpeak at intervals
  if (currentMillis - previousMillis >= RECONNECT_INTERVAL) {
    Serial.println("Sending data to ThingSpeak...");
    sendDataToThingSpeak();
    previousMillis = currentMillis;
  }
  delay(1000);
}
