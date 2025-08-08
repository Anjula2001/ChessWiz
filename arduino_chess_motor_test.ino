/*
 * Arduino Chess Motorvoid setup() {
  Serial.begin(9600);  // Match your motor controller baud rate
  
  Serial.println("🤖 Arduino Chess Motor Controller - Test Mode");
  Serial.println("📡 Waiting for moves from ESP32...");
  Serial.println("🔌 Connected to ESP32 default Serial (GPIO1/GPIO3) at 9600 baud");
  Serial.println("==========================================");l.println("🤖 Arduino Chess Motor Controller - Test Mode");
  Serial.println("📡 Waiting for moves from ESP32...");
  Serial.println("🔌 Connected to ESP32 default Serial (GPIO1/GPIO3)");
  Serial.println("==========================================");troller - Test Version
 * 
 * This sketch receives chess moves from ESP32 and simulates motor control
 * for testing ESP32 ↔ Arduino communication protocol
 * 
 * Connect to ESP32 default Serial:
 * - Arduino Pin 0 (RX) → ESP32 GPIO3 (TX)
 * - Arduino Pin 1 (TX) → ESP32 GPIO1 (RX)
 * - Arduino GND → ESP32 GND
 * 
 * Protocol:
 * 1. ESP32 sends move (e.g., "e2-e4")
 * 2. Arduino responds "MAGNET_ON"
 * 3. ESP32 activates magnet, responds "MAGNET_READY"
 * 4. Arduino simulates move, responds "MAGNET_OFF"
 * 5. ESP32 deactivates magnet
 */

String receivedMove = "";
bool processingMove = false;

void setup() {
  Serial.begin(9600);  // Match ESP32 ARDUINO_SERIAL baud rate
  
  Serial.println("🤖 Arduino Chess Motor Controller - Test Mode");
  Serial.println("📡 Waiting for moves from ESP32...");
  Serial.println("🔌 Connected to ESP32 default Serial (GPIO1/GPIO3) at 9600 baud");
  Serial.println("==========================================");
}

void loop() {
  // Check for incoming moves from ESP32
  if (Serial.available()) {
    String message = Serial.readStringUntil('\n');
    message.trim();
    
    if (message == "TEST") {
      // ESP32 is testing connection
      Serial.println("ARDUINO_READY");
      return;
    }
    
    if (message.length() >= 4 && !processingMove) {
      // Received a chess move
      receivedMove = message;
      processingMove = true;
      
      Serial.println("📥 Received move: " + receivedMove);
      Serial.println("🔄 Starting move sequence...");
      
      // Step 1: Request magnet activation
      Serial.println("MAGNET_ON");
      Serial.println("🧲 Requested magnet ON from ESP32");
      
      // Wait for ESP32 confirmation
      unsigned long startTime = millis();
      bool magnetReady = false;
      
      while (millis() - startTime < 5000) { // 5 second timeout
        if (Serial.available()) {
          String response = Serial.readStringUntil('\n');
          response.trim();
          
          if (response == "MAGNET_READY") {
            magnetReady = true;
            Serial.println("✅ ESP32 confirmed magnet ready");
            break;
          }
        }
        delay(10);
      }
      
      if (magnetReady) {
        // Step 2: Simulate move execution
        Serial.println("🚀 Simulating motor movement for: " + receivedMove);
        delay(2000); // Simulate 2 seconds of motor movement
        
        // Step 3: Request magnet deactivation
        Serial.println("MAGNET_OFF");
        Serial.println("✅ Move completed, requested magnet OFF");
      } else {
        Serial.println("❌ ESP32 magnet confirmation timeout");
        Serial.println("MAGNET_OFF"); // Try to turn off magnet anyway
      }
      
      processingMove = false;
      Serial.println("==========================================");
      Serial.println("📡 Ready for next move...");
    }
  }
}
