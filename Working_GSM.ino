#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <SoftwareSerial.h>

// Define RX and TX pins for the GPRS module
#define GPRS_RX 26// Adjust according to your connections
#define GPRS_TX 25// Adjust according to your connections

// Use SoftwareSerial for GPRS communication
SoftwareSerial gprsSerial(GPRS_RX, GPRS_TX);

// Initialize AHT10 sensor
Adafruit_AHTX0 aht;

// Define ThingSpeak channel write API key
const String THINGSPEAK_API_KEY = "SMEIMS8E45P0P8H3"; // Replace with your API key

void setup() {
  gprsSerial.begin(9600); // Initialize GPRS serial communication
  Serial.begin(9600); // Initialize Serial Monitor

  // Initialize AHT10
  if (!aht.begin()) {
    Serial.println("Failed to initialize AHT10 sensor! Check connections.");
    while (1);
  }
  Serial.println("AHT10 sensor initialized successfully.");

  delay(1000); // Allow time for module initialization
}

void loop() {
  // Read temperature and humidity from AHT10
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  float temperature = temp.temperature;
  float humidityLevel = humidity.relative_humidity;

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(humidityLevel);
  Serial.println(" %");

  // AT commands to set up GPRS
  sendATCommand("AT");
  delay(1000); // Check if the module is responding
  sendATCommand("AT+CPIN?");
  delay(1000); // Check if SIM is ready
  sendATCommand("AT+CREG?");
  delay(2000); // Check network registration
  sendATCommand("AT+CGATT?");
  delay(2000); // Check GPRS attachment

  sendATCommand("AT+CIPSHUT");
  delay(2000); // Close any existing connection
  sendATCommand("AT+CIPSTATUS");
  delay(1000); // Check connection status
  sendATCommand("AT+CIPMUX=0");
  delay(1000); // Set single connection mode

  sendATCommand("AT+CSTT=\"bsnlnet\""); 
  delay(2000); // Set APN for BSNL
  sendATCommand("AT+CIICR");
  delay(7000); // Bring up wireless connection
  sendATCommand("AT+CIFSR");
  delay(2000); // Get local IP address

  sendATCommand("AT+CIPSPRT=0");
  delay(1000); // Disable print of extra information

  // Start TCP connection to ThingSpeak
  sendATCommand("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"");
  delay(7000);

  // Prepare and send the HTTP GET request with temperature and humidity data
  String str = "GET /update?api_key=" + THINGSPEAK_API_KEY + 
               "&field1=" + String(temperature, 2) + 
               "&field2=" + String(humidityLevel, 2) + 
               " HTTP/1.1\r\nHost: api.thingspeak.com\r\nConnection: close\r\n\r\n";
  
  Serial.println(str);
  
  gprsSerial.println("AT+CIPSEND=" + String(str.length())); // Prepare to send data
  delay(4000);

  ShowSerialData();

  gprsSerial.println(str); // Send the HTTP GET request
  delay(4000);
  ShowSerialData();

  gprsSerial.println((char)26); // End of the request
  delay(4000); // Wait for response

  ShowSerialData(); // Read the final response

  sendATCommand("AT+CIPSHUT"); // Close the connection
  delay(2000); // Wait before the next iteration
}

void ShowSerialData() {
  while (gprsSerial.available()) {
    Serial.write(gprsSerial.read());
  }
  delay(1000); // Shorter delay to read response more quickly
}

void sendATCommand(const char* command) {
  Serial.print("Sending command: ");
  Serial.println(command);
  gprsSerial.println(command);
  ShowSerialData();
}
