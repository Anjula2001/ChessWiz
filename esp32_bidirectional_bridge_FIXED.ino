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

// Server configuration - DYNAMIC IP DETECTION
String serverIP = "192.168.170.94";  // Update this to match your network
String getMovesURL = "http://" + serverIP + ":3001/getAnyMove";        // Get web/AI moves
String sendMoveURL = "http://" + serverIP + ":3001/physicalMove";      // Send physical moves

// --- Pin Definitions ---
#define MAGNET_PIN 23
#define BUTTON_PIN 19  // Push button to confirm physical moves

// MUX control pins for hall sensors
#define S0 14
#define S1 27
#define S2 26
#define S3 25

// MUX SIG pins for 4 MUXes
#define MUX1_SIG 4
#define MUX2_SIG 16
#define MUX3_SIG 32
#define MUX4_SIG 33

#define LED_PIN 2

// MUX and sensor arrays
int muxPins[4] = {MUX1_SIG, MUX2_SIG, MUX3_SIG, MUX4_SIG};

// Enhanced sensor state structure - from your tested code
struct SensorState {
  bool rawState;
  bool stableState;
  bool previousStable;
  unsigned long lastChangeTime;
  byte consistentCount;
  bool hasChanged;
};

// Change tracking for debugging
struct SensorChange {
  byte row, col;
  bool added; // true = magnet added (HIGH->LOW), false = magnet removed (LOW->HIGH)
};
SensorChange changes[64]; // Max 64 changes per 2s
int changeCount = 0;

// Movement tracking with timeout - simplified like your tested code
struct PendingMove {
  byte fromRow, fromCol;
  unsigned long startTime;
  bool isActive;
};
PendingMove pendingMove = {0, 0, 0, false};

// Board state
SensorState sensors[8][8];
bool boardInitialized = false;

// Debouncing constants - from your tested code
const unsigned long DEBOUNCE_TIME = 30;     // 30ms debounce (faster response)
const byte DEBOUNCE_THRESHOLD = 2;          // Need 2 consistent readings (faster)
const unsigned long MOVE_TIMEOUT = 10000;   // 10 second timeout for incomplete moves
const unsigned long SCAN_INTERVAL = 20;     // Scan every 20ms
const unsigned long PRINT_INTERVAL = 1000; // Print status every 1s

// Button variables for sensor activation control
bool lastButtonState = HIGH;               // Button is pulled up
unsigned long lastButtonPress = 0;
const unsigned long BUTTON_DEBOUNCE = 200; // 200ms button debounce
bool sensorsDisabled = false;              // NEW: Sensors disabled when motors start, enabled by button

// Timing control
unsigned long lastScanTime = 0;
unsigned long lastPrintTime = 0;

// MUX channel selection binary map - from your tested code
const byte MUX_CHANNEL[16][4] = {
  {0, 0, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}, {1, 1, 0, 0},
  {0, 0, 1, 0}, {1, 0, 1, 0}, {0, 1, 1, 0}, {1, 1, 1, 0},
  {0, 0, 0, 1}, {1, 0, 0, 1}, {0, 1, 0, 1}, {1, 1, 0, 1},
  {0, 0, 1, 1}, {1, 0, 1, 1}, {0, 1, 1, 1}, {1, 1, 1, 1}
};

// Timing variables
unsigned long lastPollTime = 0;
unsigned long pollInterval = 2000; // Poll every 2 seconds to prevent overload
unsigned long lastSensorRead = 0;   // Add missing variable
String lastReceivedMove = "";
String lastSentMove = "";
int moveCount = 0;

// Connection stability
unsigned long lastWiFiCheck = 0;
unsigned long wifiCheckInterval = 30000; // Check WiFi every 30 seconds (reduced frequency)
int consecutiveErrors = 0;
const int maxConsecutiveErrors = 10; // Increased tolerance
bool wifiReconnecting = false; // Flag to track reconnection state

// Motor movement state - CRITICAL FIX for ignoring sensor during motor dragging
bool motorMovementActive = false;       // Flag to block sensor detection during motor moves
unsigned long motorMovementStartTime = 0;
const unsigned long MOTOR_MOVEMENT_TIMEOUT = 120000; // 2 minutes max for motor movement
const unsigned long MOTOR_STABILIZATION_TIME = 0;    // 0 seconds - immediate physical player turn

