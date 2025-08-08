/*
 * ESP32 Chess Board Bridge - Physical ↔ Web Communication
 * 
 * 🎯 FUNCTION:
 * - Receives physical moves from hall sensors → Sends to web
 * - Receives web/AI moves from server → Sends to Arduino motors
 * 
 * 🎮 SINGLE PLAYER MODE:
 * - Physical player moves → Web bottom player
 * - AI moves → Physical board via Arduino
 * 
 * 👥 MULTIPLAYER MODE:
 * - Physical player moves → Web top player  
 * - Web player moves → Physical board via Arduino
 */

#include <WiFi.h>
#include <HTTPClient.h>

// WiFi credentials
const char* ssid = "POCO X3 NFC";
const char* password = "123456789";

// Server configuration
String getMovesURL = "http://192.168.45.94:3001/getAnyMove";        // Get web/AI moves
String sendMoveURL = "http://192.168.45.94:3001/physicalMove";      // Send physical moves

// --- Pin Definitions ---
#define MAGNET_PIN 23
// Add your hall sensor pins here (example)
#define HALL_SENSOR_START_PIN 2  // Starting pin for hall sensors
// You'll need to define all 64 hall sensor pins for a1-h8

// Timing variables
unsigned long lastPollTime = 0;
unsigned long pollInterval = 1000; // Poll every 1 second
String lastReceivedMove = "";
String lastSentMove = "";
int moveCount = 0;

// Physical board state tracking
bool boardInitialized = false;

void setup() {
  Serial.begin(9600);  // Match Arduino communication speed
  delay(1000);
  
  // Initialize Arduino communication pins
  pinMode(MAGNET_PIN, OUTPUT);
  digitalWrite(MAGNET_PIN, LOW);  // Magnet off at start
  
  // Initialize hall sensor pins (add your actual pin configuration)
  // Example: for (int i = 0; i < 64; i++) { pinMode(HALL_SENSOR_START_PIN + i, INPUT); }
  
  Serial.println("\n========================================");
  Serial.println("🎯 ESP32 Chess Board Bridge");
  Serial.println("🔄 Physical ↔ Web Communication");
  Serial.println("========================================");
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("📍 IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("📥 Get moves from: ");
    Serial.println(getMovesURL);
    Serial.print("📤 Send moves to: ");
    Serial.println(sendMoveURL);
    Serial.println("📡 Polling for web/AI moves every 1 second...");
    Serial.println("👁️ Monitoring hall sensors for physical moves...");
    Serial.println("🤖 Arduino communication ready on GPIO23");
    Serial.println("========================================");
  } else {
    Serial.println("\n❌ WiFi Connection Failed!");
    Serial.println("Please check your WiFi credentials");
  }
}

void loop() {
  // Poll for web/AI moves every second
  if (millis() - lastPollTime >= pollInterval) {
    checkForWebMoves();
    lastPollTime = millis();
  }
  
  // Check for physical moves from hall sensors
  checkForPhysicalMoves();
  
  delay(50);
}

