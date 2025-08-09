// === RESET SYSTEM TEST ===
// 
// This code tests the reset functionality for both ESP32 and Arduino
// Upload this to ESP32 to test reset button

#define RESET_BUTTON_PIN 18
#define ARDUINO_RESET_PIN 4
#define MAGNET_PIN 23

bool lastResetButtonState = HIGH;
unsigned long lastResetButtonPress = 0;
const unsigned long RESET_BUTTON_DEBOUNCE = 300;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ARDUINO_RESET_PIN, OUTPUT);
  pinMode(MAGNET_PIN, OUTPUT);
  
  digitalWrite(ARDUINO_RESET_PIN, HIGH);  // Keep Arduino running
  digitalWrite(MAGNET_PIN, LOW);          // Magnet off
  
  Serial.println("=== ESP32 Reset Test Ready ===");
  Serial.println("Press GPIO18 button to test reset");
  Serial.println("Watching for button press...");
}

void loop() {
  // Check reset button
  bool currentResetButtonState = digitalRead(RESET_BUTTON_PIN);
  
  if (lastResetButtonState == HIGH && currentResetButtonState == LOW) {
    unsigned long currentTime = millis();
    
    if (currentTime - lastResetButtonPress > RESET_BUTTON_DEBOUNCE) {
      lastResetButtonPress = currentTime;
      
      Serial.println("RESET BUTTON PRESSED!");
      Serial.println("Sending reset commands to Arduino...");
      
      // Test software reset
      Serial.println("RESET_ARDUINO");
      Serial.flush();
      delay(300);
      
      // Test hardware reset (if connected)
      Serial.println("Hardware resetting Arduino...");
      digitalWrite(ARDUINO_RESET_PIN, LOW);
      delay(100);
      digitalWrite(ARDUINO_RESET_PIN, HIGH);
      delay(500);
      
      // Send software reset again
      Serial.println("RESET_ARDUINO");
      Serial.flush();
      
      Serial.println("Reset commands sent. ESP32 restarting in 2 seconds...");
      delay(2000);
      ESP.restart();
    }
  }
  
  lastResetButtonState = currentResetButtonState;
  
  delay(10);
}
