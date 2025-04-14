#include <Adafruit_AHT10.h>
#include <Wire.h>

// CO2 Sensor Configuration
const int CO2_PIN = 13; // Define the PWM pin for the CO2 sensor
unsigned long th, tl;
int ppm;

// PM Sensor Configuration
byte command_frame[9] = {0xAA, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x67, 0xBB};
byte received_data[9];
unsigned long prev_time = 0;
int sum = 0;
int pm25 = 0;
int pm10 = 0;

// Sound Sensor Configuration
#define SoundSensorPin 36  // Pin for the sound sensor
#define VREF  5.0          // Voltage on AREF pin, default operating voltage

Adafruit_AHT10 aht;
Adafruit_Sensor* aht_humidity;
Adafruit_Sensor* aht_temp;
float temp;
float humd;

void setup() {
  Serial.begin(9600); // Initialize serial communication
  pinMode(CO2_PIN, INPUT); // Set CO2_PIN as input
  aht_setup();
  pm_setup();
}

void loop() {
  CO2_Monitor(); // Call the CO2 monitoring function
  pm_loop();     // Call the PM sensor function
  aht_loop();    // Call the AHT10 sensor function
  sound_loop();   // Call the sound sensor function
  delay(10000);   // Delay before the next reading
}

// CO2 Monitoring Function
void CO2_Monitor() {
  th = pulseIn(CO2_PIN, HIGH, 2008000) / 1000; // Measure the high pulse width
  tl = 1004 - th; // Calculate the low pulse width
  ppm = 2000 * (th - 2) / (th + tl - 4); // Calculate CO2 concentration in ppm

  Serial.print("CO2 Concentration: ");
  Serial.println(ppm); // Print the CO2 concentration
}

// PM Sensor Functions
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
  pm10 = int(received_data[2]) * 256 + int(received_data[3]);
  Serial.print("PM2.5: ");
  Serial.println(pm25);
  Serial.print("PM10: ");
  Serial.println(pm10);
}

// AHT10 Sensor Functions
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

// AHT10 Loop
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
}

// Sound Sensor Function
void sound_loop() {
  float voltageValue, dbValue;
  voltageValue = analogRead(SoundSensorPin) / 4096.0 * VREF; // Read the analog value and convert it to voltage
  dbValue = voltageValue * 50.0;  // Convert voltage to decibel value
  
  Serial.print("Noise Level: ");
  Serial.print(dbValue, 1);
  Serial.println(" dBA");
}