// Arduino communication via default Serial pins (GPIO1/GPIO3)
#define ARDUINO_SERIAL Serial
#define DEBUG_SERIAL Serial  // Both use same Serial for this setup

String squareName(byte row, byte col) {
  char file = 'a' + col;
  char rank = '1' + row;
  return String(file) + String(rank);
}

void selectMUXChannel(int channel) {
  digitalWrite(S0, MUX_CHANNEL[channel][0]);
  digitalWrite(S1, MUX_CHANNEL[channel][1]);
  digitalWrite(S2, MUX_CHANNEL[channel][2]);
  digitalWrite(S3, MUX_CHANNEL[channel][3]);
  delayMicroseconds(300); // Allow channel to settle - from tested code
}

void setup() {
  Serial.begin(115200);  // Keep higher baud rate for ESP32
  
  // Initialize sensors first
  initializeSensors();
  
  setupWiFi();
  
  setupMultiplexers();
  
  lastSensorRead = millis();
  lastWiFiCheck = millis();
  lastScanTime = millis();
  lastPrintTime = millis();
}

void setupWiFi() {
  connectToWiFi();
}

void setupMultiplexers() {
  // Configure MUX control pins
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  
  // Configure MUX signal pins as inputs with pullup (from your tested code)
  for (int i = 0; i < 4; i++) {
    pinMode(muxPins[i], INPUT_PULLUP);
  }
  
  // Configure other pins
  pinMode(MAGNET_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(MAGNET_PIN, LOW);
  digitalWrite(LED_PIN, HIGH); // Start with LED on during initialization
  
  // Initialize sensors with your tested method
  initializeSensors();
  
  Serial.println("Chess Board System Starting...");
  Serial.println("Initializing sensors... Please wait 3 seconds.");
  
  // Calibration phase from your tested code
  for (int i = 0; i < 50; i++) { // 50 scans over ~1 second at 20ms interval
    scanAndDebounceBoard();
    delay(20);
  }
  
  // Set previous stable states after calibration
  for (byte row = 0; row < 8; row++) {
    for (byte col = 0; col < 8; col++) {
      sensors[row][col].previousStable = sensors[row][col].stableState;
    }
  }
  
  boardInitialized = true;
  Serial.println("System Ready! Waiting for movement...");
  Serial.println("Format: FROM_SQUARE -> TO_SQUARE");
  Serial.println("----------------------------------------");
}

void connectToWiFi() {
  DEBUG_SERIAL.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    DEBUG_SERIAL.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_SERIAL.println("\n✅ WiFi Connected!");
    DEBUG_SERIAL.print("📍 ESP32 IP: ");
    DEBUG_SERIAL.println(WiFi.localIP());
    
    // FIXED: Use exact server IP - no auto-detection to prevent reconnection issues
    // Keep the manually set serverIP instead of auto-detecting
    DEBUG_SERIAL.print("📡 Using server IP: ");
    DEBUG_SERIAL.println(serverIP);
    
    // Update URLs with server IP
    getMovesURL = "http://" + serverIP + ":3001/getAnyMove";
    sendMoveURL = "http://" + serverIP + ":3001/physicalMove";
    
    DEBUG_SERIAL.print("📥 Get moves from: ");
    DEBUG_SERIAL.println(getMovesURL);
    DEBUG_SERIAL.print("📤 Send moves to: ");
    DEBUG_SERIAL.println(sendMoveURL);
    DEBUG_SERIAL.println("📡 Polling every 2 seconds...");
    DEBUG_SERIAL.println("👁️ Monitoring hall sensors continuously...");
    DEBUG_SERIAL.println("🔘 Push button on GPIO19 to activate sensor detection after motor moves...");
    DEBUG_SERIAL.println("🤖 Arduino communication on default Serial (GPIO1/GPIO3) at 9600 baud");
  } else {
    DEBUG_SERIAL.println("\n❌ WiFi Connection Failed!");
    DEBUG_SERIAL.println("Please check your WiFi credentials");
  }
}

void loop() {
  // PRIORITY 1: Check button for sensor activation (anytime)
  checkButtonForSensorActivation();
  
  // PRIORITY 2: Check for physical moves ONLY if sensors are enabled
  if (!sensorsDisabled) {
    checkForPhysicalMoves();
  }
  
  // PRIORITY 3: Check WiFi connection (non-blocking)
  checkWiFiConnectionNonBlocking();
  
  // PRIORITY 4: Poll for web/AI moves only if WiFi is stable
  if (WiFi.status() == WL_CONNECTED && !wifiReconnecting) {
    if (millis() - lastPollTime >= pollInterval) {
      checkForWebMoves();
      lastPollTime = millis();
    }
  }
  
  delay(5); // Minimal delay to keep sensor monitoring responsive
}

