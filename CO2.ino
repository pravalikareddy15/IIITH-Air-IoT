const int CO2_PIN = 33; // Define the PWM pin for the CO2 sensor
unsigned long th, tl;
int ppm;

void setup() {
  Serial.begin(9600); // Initialize serial communication
  pinMode(CO2_PIN, INPUT); // Set CO2_PIN as input
}

void loop() {
  CO2_Monitor(); // Call the CO2 monitoring function
  delay(10000); // Delay before the next reading
}

void CO2_Monitor() {
  th = pulseIn(CO2_PIN, HIGH, 2008000) / 1000; // Measure the high pulse width
  tl = 1004 - th; // Calculate the low pulse width
  ppm = 2000 * (th - 2) / (th + tl - 4); // Calculate CO2 concentration in ppm

  Serial.print("CO2 Concentration: ");
  Serial.println(ppm); // Print the CO2 concentration
}
