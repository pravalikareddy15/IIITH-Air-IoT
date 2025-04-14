#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  5  // in seconds

void setup() {
  Serial.begin(115200);
  delay(1000); // Give time for serial to connect

  Serial.println("Starting Light Sleep for 5 seconds...");

  // Configure timer wakeup
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Set up the system to go into light sleep
  esp_light_sleep_start();

  // This line will execute after wakeup
  Serial.println("Woke up from light sleep!");
}

void loop() {
  // Not used in this example
}