// ==========================================
// 🌐 WIFI CONNECTION MANAGEMENT
// ==========================================
void checkWiFiConnectionNonBlocking() {
  // Only check WiFi status periodically, don't block physical detection
  if (millis() - lastWiFiCheck >= wifiCheckInterval) {
    lastWiFiCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED && !wifiReconnecting) {
      DEBUG_SERIAL.println("❌ WiFi disconnected! Starting non-blocking reconnection...");
      wifiReconnecting = true;
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      DEBUG_SERIAL.println("🔄 Reconnection started in background...");
    }
  }
  
  // Check if reconnection completed (non-blocking)
  if (wifiReconnecting) {
    if (WiFi.status() == WL_CONNECTED) {
      DEBUG_SERIAL.println("✅ WiFi Reconnected!");
      consecutiveErrors = 0;
      wifiReconnecting = false;
      
      // FIXED: Don't auto-detect IP on reconnection - keep original serverIP
      DEBUG_SERIAL.print("📡 Using server IP: ");
      DEBUG_SERIAL.println(serverIP);
      
      // Update URLs with original server IP
      getMovesURL = "http://" + serverIP + ":3001/getAnyMove";
      sendMoveURL = "http://" + serverIP + ":3001/physicalMove";
    }
    // If still reconnecting, just continue - don't block anything
  }
}

// ==========================================
// 📥 CHECK FOR WEB/AI MOVES TO SEND TO ARDUINO
// ==========================================
void checkForWebMoves() {
  // Skip if WiFi is reconnecting to avoid blocking
  if (WiFi.status() != WL_CONNECTED || wifiReconnecting) {
    return; // Silent return - don't spam error messages
  }
  
  HTTPClient http;
  WiFiClient client;
  
  http.begin(client, getMovesURL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000); // Reduced timeout to 3 seconds
  
  unsigned long httpStartTime = millis();
  int httpCode = http.GET();
  unsigned long httpDuration = millis() - httpStartTime;
  
  // Show polling status every 20 polls (40 seconds at 2000ms intervals)
  if (moveCount % 20 == 0) {
    DEBUG_SERIAL.print("📡 Polling... HTTP: ");
    DEBUG_SERIAL.print(httpCode);
    DEBUG_SERIAL.print(" (");
    DEBUG_SERIAL.print(httpDuration);
    DEBUG_SERIAL.println("ms)");
  }
  
  if (httpCode > 0) {
    consecutiveErrors = 0; // Reset error counter on success
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
            
            // CLEAR DISPLAY FOR WEB MOVES
            DEBUG_SERIAL.println();
            DEBUG_SERIAL.println("************************************************");
            DEBUG_SERIAL.println("🌐 WEB/AI MOVE RECEIVED! 🌐");
            DEBUG_SERIAL.println("************************************************");
            DEBUG_SERIAL.print("🎯 MOVE #");
            DEBUG_SERIAL.print(moveCount);
            DEBUG_SERIAL.print(": ");
            DEBUG_SERIAL.println(move);
            DEBUG_SERIAL.print("📍 SOURCE: ");
            DEBUG_SERIAL.println(source);
            
            if (source == "singleplayer-default") {
              DEBUG_SERIAL.println("🤖 TYPE: AI Move → Send to Arduino");
            } else if (source == "default") {
              DEBUG_SERIAL.println("👤 TYPE: Web Player Move → Send to Arduino");
            }
            
            DEBUG_SERIAL.println("************************************************");
            DEBUG_SERIAL.println("🤖 SENDING TO ARDUINO...");
            
            // Send move to Arduino with optimized timing
            sendMoveToArduino(move);
            DEBUG_SERIAL.println("✅ Move processing complete!");
            DEBUG_SERIAL.println();
          }
        }
      }
    }
  } else {
    consecutiveErrors++;
    // Only show errors for first few attempts to avoid spam
    if (consecutiveErrors <= 3) {
      DEBUG_SERIAL.printf("❌ HTTP Error: %d (attempt %d)\n", httpCode, consecutiveErrors);
    }
  }
  
  http.end();
}

