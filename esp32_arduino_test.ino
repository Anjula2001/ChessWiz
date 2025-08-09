// === ESP32-ARDUINO COMMUNICATION TEST FOR GPIO1/3 WIRING ===
// 
// Tests communication between ESP32 and Arduino using:
// ESP32 GPIO1 (TX) → Arduino GPIO0 (RX)
// ESP32 GPIO3 (RX) → Arduino GPIO1 (TX)
// ESP32 GND → Arduino GND

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== ESP32-Arduino GPIO1/3 Communication Test ===");
  Serial.println("Wiring: ESP32(1,3) ↔ Arduino(0,1)");
  
  // Test 1: Send test command to verify Arduino connection
  Serial.println("Sending ESP32_TEST...");
  Serial.println("ESP32_TEST");
  Serial.flush();
  delay(2000);
  
  // Test 2: Send a chess move command
  Serial.println("Sending test move e2-e4...");
  Serial.println("e2-e4");
  Serial.flush();
  
  Serial.println("Test commands sent. Check Arduino for motor movement.");
}

void loop() {
  // Listen for any Arduino responses
  if (Serial.available()) {
    String response = Serial.readStringUntil('\n');
    response.trim();
    if (response.length() > 0) {
      Serial.print("Arduino says: ");
      Serial.println(response);
    }
  }
  
  delay(100);
}