void checkForMoves() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected");
    return;
  }
  
  HTTPClient http;
  WiFiClient client;
  
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  
  // Show polling status every 10 polls
  if (moveCount % 10 == 0) {
    Serial.print("📡 Polling... HTTP: ");
    Serial.print(httpCode);
    Serial.println(" (Waiting for moves)");
  }
  
  if (httpCode > 0) {
    String response = http.getString();
    
    if (httpCode == 200) {
      // Parse JSON response for move
      int moveStart = response.indexOf("\"move\":\"");
      if (moveStart != -1) {
        moveStart += 8; // Skip "move":"
        int moveEnd = response.indexOf("\"", moveStart);
        if (moveEnd != -1) {
          String move = response.substring(moveStart, moveEnd);
          
          // Check if it's a valid move and not null
          if (move.length() >= 4 && move != "null" && move != lastReceivedMove) {
            lastReceivedMove = move;
            moveCount++;
            
            // Parse source room
            String source = "unknown";
            int sourceStart = response.indexOf("\"source\":\"");
            if (sourceStart != -1) {
              sourceStart += 10; // Skip "source":"
              int sourceEnd = response.indexOf("\"", sourceStart);
              if (sourceEnd != -1) {
                source = response.substring(sourceStart, sourceEnd);
              }
            }
            
            // BIG CLEAR DISPLAY FOR MOVES
            Serial.println();
            Serial.println();
            Serial.println("************************************************");
            Serial.println("�🚨🚨 CHESS MOVE RECEIVED! 🚨🚨🚨");
            Serial.println("************************************************");
            Serial.print("🎯 MOVE #");
            Serial.print(moveCount);
            Serial.print(": ");
            Serial.println(move);
            Serial.print("📍 SOURCE: ");
            Serial.println(source);
            
            if (source == "singleplayer-default") {
              Serial.println("🤖 TYPE: AI Move (Single Player)");
            } else if (source == "default") {
              Serial.println("👤 TYPE: Bottom Player Move (Multiplayer)");
            }
            
            // Parse move components
            String fromSquare = "";
            String toSquare = "";
            if (move.indexOf("-") != -1) {
              int dashPos = move.indexOf("-");
              fromSquare = move.substring(0, dashPos);
              toSquare = move.substring(dashPos + 1);
            } else if (move.length() >= 4) {
              fromSquare = move.substring(0, 2);
              toSquare = move.substring(2, 4);
            }
            
            Serial.print("📤 FROM: ");
            Serial.print(fromSquare);
            Serial.print(" ➡️ TO: ");
            Serial.println(toSquare);
            Serial.print("🕐 TIME: ");
            Serial.println(millis());
            Serial.print("📏 MOVE LENGTH: ");
            Serial.println(move.length());
            Serial.print("🔍 RAW RESPONSE: ");
            Serial.println(response);
            Serial.print("📱 LAST MOVE: ");
            Serial.println(lastReceivedMove);
            Serial.print("🔢 HTTP CODE: ");
            Serial.println(httpCode);
            Serial.print("📊 MOVE COUNT: ");
            Serial.println(moveCount);
            Serial.println("************************************************");
            Serial.println("🤖 SENDING MOVE TO ARDUINO...");
            Serial.println("************************************************");
            
            // Send move to Arduino and handle the sequence
            sendMoveToArduino(move);
            
            Serial.println();
            Serial.println();
          }
        }
      }
    }
  }
  
  http.end();
}

void sendMoveToArduino(String move) {
  // Enhanced debug information before sending
  Serial.println("📡 ===== ARDUINO COMMUNICATION START =====");
  Serial.print("📤 Sending move to Arduino: ");
  Serial.println(move);
  Serial.print("📏 Move length: ");
  Serial.println(move.length());
  Serial.print("� Send timestamp: ");
  Serial.println(millis());
  
  // Send the move to Arduino via Serial
  Serial.println(move);
  Serial.println("📡 Move sent via Serial to Arduino");
  
  // Handle the complete move sequence with Arduino
  handleMoveSequence();
}

void handleMoveSequence() {
  Serial.println("🤖 Waiting for Arduino commands...");
  unsigned long startTime = millis();
  const unsigned long timeout = 120000; // 120 seconds timeout (increased for diagonal moves)
  
  while (millis() - startTime < timeout) {
    if (Serial.available()) {
      String msg = Serial.readStringUntil('\n');
      msg.trim();
      
      Serial.print("📥 Received from Arduino: ");
      Serial.println(msg);
      
      if (msg == "MAGNET_ON") {
        // Arduino requests magnet ON
        Serial.println("⚡ Turning magnet ON");
        digitalWrite(MAGNET_PIN, HIGH);
        
        // Debug: Check if GPIO is actually HIGH
        Serial.print("🔍 GPIO23 state: ");
        Serial.println(digitalRead(MAGNET_PIN));
        
        // Send confirmation to Arduino
        Serial.println("MAGNET_READY");
        Serial.println("🧲 Magnet will stay ON until MAGNET_OFF command...");
        
      } else if (msg == "MAGNET_OFF") {
        // Arduino requests magnet OFF
        Serial.println("🔌 Turning magnet OFF");
        digitalWrite(MAGNET_PIN, LOW);
        Serial.print("🔍 GPIO23 state: ");
        Serial.println(digitalRead(MAGNET_PIN));
        Serial.println("✅ Move sequence completed!");
        return; // Exit the function - move is complete
        
      } else if (msg.startsWith("Received move:") || 
                 msg.startsWith("Moving to") || 
                 msg.startsWith("Move completed") ||
                 msg.startsWith("Ready for next") ||
                 msg.startsWith("Diagonal move")) {
        // Just status messages from Arduino - acknowledge them
        continue;
      }
    }
    delay(10); // Small delay to prevent busy waiting
  }
  
  // Timeout occurred
  Serial.println("⏰ Timeout waiting for Arduino!");
  digitalWrite(MAGNET_PIN, LOW);  // Turn magnet OFF on timeout
}