// ==========================================
// 🎯 ENHANCED SENSOR INITIALIZATION (FROM YOUR TESTED CODE)
// ==========================================
void initializeSensors() {
  for (byte row = 0; row < 8; row++) {
    for (byte col = 0; col < 8; col++) {
      sensors[row][col].rawState = true;
      sensors[row][col].stableState = true;
      sensors[row][col].previousStable = true;
      sensors[row][col].lastChangeTime = 0;
      sensors[row][col].consistentCount = DEBOUNCE_THRESHOLD;
      sensors[row][col].hasChanged = false;
    }
  }
}

// ==========================================
// 🎯 ENHANCED SENSOR SCANNING (FROM YOUR TESTED CODE)
// ==========================================
void scanAndDebounceBoard() {
  unsigned long currentTime = millis();
  
  // MUX column mapping from your tested code
  const int muxColumnPairs[4][2] = {
    {7, 6}, // MUX1: h (7), g (6)
    {5, 4}, // MUX2: f (5), e (4)
    {3, 2}, // MUX3: d (3), c (2)
    {1, 0}  // MUX4: b (1), a (0)
  };
  
  for (int mux = 0; mux < 4; mux++) {
    int colFirst = muxColumnPairs[mux][0];
    int colSecond = muxColumnPairs[mux][1];
    
    for (int ch = 0; ch < 16; ch++) {
      selectMUXChannel(ch);
      int sensorReading = digitalRead(muxPins[mux]);
      bool newState = (sensorReading == HIGH); // HIGH = no magnet, LOW = magnet
      
      byte row, col;
      if (ch < 8) {
        row = ch;            // Channel 0 -> row 0 (rank 1), Channel 7 -> row 7 (rank 8)
        col = colFirst;
      } else {
        row = 7 - (ch - 8);  // Channel 8 -> row 7 (rank 8), Channel 15 -> row 0 (rank 1) 
        col = colSecond;
      }
      
      sensors[row][col].rawState = newState;
      
      if (newState == sensors[row][col].stableState) {
        sensors[row][col].consistentCount = DEBOUNCE_THRESHOLD;
        sensors[row][col].lastChangeTime = currentTime;
      } else {
        if (currentTime - sensors[row][col].lastChangeTime >= DEBOUNCE_TIME) {
          sensors[row][col].consistentCount--;
          sensors[row][col].lastChangeTime = currentTime;
          
          if (sensors[row][col].consistentCount == 0) {
            sensors[row][col].stableState = newState;
            sensors[row][col].hasChanged = true;
            sensors[row][col].consistentCount = DEBOUNCE_THRESHOLD;
          }
        }
      }
    }
  }
}

// ==========================================
// 🔘 CHECK BUTTON FOR SENSOR ACTIVATION (FIXED - PROPER LOGIC)
// ==========================================
void checkButtonForSensorActivation() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Button pressed (HIGH to LOW transition)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    unsigned long currentTime = millis();
    
    // Debounce button press
    if (currentTime - lastButtonPress > BUTTON_DEBOUNCE) {
      lastButtonPress = currentTime;
      
      // ALWAYS show button press message
      Serial.println();
      Serial.println("🔘 BUTTON PRESSED!");
      
      if (sensorsDisabled) {
        Serial.println("================================================");
        Serial.println("🔄 Activating sensor detection...");
        
        // Enable sensor detection FIRST
        sensorsDisabled = false;
        
        // Do quick scan to get current state
        scanAndDebounceBoard();
        
        // FORCE all sensors to current stable state as baseline
        for (byte row = 0; row < 8; row++) {
          for (byte col = 0; col < 8; col++) {
            sensors[row][col].previousStable = sensors[row][col].stableState;
            sensors[row][col].hasChanged = false; // Clear any changes
          }
        }
        
        // Clear any pending moves
        pendingMove.isActive = false;
        digitalWrite(LED_PIN, HIGH);
        
        Serial.println("✅ Sensor detection ACTIVATED!");
        Serial.println("🎯 Ready for PHYSICAL PLAYER moves!");
        Serial.println("👤 Make your move on the physical board now...");
        Serial.println("================================================");
      } else {
        Serial.println("ℹ️ Sensors already active");
      }
      Serial.println();
    }
  }
  
  lastButtonState = currentButtonState;
}

