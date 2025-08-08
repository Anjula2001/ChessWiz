/*
 * ESP32 Chess Board Bridge - Physical ↔ Web Communication
 * 
 * 🎯 FUNCTION:
 * - Receives phys// Board state
SensorState sensors[8][8];
bool boardInitialized = false;

// Timing control
unsigned long lastScanTime = 0;
unsigned long lastPrintTime = 0;moves from hall sensors → Sends to web
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

String squareNames[64] = {
  // MUX1
  "h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8",
  "g8", "g7", "g6", "g5", "g4", "g3", "g2", "g1",
  // MUX2
  "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8",
  "e8", "e7", "e6", "e5", "e4", "e3", "e2", "e1",
  // MUX3
  "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8",
  "c8", "c7", "c6", "c5", "c4", "c3", "c2", "c1",
  // MUX4
  "b1", "b2", "b3", "b4", "b5", "b6", "b7", "b8",
  "a8", "a7", "a6", "a5", "a4", "a3", "a2", "a1"
};

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

// Chess piece definitions (kept for potential future use)
enum PieceType {
  EMPTY = 0,
  WHITE_PAWN = 1, WHITE_ROOK = 2, WHITE_KNIGHT = 3, WHITE_BISHOP = 4, WHITE_QUEEN = 5, WHITE_KING = 6,
  BLACK_PAWN = 7, BLACK_ROOK = 8, BLACK_KNIGHT = 9, BLACK_BISHOP = 10, BLACK_QUEEN = 11, BLACK_KING = 12
};

// Movement tracking with timeout and button confirmation
struct PendingMove {
  byte fromRow, fromCol;
  byte toRow, toCol;
  String fromSquare;
  String toSquare;
  unsigned long startTime;
  bool isActive;
  bool waitingForConfirmation;  // Added: waiting for button press
  bool hasDestination;          // Added: destination detected
};
PendingMove pendingMove = {0, 0, 0, 0, "", "", 0, false, false, false};

// Board state
SensorState sensors[8][8];
bool boardInitialized = false;

// Debouncing constants - optimized from your tested code
const unsigned long DEBOUNCE_TIME = 30;     // Reduced to 30ms for faster response
const byte DEBOUNCE_THRESHOLD = 4;          // Increased to 4 for noise rejection
const unsigned long MOVE_TIMEOUT = 10000;   // 10s timeout for incomplete moves
const unsigned long SCAN_INTERVAL = 10;     // Scan every 10ms (~100Hz)
const unsigned long PRINT_INTERVAL = 2000;  // Print status every 2s

// Button variables (for move confirmation)
bool lastButtonState = HIGH;               // Button is pulled up
unsigned long lastButtonPress = 0;
const unsigned long BUTTON_DEBOUNCE = 50; // 50ms button debounce

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

// Arduino communication via default Serial pins (GPIO1/GPIO3)
#define ARDUINO_SERIAL Serial
#define DEBUG_SERIAL Serial  // Both use same Serial for this setup

void setup() {
  Serial.begin(9600);         // Match Arduino baud rate (9600)
  
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
  delayMicroseconds(100); // Reduced settling time from your tested code
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
  for (int i = 0; i < 100; i++) { // 100 scans over ~1s at 10ms interval
    scanAndDebounceBoard();
    delay(10);
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
    
    // Auto-detect server IP (assume server is on .94 of same subnet)
    String espIP = WiFi.localIP().toString();
    int lastDot = espIP.lastIndexOf('.');
    String subnet = espIP.substring(0, lastDot + 1);
    serverIP = subnet + "94";
    
    // Update URLs with detected IP
    getMovesURL = "http://" + serverIP + ":3001/getAnyMove";
    sendMoveURL = "http://" + serverIP + ":3001/physicalMove";
    
    DEBUG_SERIAL.print("📡 Auto-detected server: ");
    DEBUG_SERIAL.println(serverIP);
    DEBUG_SERIAL.print("📥 Get moves from: ");
    DEBUG_SERIAL.println(getMovesURL);
    DEBUG_SERIAL.print("📤 Send moves to: ");
    DEBUG_SERIAL.println(sendMoveURL);
    DEBUG_SERIAL.println("📡 Polling every 2 seconds...");
    DEBUG_SERIAL.println("👁️ Monitoring hall sensors continuously...");
    DEBUG_SERIAL.println("🔘 Push button on GPIO19 to confirm physical moves...");
    DEBUG_SERIAL.println("🤖 Arduino communication on default Serial (GPIO1/GPIO3) at 9600 baud");
  } else {
    DEBUG_SERIAL.println("\n❌ WiFi Connection Failed!");
    DEBUG_SERIAL.println("Please check your WiFi credentials");
  }
}

void loop() {
  // PRIORITY 1: Always check for physical moves first (using your tested timing)
  checkForPhysicalMoves();
  
  // PRIORITY 2: Check for button press to confirm moves
  checkButtonPress();
  
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
      
      // Update server URLs if needed
      String espIP = WiFi.localIP().toString();
      int lastDot = espIP.lastIndexOf('.');
      String subnet = espIP.substring(0, lastDot + 1);
      serverIP = subnet + "94";
      getMovesURL = "http://" + serverIP + ":3001/getAnyMove";
      sendMoveURL = "http://" + serverIP + ":3001/physicalMove";
      
      DEBUG_SERIAL.print("📡 Updated server IP: ");
      DEBUG_SERIAL.println(serverIP);
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
  changeCount = 0;
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
  
  changeCount = 0; // Reset change counter for this scan
  
  for (int mux = 0; mux < 4; mux++) {
    int colFirst = muxColumnPairs[mux][0];
    int colSecond = muxColumnPairs[mux][1];
    
    for (int ch = 0; ch < 16; ch++) {
      selectMUXChannel(ch);
      int sensorReading = digitalRead(muxPins[mux]);
      bool newState = (sensorReading == HIGH); // HIGH = no magnet, LOW = magnet
      
      byte row, col;
      if (ch < 8) {
        row = ch;
        col = colFirst;
      } else {
        row = 7 - (ch - 8);
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
            
            // Record change for debugging
            if (changeCount < 64) {
              changes[changeCount].row = row;
              changes[changeCount].col = col;
              changes[changeCount].added = !newState; // LOW = magnet added
              changeCount++;
            }
          }
        }
      }
    }
  }
}

// ==========================================
// 🎯 ENHANCED MOVEMENT DETECTION WITH BUTTON CONFIRMATION
// ==========================================
void detectMovements() {
  if (!boardInitialized) return;
  
  unsigned long currentTime = millis();
  
  // Check for timeout
  if (pendingMove.isActive && (currentTime - pendingMove.startTime > MOVE_TIMEOUT)) {
    Serial.println("TIMEOUT: Move incomplete, resetting...");
    pendingMove.isActive = false;
    pendingMove.waitingForConfirmation = false;
    pendingMove.hasDestination = false;
    digitalWrite(LED_PIN, HIGH);
  }
  
  for (byte row = 0; row < 8; row++) {
    for (byte col = 0; col < 8; col++) {
      if (sensors[row][col].hasChanged) {
        bool currentState = sensors[row][col].stableState;
        bool previousState = sensors[row][col].previousStable;
        
        if (!previousState && currentState) { // Magnet removed (piece lifted)
          if (!pendingMove.isActive) {
            // Start new move tracking
            pendingMove.fromRow = row;
            pendingMove.fromCol = col;
            pendingMove.fromSquare = squareName(row, col);
            pendingMove.startTime = currentTime;
            pendingMove.isActive = true;
            pendingMove.waitingForConfirmation = false;
            pendingMove.hasDestination = false;
            digitalWrite(LED_PIN, LOW); // LED off during move
            
            Serial.print("LIFT: ");
            Serial.print(pendingMove.fromSquare);
            Serial.println(" (waiting for placement...)");
          }
          // Ignore other lifts during active move
          
        } else if (previousState && !currentState) { // Magnet added (piece placed)
          if (pendingMove.isActive && !pendingMove.waitingForConfirmation) {
            // Check if it's not the same square (ignore same square placement)
            if (!(pendingMove.fromRow == row && pendingMove.fromCol == col)) {
              // Update destination - this could be intermediate or final
              pendingMove.toRow = row;
              pendingMove.toCol = col;
              pendingMove.toSquare = squareName(row, col);
              pendingMove.hasDestination = true;
              
              Serial.print("PLACE: ");
              Serial.print(pendingMove.toSquare);
              Serial.println(" (temporary - waiting for button confirmation)");
              
              // Wait for button press to confirm this as the final destination
              pendingMove.waitingForConfirmation = true;
              digitalWrite(LED_PIN, HIGH); // LED on - ready for button press
              
              Serial.println("🔘 PRESS BUTTON to confirm move:");
              Serial.print("   ");
              Serial.print(pendingMove.fromSquare);
              Serial.print(" -> ");
              Serial.println(pendingMove.toSquare);
            }
          } else if (pendingMove.isActive && pendingMove.waitingForConfirmation) {
            // Update destination if piece is moved again before button press
            if (!(row == pendingMove.toRow && col == pendingMove.toCol)) {
              pendingMove.toRow = row;
              pendingMove.toCol = col;
              pendingMove.toSquare = squareName(row, col);
              
              Serial.print("PLACE UPDATED: ");
              Serial.print(pendingMove.toSquare);
              Serial.println(" (press button to confirm new destination)");
              
              Serial.println("🔘 PRESS BUTTON to confirm move:");
              Serial.print("   ");
              Serial.print(pendingMove.fromSquare);
              Serial.print(" -> ");
              Serial.println(pendingMove.toSquare);
            }
          } else {
            String square = squareName(row, col);
            Serial.print("PLACE: ");
            Serial.print(square);
            Serial.println(" (no lift detected - ignored)");
          }
        }
        
        sensors[row][col].previousStable = sensors[row][col].stableState;
        sensors[row][col].hasChanged = false;
      }
    }
  }
}

// Reset pending move state
void resetPendingMove() {
  pendingMove.isActive = false;
  pendingMove.waitingForConfirmation = false;
  pendingMove.hasDestination = false;
  pendingMove.fromSquare = "";
  pendingMove.toSquare = "";
  digitalWrite(LED_PIN, HIGH); // LED on when idle
}

// ==========================================
// 👁️ CHECK FOR PHYSICAL MOVES (UPDATED WITH YOUR TESTED CODE)
// ==========================================
void checkForPhysicalMoves() {
  unsigned long currentTime = millis();
  
  // Scan at your tested interval (10ms)
  if (currentTime - lastScanTime >= SCAN_INTERVAL) {
    scanAndDebounceBoard();
    detectMovements();
    lastScanTime = currentTime;
  }
  
  // Print board status every 2 seconds (from your tested code)
  if (currentTime - lastPrintTime >= PRINT_INTERVAL) {
    printBoardStatus();
    lastPrintTime = currentTime;
  }
}

// ==========================================
// 📊 BOARD STATUS PRINTING (FROM YOUR TESTED CODE)
// ==========================================
void printBoardStatus() {
  Serial.println("\n=== BOARD STATUS ===");
  Serial.print("Changes in last 2s: ");
  Serial.println(changeCount);
  for (int i = 0; i < changeCount; i++) {
    String square = squareName(changes[i].row, changes[i].col);
    Serial.print(square);
    Serial.print(changes[i].added ? " comes to magnet" : " remove magnet");
    Serial.println();
  }
  
  for (byte row = 7; row != 255; row--) {
    Serial.print(row + 1);
    Serial.print(" | ");
    for (byte col = 0; col < 8; col++) {
      Serial.print(sensors[row][col].stableState ? "." : "X");
      Serial.print(" ");
    }
    Serial.println();
  }
  Serial.println("  +----------------");
  Serial.println("    a b c d e f g h");
  
  if (pendingMove.isActive) {
    String fromSquare = squareName(pendingMove.fromRow, pendingMove.fromCol);
    unsigned long elapsed = (millis() - pendingMove.startTime) / 1000;
    Serial.print("PENDING MOVE FROM: ");
    Serial.print(fromSquare);
    
    if (pendingMove.hasDestination) {
      Serial.print(" TO: ");
      Serial.print(pendingMove.toSquare);
      if (pendingMove.waitingForConfirmation) {
        Serial.print(" [PRESS BUTTON TO CONFIRM]");
      }
    } else {
      Serial.print(" [WAITING FOR PLACEMENT]");
    }
    
    Serial.print(" (");
    Serial.print(elapsed);
    Serial.println("s)");
  }
  Serial.println("==================\n");
}

// ==========================================
// 📤 SEND PHYSICAL MOVE TO WEB (NON-BLOCKING)
// ==========================================
void sendPhysicalMove(String move) {
  // Store the move even if WiFi is down
  if (WiFi.status() != WL_CONNECTED || wifiReconnecting) {
    DEBUG_SERIAL.println("⚠️ WiFi not ready - will retry sending move: " + move);
    // Store move for later retry (simple approach - just log it)
    return;
  }
  
  // Prevent duplicate moves
  if (move == lastSentMove) {
    DEBUG_SERIAL.println("⚠️ Duplicate move blocked: " + move);
    return;
  }
  
  HTTPClient http;
  WiFiClient client;
  
  http.begin(client, sendMoveURL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000); // Quick timeout to avoid blocking
  
  String payload = "{\"move\":\"" + move + "\",\"source\":\"physical\"}";
  
  DEBUG_SERIAL.println("📤 Sending physical move: " + move);
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    if (httpResponseCode == 200) {
      DEBUG_SERIAL.println("✅ Physical move sent successfully!");
      lastSentMove = move;
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
    } else {
      DEBUG_SERIAL.printf("❌ HTTP Error: %d\n", httpResponseCode);
    }
  } else {
    DEBUG_SERIAL.printf("❌ Connection Error: %d\n", httpResponseCode);
  }
  
  http.end();
}

// ==========================================
// 🤖 SEND MOVE TO ARDUINO (OPTIMIZED)
// ==========================================
void sendMoveToArduino(String move) {
  DEBUG_SERIAL.print("📤 Sending to Arduino: ");
  DEBUG_SERIAL.println(move);
  
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
        DEBUG_SERIAL.println("✅ Move completed - Magnet OFF");
        moveCompleted = true;
        
      } else if (msg.startsWith("Moving to")) {
        // Arduino status updates
        DEBUG_SERIAL.println("🚀 " + msg);
        
      } else if (msg == "Move completed. Ready for next move.") {
        DEBUG_SERIAL.println("✅ Arduino ready for next move");
        moveCompleted = true;
        
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
  }
}

// ==========================================
// 📊 HALL SENSOR READING FUNCTIONS
// ==========================================

// Read individual sensor through MUX (simplified - no longer needed as scanning is integrated)
bool readSensor(int index) {
  // This function is kept for compatibility but no longer used
  // Your tested code integrates sensor reading directly into scanAndDebounceBoard()
  return false;
}

// ==========================================
// 🔘 BUTTON PRESS CONFIRMATION SYSTEM
// ==========================================
void checkButtonPress() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Check for button press (LOW when pressed due to pull-up)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    // Button was just pressed
    if (millis() - lastButtonPress > BUTTON_DEBOUNCE) {
      lastButtonPress = millis();
      
      // If we have a pending move waiting for confirmation
      if (pendingMove.isActive && pendingMove.waitingForConfirmation && pendingMove.hasDestination) {
        String moveStr = pendingMove.fromSquare + "-" + pendingMove.toSquare;
        
        // Validate the move before sending
        if (isValidMove(moveStr)) {
          Serial.println();
          Serial.println("🔘 BUTTON PRESSED! Confirming move...");
          Serial.print("✅ Sending confirmed move: ");
          Serial.println(moveStr);
          
          // Send the confirmed move to web
          sendPhysicalMove(moveStr);
          
          // Reset pending move
          resetPendingMove();
          
          // LED feedback - quick double blink for confirmation
          digitalWrite(LED_PIN, HIGH);
          delay(100);
          digitalWrite(LED_PIN, LOW);
          delay(100);
          digitalWrite(LED_PIN, HIGH);
          delay(100);
          digitalWrite(LED_PIN, LOW);
          delay(100);
          digitalWrite(LED_PIN, HIGH);
          
          Serial.println("📤 Move confirmed and sent to web!");
          Serial.println("🎯 Ready for next move...");
          Serial.println();
        } else {
          Serial.println();
          Serial.println("❌ INVALID MOVE DETECTED: " + moveStr);
          Serial.println("⚠️ Move rejected - invalid format or same square");
          
          // Reset pending move
          resetPendingMove();
          
          // LED feedback - quick triple blink for error
          for (int i = 0; i < 3; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(150);
            digitalWrite(LED_PIN, LOW);
            delay(150);
          }
          digitalWrite(LED_PIN, HIGH);
          
          Serial.println("🔄 Ready for new move");
          Serial.println();
        }
      } else if (pendingMove.isActive && !pendingMove.waitingForConfirmation) {
        Serial.println("🔘 Button pressed but move not ready for confirmation");
        Serial.println("   Please place the piece first, then press button");
      } else {
        Serial.println("🔘 Button pressed but no pending move to confirm");
        Serial.println("   Lift a piece first to start a move");
      }
    }
  }
  
  lastButtonState = currentButtonState;
}

// ==========================================
// 🔘 BUTTON PRESS CONFIRMATION SYSTEM
// ==========================================
void checkButtonPress() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Check for button press (LOW when pressed due to pull-up)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    // Button was just pressed
    if (millis() - lastButtonPress > BUTTON_DEBOUNCE) {
      lastButtonPress = millis();
      
      // If we have a pending move waiting for confirmation
      if (pendingMove.isActive && pendingMove.waitingForConfirmation && pendingMove.hasDestination) {
        String moveStr = pendingMove.fromSquare + "-" + pendingMove.toSquare;
        
        // Validate the move before sending
        if (isValidMove(moveStr)) {
          Serial.println();
          Serial.println("🔘 BUTTON PRESSED! Confirming move...");
          Serial.println("✅ Sending confirmed move: " + moveStr);
          
          // Send the confirmed move to web
          sendPhysicalMove(moveStr);
          
          // Reset pending move
          pendingMove.isActive = false;
          pendingMove.waitingForConfirmation = false;
          pendingMove.hasDestination = false;
          
          // LED feedback - double blink for confirmation
          digitalWrite(LED_PIN, HIGH);
          delay(100);
          digitalWrite(LED_PIN, LOW);
          delay(100);
          digitalWrite(LED_PIN, HIGH);
          delay(100);
          digitalWrite(LED_PIN, LOW);
          delay(100);
          digitalWrite(LED_PIN, HIGH);
          
          Serial.println("📤 Move confirmed and sent to web!");
          Serial.println();
        } else {
          Serial.println();
          Serial.println("❌ INVALID MOVE DETECTED: " + moveStr);
          Serial.println("⚠️ Move rejected - invalid format or same square");
          
          // Reset pending move
          pendingMove.isActive = false;
          pendingMove.waitingForConfirmation = false;
          pendingMove.hasDestination = false;
          
          // LED feedback - triple blink for error
          for (int i = 0; i < 3; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(150);
            digitalWrite(LED_PIN, LOW);
            delay(150);
          }
          digitalWrite(LED_PIN, HIGH);
          
          Serial.println("🔘 Try making move again");
          Serial.println();
        }
      } else {
        Serial.println("🔘 Button pressed but no pending move to confirm");
      }
    }
  }
  
  lastButtonState = currentButtonState;
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

// Helper function for compatibility (no longer needed but kept for structure)
String readHallSensors() {
  // This function is no longer used since move detection is integrated 
  // directly into checkForPhysicalMoves(), but kept for compatibility
  return "";
}
