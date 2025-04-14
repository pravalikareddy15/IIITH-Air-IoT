#include <SoftwareSerial.h>

// Define RX and TX pins for the GPRS module
#define GPRS_RX 26 // Adjust according to your connections
#define GPRS_TX 25 // Adjust according to your connections

// OM2M configuration
#define CSE_IP      "dev-onem2m.iiit.ac.in"
#define CSE_PORT    80 // Change this to 80 for HTTP
#define OM2M_ORGIN  "Tue_20_12_22:Tue_20_12_22"
#define OM2M_MN     "/~/in-cse/in-name/"
#define OM2M_AE     "AE-WM/WM-WD"
#define OM2M_DATA_CONT  "WM-WD-PL00-00/Data"

// Use SoftwareSerial for GPRS communication
SoftwareSerial gprsSerial(GPRS_RX, GPRS_TX);

void setup() {
  // Initialize serial communication
  gprsSerial.begin(9600); // Initialize GPRS serial communication
  Serial.begin(115200);   // Initialize Serial Monitor

  delay(2000); // Allow time for module initialization
  Serial.println("GSM Module Initialized");

  // Initialize GPRS module
  setupGPRS();
}

void loop() {
  // Generate random sensor data
  float temp = random(27, 48);  // Simulate temperature data
  float rh = random(60, 85);    // Simulate humidity data

  // Format data for OM2M
  String data = "[" + String(temp) + " , " + String(rh) + "]";
  String requestBody = createOM2MRequest(data);

  // Send data to OM2M
  sendDataToOM2M(requestBody);

  delay(30000); // Wait for 30 seconds before sending the next set of data
}

// Function to initialize GPRS connection
void setupGPRS() {
  sendATCommand("AT");               // Check if the module is responding
  delay(1000);
  sendATCommand("AT+CPIN?");         // Check if SIM is ready
  delay(1000);
  sendATCommand("AT+CREG?");         // Check network registration
  delay(3000);
  sendATCommand("AT+CGATT?");        // Check GPRS attachment
  delay(3000);
  sendATCommand("AT+CIPSHUT");       // Close any existing connection
  delay(2000);
  sendATCommand("AT+CIPMUX=0");      // Set single connection mode
  delay(1000);
  sendATCommand("AT+CSTT=\"bsnlnet\""); // Set APN for BSNL (adjust as needed)
  delay(3000);
  sendATCommand("AT+CIICR");         // Bring up wireless connection
  delay(5000);
  sendATCommand("AT+CIFSR");         // Get local IP address
  delay(2000);
}

// Function to send data to OM2M
void sendDataToOM2M(String requestBody) {
  // Start TCP connection to OM2M server
  String cipstartCommand = "AT+CIPSTART=\"TCP\",\"" + String(CSE_IP) + "\"," + String(CSE_PORT);
  sendATCommand(cipstartCommand.c_str());
  delay(5000);

  // Send HTTP POST request
  String httpRequest = "POST " + String(OM2M_MN) + String(OM2M_AE) + "/" + String(OM2M_DATA_CONT) + " HTTP/1.1\r\n";
  httpRequest += "Host: " + String(CSE_IP) + ":" + String(CSE_PORT) + "\r\n";
  httpRequest += "X-M2M-Origin: " + String(OM2M_ORGIN) + "\r\n";
  httpRequest += "Content-Type: application/json;ty=4\r\n";
  httpRequest += "Content-Length: " + String(requestBody.length()) + "\r\n";
  httpRequest += "Connection: close\r\n\r\n";
  httpRequest += requestBody;

  sendATCommand("AT+CIPSEND");
  delay(2000);
  gprsSerial.print(httpRequest);
  gprsSerial.write((char)26); // End of the request
  delay(5000);

  // Close the connection
  sendATCommand("AT+CIPSHUT");
  delay(2000);
}

// Function to create OM2M request body
String createOM2MRequest(String data) {
  static int i = 0; // Unique resource name counter
  String requestBody = "{\"m2m:cin\": {";
  requestBody += "\"con\": \"" + data + "\",";
  requestBody += "\"rn\": \"" + String("cin_") + String(i++) + "\",";
  requestBody += "\"cnf\": \"text\"";
  requestBody += "}}";
  return requestBody;
}

// Function to send AT command and print response
void sendATCommand(const char* command) {
  Serial.print("Sending command: ");
  Serial.println(command);
  gprsSerial.println(command);
  delay(1000);
  ShowSerialData();
}

// Function to display GPRS module response
void ShowSerialData() {
  while (gprsSerial.available()) {
    Serial.write(gprsSerial.read());
  }
  delay(500);
}