// ==========================================
// 🎯 ENHANCED MOVEMENT DETECTION (UPDATED - SIMPLE SENSOR DISABLE)
// ==========================================
void detectMovements() {
  if (!boardInitialized) return;
  
  // CRITICAL: Block sensor detection if sensors are disabled
  if (sensorsDisabled) {
    // Only print status occasionally to avoid spam
    static unsigned long lastStatusMessage = 0;
    if (millis() - lastStatusMessage > 2000) { // Every 2 seconds (reduced from 5)
      Serial.println("🔘 Sensors DISABLED - Press button to activate");
      lastStatusMessage = millis();
    }
    return;
  }
  
  unsigned long currentTime = millis();
  
  // Check for timeout on pending moves
  if (pendingMove.isActive && (currentTime - pendingMove.startTime > MOVE_TIMEOUT)) {
    Serial.println("TIMEOUT: Move incomplete, resetting...");
    pendingMove.isActive = false;
    digitalWrite(LED_PIN, HIGH); // Reset LED
  }
  
  // Process sensor changes
  for (byte row = 0; row < 8; row++) {
    for (byte col = 0; col < 8; col++) {
      if (sensors[row][col].hasChanged) {
        bool currentState = sensors[row][col].stableState;
        bool previousState = sensors[row][col].previousStable;
        
        // Piece lifted (magnet removed: 0 -> 1)
        if (!previousState && currentState) {
          if (!pendingMove.isActive) {
            pendingMove.fromRow = row;
            pendingMove.fromCol = col;
            pendingMove.startTime = currentTime;
            pendingMove.isActive = true;
            digitalWrite(LED_PIN, LOW); // Indicate pending move
            
            String fromSquare = squareName(row, col);
            Serial.println("🎯 PIECE LIFTED: " + fromSquare);
            Serial.println("👤 Waiting for piece placement...");
          }
        }
        
        // Piece placed (magnet detected: 1 -> 0)
        else if (previousState && !currentState) {
          if (pendingMove.isActive) {
            String fromSquare = squareName(pendingMove.fromRow, pendingMove.fromCol);
            String toSquare = squareName(row, col);
            
            // Check if it's not the same square (avoid false positives)
            if (!(pendingMove.fromRow == row && pendingMove.fromCol == col)) {
              Serial.println("================================================");
              Serial.println("🎯 PHYSICAL MOVE DETECTED!");
              Serial.println("================================================");
              Serial.print("📍 FROM: ");
              Serial.println(fromSquare);
              Serial.print("📍 TO: ");
              Serial.println(toSquare);
              
              String moveStr = fromSquare + "-" + toSquare;
              Serial.print("🎮 MOVE: ");
              Serial.println(moveStr);
              
              // IMMEDIATELY mark as processed to prevent duplicate
              pendingMove.isActive = false;
              digitalWrite(LED_PIN, HIGH);
              
              if (isValidMove(moveStr)) {
                Serial.println("✅ Valid move - sending to web...");
                sendPhysicalMove(moveStr);
                Serial.println("📤 Move sent to web chess board!");
              } else {
                Serial.println("❌ Invalid move format!");
              }
              
              Serial.println("================================================");
              
              // FORCE clear the change to prevent showing again
              sensors[row][col].hasChanged = false;
              sensors[pendingMove.fromRow][pendingMove.fromCol].hasChanged = false;
            }
          } else {
            // Piece placed without lift detected (possible missed lift or initial placement)
            String square = squareName(row, col);
            Serial.print("PLACE: ");
            Serial.print(square);
            Serial.println(" (no lift detected)");
          }
        }
        
        // Update previous state and clear change flag
        sensors[row][col].previousStable = sensors[row][col].stableState;
        sensors[row][col].hasChanged = false;
      }
    }
  }
}

// ==========================================
// 👁️ CHECK FOR PHYSICAL MOVES (UPDATED WITH YOUR TESTED CODE)
// ==========================================
void checkForPhysicalMoves() {
  unsigned long currentTime = millis();
  
  // Scan at your tested interval (20ms)
  if (currentTime - lastScanTime >= SCAN_INTERVAL) {
    scanAndDebounceBoard();
    detectMovements();
    lastScanTime = currentTime;
  }
  
  // Print board status every 10 seconds (from your tested code)
  if (currentTime - lastPrintTime >= PRINT_INTERVAL) {
    printBoardStatus();
    lastPrintTime = currentTime;
  }
}

// REMOVED: checkMotorMovementStatus() - No longer needed since button can be pressed anytime
// ==========================================
// 📊 BOARD STATUS PRINTING (UPDATED - FAST 1 SECOND DISPLAY)
// ==========================================
void printBoardStatus() {
  Serial.println("\n=== BOARD STATUS ===");
  
  // Show current system state
  if (sensorsDisabled) {
    Serial.println("🔘 SENSORS DISABLED - Press button on GPIO19 to activate");
  } else {
    Serial.println("🎯 SENSOR DETECTION ACTIVE - Ready for physical moves");
  }
  
  // Quick board display
  for (byte row = 7; row != 255; row--) { // Count down from 7 to 0 (255 handles underflow)
    Serial.print(row + 1); // Show rank 8 to 1
    Serial.print(" | ");
    for (byte col = 0; col < 8; col++) {
      Serial.print(sensors[row][col].stableState ? "." : "X");
      Serial.print(" ");
    }
    Serial.println();
  }
  Serial.println("  +----------------");
  Serial.println("    a b c d e f g h");
  
  if (pendingMove.isActive && !sensorsDisabled) {
    String fromSquare = squareName(pendingMove.fromRow, pendingMove.fromCol);
    unsigned long elapsed = (millis() - pendingMove.startTime) / 1000;
    Serial.print("PENDING MOVE FROM: ");
    Serial.print(fromSquare);
    Serial.print(" (");
    Serial.print(elapsed);
    Serial.println("s)");
  }
  Serial.println("==================\n");
}

// ==========================================
//  SEND PHYSICAL MOVE TO WEB (NON-BLOCKING)
// ==========================================
void sendPhysicalMove(String move) {
  // Store the move even if WiFi is down
  if (WiFi.status() != WL_CONNECTED || wifiReconnecting) {
    Serial.println("⚠️ WiFi not ready - will retry sending move: " + move);
    // Store move for later retry (simple approach - just log it)
    return;
  }
  
  // Prevent duplicate moves
  if (move == lastSentMove) {
    Serial.println("⚠️ Duplicate move blocked: " + move);
    return;
  }
  
  HTTPClient http;
  WiFiClient client;
  
  http.begin(client, sendMoveURL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000); // Quick timeout to avoid blocking
  
  String payload = "{\"move\":\"" + move + "\",\"source\":\"physical\"}";
  
  Serial.println("📤 Sending physical move: " + move);
  Serial.println("📤 Payload: " + payload);
  Serial.println("📤 URL: " + sendMoveURL);
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("📥 Server response: " + response);
    
    if (httpResponseCode == 200) {
      Serial.println("✅ Physical move sent successfully!");
      lastSentMove = move;
      
      // LED feedback - quick double blink for successful send
      digitalWrite(LED_PIN, HIGH);
      delay(10);
      digitalWrite(LED_PIN, LOW);
      delay(10);
      digitalWrite(LED_PIN, HIGH);
    } else {
      Serial.printf("❌ HTTP Error: %d\n", httpResponseCode);
    }
  } else {
    Serial.printf("❌ Connection Error: %d\n", httpResponseCode);
  }
  
  http.end();
}

// ==========================================
// 🤖 SEND MOVE TO ARDUINO (OPTIMIZED - IMMEDIATE SENSOR DISABLE)
// ==========================================
void sendMoveToArduino(String move) {
  DEBUG_SERIAL.print("📤 Sending to Arduino: ");
  DEBUG_SERIAL.println(move);
  
  // IMMEDIATE: Disable sensor detection when motors start
  sensorsDisabled = true;
  
  Serial.println("🚫 SENSOR DETECTION DISABLED - Motor movement starting");
  Serial.println("🤖 Arduino will move piece from web/AI move");
  Serial.println("🔘 Press BUTTON anytime to re-enable sensor detection");
  
  // Send the move directly to Arduino - it will handle the complete sequence
  ARDUINO_SERIAL.println(move);
  
  // Handle the Arduino motor sequence with magnet coordination
  handleArduinoMotorSequence();
}

// ==========================================
// 🔄 HANDLE ARDUINO MOTOR SEQUENCE (UPDATED FOR YOUR MOTOR CONTROLLER)
// ==========================================
void handleArduinoMotorSequence() {
  DEBUG_SERIAL.println("🤖 Coordinating with Arduino motor controller...");
  DEBUG_SERIAL.println("📋 Arduino motor sequence:");
  DEBUG_SERIAL.println("   1. Arduino moves to source position");
  DEBUG_SERIAL.println("   2. Arduino sends 'MAGNET_ON' request");
  DEBUG_SERIAL.println("   3. ESP32 activates magnet, sends 'MAGNET_READY'");
  DEBUG_SERIAL.println("   4. Arduino moves to destination");
  DEBUG_SERIAL.println("   5. Arduino sends 'MAGNET_OFF' request");
  DEBUG_SERIAL.println("   6. Arduino sends 'MOVE_COMPLETED'");
  
  unsigned long startTime = millis();
  const unsigned long timeout = 60000; // 60 seconds timeout for motor moves
  bool moveCompleted = false;
  
  while (millis() - startTime < timeout && !moveCompleted) {
    if (ARDUINO_SERIAL.available()) {
      String msg = ARDUINO_SERIAL.readStringUntil('\n');
      msg.trim();
      
      DEBUG_SERIAL.print("📥 Arduino: ");
      DEBUG_SERIAL.println(msg);
      
      if (msg == "MAGNET_ON") {
        // Arduino is at source position, requests magnet activation
        digitalWrite(MAGNET_PIN, HIGH);
        ARDUINO_SERIAL.println("MAGNET_READY");
        DEBUG_SERIAL.println("🧲 Magnet ON - Arduino can pick up piece");
        
      } else if (msg == "MAGNET_OFF") {
        // Arduino completed move, requests magnet deactivation
        digitalWrite(MAGNET_PIN, LOW);
        DEBUG_SERIAL.println("🧲 Magnet OFF - Piece placed");
        
      } else if (msg == "MOVE_COMPLETED" || msg == "Move completed. Ready for next move." || msg.indexOf("Move completed successfully") >= 0) {
        DEBUG_SERIAL.println("✅ Arduino movement sequence completed");
        moveCompleted = true;
        // Note: motorMovementActive will be disabled in checkMotorMovementStatus()
        
      } else if (msg.startsWith("Moving to")) {
        // Arduino status updates
        DEBUG_SERIAL.println("🚀 " + msg);
        
      } else if (msg.startsWith("Received move:")) {
        DEBUG_SERIAL.println("📨 " + msg);
        
      } else {
        DEBUG_SERIAL.println("ℹ️ Arduino: " + msg);
      }
    }
    delay(10); // Small delay for Arduino communication
  }
  
  // Safety check - ensure magnet is off
  digitalWrite(MAGNET_PIN, LOW);
  
  if (!moveCompleted) {
    DEBUG_SERIAL.println("⏰ Arduino motor sequence timeout!");
    DEBUG_SERIAL.println("🔧 Check:");
    DEBUG_SERIAL.println("   1. Arduino motor controller is running");
    DEBUG_SERIAL.println("   2. Stepper motors are connected and powered");
    DEBUG_SERIAL.println("   3. Chess move is valid (Arduino can reach positions)");
    DEBUG_SERIAL.println("   4. Baud rate is 9600 on both ESP32 and Arduino");
    DEBUG_SERIAL.println("   5. Arduino sends 'MOVE_COMPLETED' when done");
    DEBUG_SERIAL.println("🔘 Press BUTTON anytime to re-enable sensor detection");
  }
}

// ==========================================
// ✅ SIMPLE MOVE VALIDATION (SIMPLIFIED)
// ==========================================
bool isValidMove(String move) {
  // Basic format validation only (your tested code handles detection correctly)
  if (move.length() != 5) return false;
  if (move.charAt(2) != '-') return false;
  
  String fromSquare = move.substring(0, 2);
  String toSquare = move.substring(3, 5);
  
  // Check if from and to squares are the same
  if (fromSquare == toSquare) return false;
  
  // Basic chess notation check
  char fromFile = fromSquare.charAt(0);
  char fromRank = fromSquare.charAt(1);
  char toFile = toSquare.charAt(0);
  char toRank = toSquare.charAt(1);
  
  if (fromFile < 'a' || fromFile > 'h' || 
      fromRank < '1' || fromRank > '8' ||
      toFile < 'a' || toFile > 'h' || 
      toRank < '1' || toRank > '8') {
    return false;
  }
  
  return true;
}
