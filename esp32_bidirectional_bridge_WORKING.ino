/*
 * ESP32 Chess Board Bridge - Physical ↔ Web Communication
 * DUAL-CORE FREERTOS IMPLEMENTATION - FIXED VERSION
 * 
 * 🎯 FUNCTION:
 * - Receives physical moves from hall sensors → Sends to web
 * - Receives web/AI moves from server → Sends to Arduino motors
 * 
 * 🔄 DUAL-CORE ARCHITECTURE:
 * - CORE 0: WiFi communications, HTTP requests, Arduino communication
 * - CORE 1: Physical sensor detection, button handling, real-time operations
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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// WiFi credentials
const char* ssid = "POCO X3 NFC";
const char* password = "123456789";

// Server configuration - FIXED TO USE LOCAL IP
// YOU NEED TO UPDATE THIS TO YOUR COMPUTER'S IP ADDRESS
// Find your computer's IP by running "ipconfig" (Windows) or "ifconfig" (Mac/Linux)
const char* serverIP = "192.168.1.100";  // CHANGE THIS TO YOUR COMPUTER'S IP ADDRESS
const char* getMovesURL = "http://192.168.1.100:3001/getAnyMove";        // CHANGE IP HERE TOO
const char* sendMoveURL = "http://192.168.1.100:3001/physicalMove";      // CHANGE IP HERE TOO

// Alternative: If you want to test locally on same machine, use:
// const char* serverIP = "localhost";
// const char* getMovesURL = "http://localhost:3001/getAnyMove";
// const char* sendMoveURL = "http://localhost:3001/physicalMove";

// --- Pin Definitions ---
#define MAGNET_PIN 23
#define BUTTON_PIN 19  // Push button to confirm physical moves
#define RESET_BUTTON_PIN 18  // Push button to reset whole game (ESP + Arduino)

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

// Enhanced movement tracking with button confirmation
struct PendingMove {
  byte fromRow, fromCol;
  byte toRow, toCol;
  unsigned long startTime;
  bool isActive;
  bool pieceLifted;
  bool piecePlaced;
  bool readyToConfirm;
};
PendingMove pendingMove = {0, 0, 0, 0, 0, false, false, false, false};

// Track all sensor changes during move sequence
struct SensorChangeLog {
  byte row, col;
  bool state; // true = piece placed, false = piece lifted
  unsigned long timestamp;
};
SensorChangeLog changeLog[10]; // Track up to 10 changes
int changeLogCount = 0;

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

// Reset button variables
bool lastResetButtonState = HIGH;         // Reset button is pulled up
unsigned long lastResetButtonPress = 0;
const unsigned long RESET_BUTTON_DEBOUNCE = 300; // 300ms reset button debounce

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

// Timing variables - MEMORY CORRUPTION FIX
unsigned long lastPollTime = 0;
unsigned long pollInterval = 5000; // Poll every 5 seconds (much less aggressive to prevent buffer corruption)
unsigned long lastSensorRead = 0;   
String lastReceivedMove = "";
String lastReceivedSource = "";  // NEW: Track the source of received moves
String webPlayerColor = "";      // NEW: Track the web player's chosen color
String lastSentMove = "";
int moveCount = 0;

// Connection stability - MEMORY CORRUPTION FIX
unsigned long lastWiFiCheck = 0;
unsigned long wifiCheckInterval = 120000; // Check WiFi every 2 minutes (prevent buffer corruption)
int consecutiveErrors = 0;
const int maxConsecutiveErrors = 5; // Much lower tolerance - fail fast to prevent corruption
bool wifiReconnecting = false; // Flag to track reconnection state
bool networkStable = true; // Track network stability

// Motor movement state - CRITICAL FIX for ignoring sensor during motor dragging
bool motorMovementActive = false;       // Flag to block sensor detection during motor moves
unsigned long motorMovementStartTime = 0;
const unsigned long MOTOR_MOVEMENT_TIMEOUT = 120000; // 2 minutes max for motor movement
const unsigned long MOTOR_STABILIZATION_TIME = 0;    // 0 seconds - immediate physical player turn

// Arduino communication via default Serial pins (GPIO1/GPIO3)
#define ARDUINO_SERIAL Serial
#define DEBUG_SERIAL Serial  // Both use same Serial for this setup

// ==========================================
// 🔄 FREERTOS DUAL-CORE VARIABLES
// ==========================================
// Task handles
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;

// Inter-core communication queues
QueueHandle_t physicalMoveQueue;    // Core 1 → Core 0: Physical moves to send
QueueHandle_t webMoveQueue;         // Core 0 → Core 1: Web moves received
QueueHandle_t arduinoMsgQueue;      // Core 0 → Core 1: Arduino messages

// Mutex for shared resources
SemaphoreHandle_t sensorMutex;
SemaphoreHandle_t serialMutex;

// Message structures for inter-core communication
struct PhysicalMoveMsg {
  char move[10];        // e.g., "e2-e4"
  char source[20];      // "physical"
};

struct WebMoveMsg {
  char move[10];        // e.g., "e7-e5"
  char source[30];      // "singleplayer-default" or "default"
};

struct ArduinoMsg {
  char message[50];     // Arduino serial messages
};

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
  
  // Initialize FreeRTOS synchronization objects
  physicalMoveQueue = xQueueCreate(5, sizeof(PhysicalMoveMsg));
  webMoveQueue = xQueueCreate(5, sizeof(WebMoveMsg));
  arduinoMsgQueue = xQueueCreate(10, sizeof(ArduinoMsg));
  
  sensorMutex = xSemaphoreCreateMutex();
  serialMutex = xSemaphoreCreateMutex();
  
  // Initialize sensors first
  initializeSensors();
  
  setupMultiplexers();
  
  // Initialize timing variables
  lastSensorRead = millis();
  lastWiFiCheck = millis();
  lastScanTime = millis();
  lastPrintTime = millis();
  
  Serial.println("🔄 Starting DUAL-CORE FreeRTOS Chess System...");
  Serial.println("📡 Core 0: WiFi communications & Arduino coordination");
  Serial.println("🎯 Core 1: Sensor detection & button handling");
  
  // Create WiFi task on Core 0 (Protocol CPU)
  xTaskCreatePinnedToCore(
    wifiTask,           // Function
    "WiFiTask",         // Name
    8192,               // Stack size (8KB)
    NULL,               // Parameters
    2,                  // Priority (high for WiFi)
    &wifiTaskHandle,    // Handle
    0                   // Core 0 (Protocol CPU)
  );
  
  // Create sensor task on Core 1 (Application CPU)
  xTaskCreatePinnedToCore(
    sensorTask,         // Function
    "SensorTask",       // Name
    4096,               // Stack size (4KB)
    NULL,               // Parameters
    3,                  // Priority (highest for real-time sensors)
    &sensorTaskHandle,  // Handle
    1                   // Core 1 (Application CPU)
  );
  
  Serial.println("✅ Dual-core tasks created successfully!");
  Serial.println("🎮 System ready for chess gameplay!");
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
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);  // Reset button with pullup
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
  Serial.print("Connecting to WiFi");
  
  // MEMORY CORRUPTION FIX: Configure WiFi for maximum stability
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  
  // MEMORY CORRUPTION FIX: Set power management for stability
  WiFi.setSleep(false); // Disable WiFi sleep to prevent buffer corruption
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("📍 ESP32 IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("📡 Server IP: ");
    Serial.println(serverIP);
    Serial.print("📥 Get moves from: ");
    Serial.println(getMovesURL);
    Serial.print("📤 Send moves to: ");
    Serial.println(sendMoveURL);
    Serial.println("📡 Polling every 5 seconds (memory-safe)...");
    Serial.println("👁️ Monitoring hall sensors continuously...");
    Serial.println("🔘 Push button on GPIO19 to activate sensor detection after motor moves...");
    Serial.println("🔄 Push button on GPIO18 to RESET whole game (ESP32 + Arduino)...");
    Serial.println("🤖 Arduino communication on default Serial (GPIO1/GPIO3) at 115200 baud");
    Serial.println("🛡️ Network buffer corruption protection enabled");
    
    // MEMORY CORRUPTION FIX: Initialize network stability flag
    networkStable = true;
    consecutiveErrors = 0;
  } else {
    Serial.println("\n❌ WiFi Connection Failed!");
    Serial.println("Please check your WiFi credentials");
    networkStable = false;
  }
}

void loop() {
  // Main loop is now empty - all work done by FreeRTOS tasks
  // This allows the ESP32 to manage tasks automatically
  vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for 1 second
}

// ==========================================
// 🌐 CORE 0 TASK: WiFi Communications & Arduino
// ==========================================
void wifiTask(void *parameter) {
  // Initialize WiFi on Core 0
  setupWiFi();
  
  unsigned long lastPollTime = 0;
  unsigned long lastWiFiCheck = 0;
  
  Serial.println("📡 Core 0: WiFi task started");
  
  while (true) {
    // Handle WiFi connection management
    if (millis() - lastWiFiCheck >= wifiCheckInterval) {
      checkWiFiConnectionNonBlocking();
      lastWiFiCheck = millis();
    }
    
    // Poll for web/AI moves - MEMORY CORRUPTION FIX: Only when network is stable
    if (WiFi.status() == WL_CONNECTED && !wifiReconnecting && networkStable) {
      if (millis() - lastPollTime >= pollInterval) {
        checkForWebMovesTask();
        lastPollTime = millis();
      }
    }
    
    // Handle physical moves from Core 1
    PhysicalMoveMsg physicalMove;
    if (xQueueReceive(physicalMoveQueue, &physicalMove, 0) == pdTRUE) {
      sendPhysicalMoveTask(String(physicalMove.move));
    }
    
    // Handle Arduino communication
    handleArduinoCommunicationTask();
    
    // MEMORY CORRUPTION FIX: Longer task delay to reduce CPU load and network pressure
    vTaskDelay(pdMS_TO_TICKS(50)); // 50ms delay instead of 10ms
  }
}

// ==========================================
// 🎯 CORE 1 TASK: Sensor Detection & Button Handling
// ==========================================
void sensorTask(void *parameter) {
  unsigned long lastScanTime = 0;
  unsigned long lastPrintTime = 0;
  
  Serial.println("🎯 Core 1: Sensor task started");
  
  while (true) {
    // Reset button handling (highest priority - check first)
    checkResetButtonTask();
    
    // Button handling (highest priority)
    checkButtonForSensorActivationTask();
    
    // Handle web moves from Core 0
    WebMoveMsg webMove;
    if (xQueueReceive(webMoveQueue, &webMove, 0) == pdTRUE) {
      processWebMoveTask(String(webMove.move), String(webMove.source));
    }
    
    // Handle Arduino messages from Core 0
    ArduinoMsg arduinoMsg;
    if (xQueueReceive(arduinoMsgQueue, &arduinoMsg, 0) == pdTRUE) {
      processArduinoMessageTask(String(arduinoMsg.message));
    }
    
    // Physical sensor detection
    if (!sensorsDisabled) {
      if (millis() - lastScanTime >= SCAN_INTERVAL) {
        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          scanAndDebounceBoard();
          detectMovementsTask();
          xSemaphoreGive(sensorMutex);
          lastScanTime = millis();
        }
      }
      
      // Print board status periodically
      if (millis() - lastPrintTime >= PRINT_INTERVAL) {
        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          printBoardStatusTask();
          xSemaphoreGive(sensorMutex);
          lastPrintTime = millis();
        }
      }
    }
    
    // Small delay to prevent watchdog issues
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ==========================================
// 📡 CORE 0 TASK FUNCTIONS: WiFi & Arduino Communication
// ==========================================

void checkForWebMovesTask() {
  // MEMORY CORRUPTION FIX: Skip if network is unstable
  if (WiFi.status() != WL_CONNECTED || wifiReconnecting || !networkStable) {
    return;
  }
  
  HTTPClient http;
  WiFiClient client;
  
  // MEMORY CORRUPTION FIX: Shorter timeouts and proper cleanup
  if (!http.begin(client, getMovesURL)) {
    Serial.println("❌ HTTP begin failed - skipping poll");
    return;
  }
  
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(1500); // Shorter timeout: 1.5 seconds to prevent buffer buildup
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    consecutiveErrors = 0;
    String response = http.getString();
    
    // MEMORY CORRUPTION FIX: Limit response size
    if (response.length() > 1000) {
      Serial.println("⚠️ Response too large, truncating to prevent memory issues");
      response = response.substring(0, 1000);
    }
    
    if (httpCode == 200) {
      int moveStart = response.indexOf("\"move\":\"");
      if (moveStart != -1) {
        moveStart += 8;
        int moveEnd = response.indexOf("\"", moveStart);
        if (moveEnd != -1) {
          String move = response.substring(moveStart, moveEnd);
          
          if (move.length() >= 4 && move != "null" && move != lastReceivedMove) {
            lastReceivedMove = move;
            moveCount++;
            
            String source = "unknown";
            int sourceStart = response.indexOf("\"source\":\"");
            if (sourceStart != -1) {
              sourceStart += 10;
              int sourceEnd = response.indexOf("\"", sourceStart);
              if (sourceEnd != -1) {
                source = response.substring(sourceStart, sourceEnd);
                lastReceivedSource = source; // STORE THE SOURCE
              }
            }
            
            // EXTRACT WEB PLAYER COLOR
            String playerSide = "unknown";
            int playerStart = response.indexOf("\"playerSide\":\"");
            if (playerStart != -1) {
              playerStart += 14;
              int playerEnd = response.indexOf("\"", playerStart);
              if (playerEnd != -1) {
                playerSide = response.substring(playerStart, playerEnd);
                webPlayerColor = playerSide; // STORE THE WEB PLAYER'S COLOR
              }
            }
            
            // Send to Core 1 via queue
            WebMoveMsg webMove;
            strncpy(webMove.move, move.c_str(), sizeof(webMove.move) - 1);
            strncpy(webMove.source, source.c_str(), sizeof(webMove.source) - 1);
            webMove.move[sizeof(webMove.move) - 1] = '\0'; // Ensure null termination
            webMove.source[sizeof(webMove.source) - 1] = '\0';
            xQueueSend(webMoveQueue, &webMove, 0);
            
            if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
              Serial.println("🌐 WEB/AI MOVE → Sent to Core 1");
              xSemaphoreGive(serialMutex);
            }
          }
        }
      }
    }
  } else {
    consecutiveErrors++;
    if (consecutiveErrors >= maxConsecutiveErrors) {
      Serial.printf("❌ Too many HTTP errors (%d), marking network unstable\n", consecutiveErrors);
      networkStable = false;
      // Reset after 30 seconds
      if (consecutiveErrors >= maxConsecutiveErrors + 10) {
        consecutiveErrors = 0;
        networkStable = true;
        Serial.println("🔄 Resetting network stability flag");
      }
    }
  }
  
  // MEMORY CORRUPTION FIX: Always properly end HTTP connection
  http.end();
  
  // MEMORY CORRUPTION FIX: Small delay to allow network stack to clean up
  delay(10);
}

void sendPhysicalMoveTask(String move) {
  // MEMORY CORRUPTION FIX: Check network stability first
  if (WiFi.status() != WL_CONNECTED || wifiReconnecting || !networkStable) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("⚠️ Network not stable - cannot send move: " + move);
      xSemaphoreGive(serialMutex);
    }
    return;
  }
  
  if (move == lastSentMove) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("⚠️ Duplicate move blocked: " + move);
      xSemaphoreGive(serialMutex);
    }
    return;
  }
  
  HTTPClient http;
  WiFiClient client;
  
  // MEMORY CORRUPTION FIX: Check if HTTP can begin properly
  if (!http.begin(client, sendMoveURL)) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("❌ HTTP begin failed for physical move");
      xSemaphoreGive(serialMutex);
    }
    return;
  }
  
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(1500); // Shorter timeout to prevent buffer corruption
  
  // SIMPLIFIED ROOM ID LOGIC - ALWAYS USE SINGLEPLAYER-DEFAULT FOR PHYSICAL MOVES
  String targetPlayerSide = "white"; // Default to white
  String targetRoomId = "singleplayer-default"; // Always use singleplayer room for physical moves
  String targetPlayerInfo = "BOTTOM (WHITE) - Single Player";
  
  // DYNAMIC PLAYER ASSIGNMENT BASED ON WEB PLAYER COLOR
  if (lastReceivedSource == "ai" || lastReceivedSource == "human_vs_ai") {
    // SINGLE PLAYER MODE: Physical board = Bottom side player (web player = physical player)
    targetRoomId = "singleplayer-default";
    if (webPlayerColor == "white") {
      targetPlayerSide = "white";
      targetPlayerInfo = "BOTTOM (WHITE) - Single Player";
    } else if (webPlayerColor == "black") {
      targetPlayerSide = "black";
      targetPlayerInfo = "BOTTOM (BLACK) - Single Player";
    } else {
      // Default to white if color not detected
      targetPlayerSide = "white";
      targetPlayerInfo = "BOTTOM (WHITE) - Single Player Default";
    }
  } else if (lastReceivedSource == "web" || lastReceivedSource == "human_vs_human" || lastReceivedSource == "default") {
    // MULTIPLAYER MODE: Physical board = Top side player
    targetRoomId = "default"; // Use default room for multiplayer
    if (webPlayerColor == "white") {
      // Web player is white, so top side (physical) is black
      targetPlayerSide = "black";
      targetPlayerInfo = "TOP (BLACK) - Multiplayer";
    } else if (webPlayerColor == "black") {
      // Web player is black, so top side (physical) is white
      targetPlayerSide = "white";
      targetPlayerInfo = "TOP (WHITE) - Multiplayer";
    } else {
      // Default to black if color not detected
      targetPlayerSide = "black";
      targetPlayerInfo = "TOP (BLACK) - Multiplayer Default";
    }
  }
  
  // FIXED PAYLOAD FORMAT - MATCHES SERVER EXPECTATIONS EXACTLY
  const char* payloadTemplate = "{\"move\":\"%s\",\"source\":\"physical\",\"roomId\":\"%s\",\"playerSide\":\"%s\"}";
  char payload[200]; // Increased buffer size
  int result = snprintf(payload, sizeof(payload), payloadTemplate, move.c_str(), targetRoomId.c_str(), targetPlayerSide.c_str());
  
  // MEMORY CORRUPTION FIX: Check if payload was truncated
  if (result >= sizeof(payload)) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("❌ Payload too large, move not sent");
      xSemaphoreGive(serialMutex);
    }
    http.end();
    return;
  }
  
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    Serial.println("================================================");
    Serial.println("📤 SENDING PHYSICAL MOVE TO WEB");
    Serial.println("================================================");
    Serial.println("🎯 Move: " + move);
    Serial.println("🎮 Game Mode Source: " + lastReceivedSource);
    Serial.println("🎨 Web Player Color: " + webPlayerColor);
    Serial.println("👤 Physical Player Assignment: " + targetPlayerInfo);
    Serial.print("📤 URL: ");
    Serial.println(sendMoveURL);
    Serial.print("📤 Payload: ");
    Serial.println(payload);
    Serial.println("📡 Posting to web server...");
    xSemaphoreGive(serialMutex);
  }
  
  int httpResponseCode = http.POST(payload);
  
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    Serial.print("📥 HTTP Response Code: ");
    Serial.println(httpResponseCode);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      
      // MEMORY CORRUPTION FIX: Limit response size
      if (response.length() > 500) {
        response = response.substring(0, 500);
        Serial.println("⚠️ Response truncated to prevent memory issues");
      }
      
      Serial.print("📥 Server Response: ");
      Serial.println(response);
      
      if (httpResponseCode == 200) {
        Serial.println("✅ Physical move sent successfully to web!");
        Serial.println("🎯 " + targetPlayerInfo + " player should see move on web interface");
        lastSentMove = move;
        
        // LED feedback for successful send
        digitalWrite(LED_PIN, HIGH);
        delay(10);
        digitalWrite(LED_PIN, LOW);
        delay(10);
        digitalWrite(LED_PIN, HIGH);
      } else {
        Serial.printf("❌ HTTP Error: %d - Move may not appear on web\n", httpResponseCode);
      }
    } else {
      Serial.printf("❌ Connection Error: %d - Check network connection\n", httpResponseCode);
      // Mark network as unstable on connection errors
      consecutiveErrors++;
      if (consecutiveErrors >= 3) {
        networkStable = false;
        Serial.println("⚠️ Marking network as unstable due to connection errors");
      }
    }
    Serial.println("================================================");
    xSemaphoreGive(serialMutex);
  }
  
  // MEMORY CORRUPTION FIX: Always properly end HTTP connection
  http.end();
  
  // MEMORY CORRUPTION FIX: Small delay to allow network stack to clean up
  delay(10);
}

void handleArduinoCommunicationTask() {
  if (ARDUINO_SERIAL.available()) {
    String msg = ARDUINO_SERIAL.readStringUntil('\n');
    msg.trim();
    
    if (msg.length() > 0) {
      // Send Arduino message to Core 1 for processing
      ArduinoMsg arduinoMsg;
      strncpy(arduinoMsg.message, msg.c_str(), sizeof(arduinoMsg.message) - 1);
      arduinoMsg.message[sizeof(arduinoMsg.message) - 1] = '\0'; // Ensure null termination
      xQueueSend(arduinoMsgQueue, &arduinoMsg, 0);
      
      // Handle magnet control on Core 0 (immediate response) - PRIMARY HANDLER
      if (msg == "MAGNET_ON") {
        digitalWrite(MAGNET_PIN, HIGH);
        ARDUINO_SERIAL.println("MAGNET_READY");
        if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          Serial.println("🧲 Core 0: Magnet ON - Arduino request");
          xSemaphoreGive(serialMutex);
        }
      } else if (msg == "MAGNET_OFF") {
        digitalWrite(MAGNET_PIN, LOW);
        if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          Serial.println("🧲 Core 0: Magnet OFF - Arduino request");
          xSemaphoreGive(serialMutex);
        }
      }
    }
  }
}

// ==========================================
// 🎯 CORE 1 TASK FUNCTIONS: Sensor & Button Handling
// ==========================================

void processWebMoveTask(String move, String source) {
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    Serial.println("************************************************");
    Serial.println("🌐 WEB/AI MOVE RECEIVED! 🌐");
    Serial.println("************************************************");
    Serial.print("🎯 MOVE: ");
    Serial.println(move);
    Serial.print("📍 SOURCE: ");
    Serial.println(source);
    
    if (source == "singleplayer-default") {
      Serial.println("🤖 TYPE: AI Move → Send to Arduino");
    } else if (source == "default") {
      Serial.println("👤 TYPE: Web Player Move → Send to Arduino");
    }
    
    Serial.println("🚫 SENSOR DETECTION DISABLED");
    Serial.println("🤖 Arduino will move piece");
    Serial.println("🔘 Press BUTTON to re-enable sensors");
    Serial.println("************************************************");
    xSemaphoreGive(serialMutex);
  }
  
  // Disable sensors immediately
  sensorsDisabled = true;
  
  // Send move to Arduino via Core 0 (where Arduino communication happens)
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    Serial.print("📤 Sending to Arduino: ");
    Serial.println(move);
    ARDUINO_SERIAL.println(move);
    ARDUINO_SERIAL.flush(); // Force send immediately
    Serial.println("✅ Move sent to Arduino via Core 1");
    xSemaphoreGive(serialMutex);
  }
}

void processArduinoMessageTask(String msg) {
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    Serial.print("📥 Arduino: ");
    Serial.println(msg);
    
    if (msg.startsWith("Moving to")) {
      Serial.println("🚀 " + msg);
    } else if (msg.startsWith("Received move:")) {
      Serial.println("📨 " + msg);
    } else if (msg == "MOVE_COMPLETED" || msg == "Move completed. Ready for next move." || msg.indexOf("Move completed successfully") >= 0) {
      Serial.println("✅ Arduino movement sequence completed");
      Serial.println("🔘 Press BUTTON anytime to re-enable sensor detection");
    } else if (msg == "MAGNET_ON") {
      // Handle magnet control on Core 1 as well
      digitalWrite(MAGNET_PIN, HIGH);
      Serial.println("🧲 Magnet ON - Arduino request");
      ARDUINO_SERIAL.println("MAGNET_READY");
    } else if (msg == "MAGNET_OFF") {
      // Handle magnet control on Core 1 as well
      digitalWrite(MAGNET_PIN, LOW);
      Serial.println("🧲 Magnet OFF - Arduino request");
    } else {
      Serial.println("ℹ️ Arduino: " + msg);
    }
    xSemaphoreGive(serialMutex);
  }
}

void checkButtonForSensorActivationTask() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    unsigned long currentTime = millis();
    
    if (currentTime - lastButtonPress > BUTTON_DEBOUNCE) {
      lastButtonPress = currentTime;
      
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        Serial.println("🔘 BUTTON PRESSED!");
        
        // CASE 1: Sensors are disabled - activate them
        if (sensorsDisabled) {
          Serial.println("✅ Sensor detection ACTIVATED!");
          Serial.println("🎯 Ready for PHYSICAL PLAYER moves!");
          
          sensorsDisabled = false;
          
          // Reset sensor states
          if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            scanAndDebounceBoard();
            for (byte row = 0; row < 8; row++) {
              for (byte col = 0; col < 8; col++) {
                sensors[row][col].previousStable = sensors[row][col].stableState;
                sensors[row][col].hasChanged = false;
              }
            }
            resetPendingMove();
            xSemaphoreGive(sensorMutex);
          }
        }
        // CASE 2: Move is ready for confirmation - send it
        else if (pendingMove.readyToConfirm) {
          String fromSquare = squareName(pendingMove.fromRow, pendingMove.fromCol);
          String toSquare = squareName(pendingMove.toRow, pendingMove.toCol);
          String moveStr = fromSquare + "-" + toSquare;
          
          Serial.println("================================================");
          Serial.println("📤 BUTTON CONFIRMATION - SENDING MOVE!");
          Serial.println("================================================");
          Serial.println("🎯 CONFIRMED MOVE: " + moveStr);
          Serial.println("📤 Sending to web interface...");
          
          if (isValidMove(moveStr)) {
            // Send to Core 0 via queue
            PhysicalMoveMsg physicalMove;
            strncpy(physicalMove.move, moveStr.c_str(), sizeof(physicalMove.move) - 1);
            strncpy(physicalMove.source, "physical", sizeof(physicalMove.source) - 1);
            physicalMove.move[sizeof(physicalMove.move) - 1] = '\0';
            physicalMove.source[sizeof(physicalMove.source) - 1] = '\0';
            xQueueSend(physicalMoveQueue, &physicalMove, 0);
            
            Serial.println("✅ MOVE CONFIRMED AND SENT!");
            Serial.println("🌐 Should appear on web interface now");
            
            // Show change log for debugging
            Serial.println("📋 Move sequence log:");
            for (int i = 0; i < changeLogCount; i++) {
              String square = squareName(changeLog[i].row, changeLog[i].col);
              String action = changeLog[i].state ? "PLACED" : "LIFTED";
              Serial.println("   " + String(i+1) + ". " + square + " " + action);
            }
          } else {
            Serial.println("❌ Invalid move format: " + moveStr);
          }
          
          Serial.println("================================================");
          resetPendingMove();
        }
        // CASE 3: Sensors active but no pending move
        else {
          Serial.println("ℹ️ Sensors active, no move pending");
          if (pendingMove.isActive) {
            Serial.println("📋 Current move status:");
            Serial.println("   Piece lifted: " + String(pendingMove.pieceLifted ? "YES" : "NO"));
            Serial.println("   Piece placed: " + String(pendingMove.piecePlaced ? "YES" : "NO"));
            Serial.println("   Ready to confirm: " + String(pendingMove.readyToConfirm ? "YES" : "NO"));
          }
        }
        xSemaphoreGive(serialMutex);
      }
    }
  }
  
  lastButtonState = currentButtonState;
}

void detectMovementsTask() {
  if (!boardInitialized || sensorsDisabled) return;
  
  unsigned long currentTime = millis();
  
  // Timeout handling
  if (pendingMove.isActive && (currentTime - pendingMove.startTime > MOVE_TIMEOUT)) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("⏰ TIMEOUT: Move incomplete, resetting...");
      xSemaphoreGive(serialMutex);
    }
    resetPendingMove();
  }
  
  // Process sensor changes
  for (byte row = 0; row < 8; row++) {
    for (byte col = 0; col < 8; col++) {
      if (sensors[row][col].hasChanged) {
        bool currentState = sensors[row][col].stableState;
        bool previousState = sensors[row][col].previousStable;
        
        // PIECE LIFTED (magnet removed: LOW -> HIGH)
        if (!previousState && currentState) {
          handlePieceLifted(row, col, currentTime);
        }
        
        // PIECE PLACED (magnet detected: HIGH -> LOW)
        else if (previousState && !currentState) {
          handlePiecePlaced(row, col, currentTime);
        }
        
        // Update sensor state
        sensors[row][col].previousStable = sensors[row][col].stableState;
        sensors[row][col].hasChanged = false;
      }
    }
  }
}

void handlePieceLifted(byte row, byte col, unsigned long currentTime) {
  String square = squareName(row, col);
  
  if (!pendingMove.isActive) {
    // START NEW MOVE SEQUENCE
    pendingMove.fromRow = row;
    pendingMove.fromCol = col;
    pendingMove.startTime = currentTime;
    pendingMove.isActive = true;
    pendingMove.pieceLifted = true;
    pendingMove.piecePlaced = false;
    pendingMove.readyToConfirm = false;
    
    // Clear change log
    changeLogCount = 0;
    
    digitalWrite(LED_PIN, LOW); // Indicate active move
    
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("🎯 PIECE LIFTED: " + square);
      Serial.println("📋 Move sequence started - waiting for piece placement...");
      xSemaphoreGive(serialMutex);
    }
    
    // Log this change
    logSensorChange(row, col, false, currentTime);
  } else {
    // Additional piece lifted during active move (ignore or log)
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("⚠️ Additional piece lifted during move: " + square + " (ignoring)");
      xSemaphoreGive(serialMutex);
    }
    logSensorChange(row, col, false, currentTime);
  }
}

void handlePiecePlaced(byte row, byte col, unsigned long currentTime) {
  String square = squareName(row, col);
  
  if (pendingMove.isActive && pendingMove.pieceLifted) {
    // Check if this is the same square where piece was lifted (piece put back)
    if (row == pendingMove.fromRow && col == pendingMove.fromCol) {
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        Serial.println("🔄 PIECE PUT BACK: " + square + " (move cancelled)");
        xSemaphoreGive(serialMutex);
      }
      resetPendingMove();
      return;
    }
    
    // PIECE PLACED ON DIFFERENT SQUARE - POTENTIAL MOVE COMPLETION
    pendingMove.toRow = row;
    pendingMove.toCol = col;
    pendingMove.piecePlaced = true;
    pendingMove.readyToConfirm = true;
    
    digitalWrite(LED_PIN, HIGH); // Indicate move ready for confirmation
    
    String fromSquare = squareName(pendingMove.fromRow, pendingMove.fromCol);
    String toSquare = squareName(row, col);
    String moveStr = fromSquare + "-" + toSquare;
    
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("================================================");
      Serial.println("✅ MOVE READY FOR CONFIRMATION!");
      Serial.println("================================================");
      Serial.println("🎯 FROM: " + fromSquare);
      Serial.println("🎯 TO: " + toSquare);
      Serial.println("🎮 MOVE: " + moveStr);
      Serial.println("🔘 Press BUTTON to CONFIRM and send to web");
      Serial.println("⏰ Or wait " + String(MOVE_TIMEOUT/1000) + " seconds for timeout");
      Serial.println("================================================");
      xSemaphoreGive(serialMutex);
    }
    
    // Log this change
    logSensorChange(row, col, true, currentTime);
  } else {
    // Piece placed without active move (normal piece placement)
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("📍 PIECE PLACED: " + square + " (no active move)");
      xSemaphoreGive(serialMutex);
    }
  }
}

void logSensorChange(byte row, byte col, bool state, unsigned long timestamp) {
  if (changeLogCount < 10) {
    changeLog[changeLogCount].row = row;
    changeLog[changeLogCount].col = col;
    changeLog[changeLogCount].state = state;
    changeLog[changeLogCount].timestamp = timestamp;
    changeLogCount++;
  }
}

void resetPendingMove() {
  pendingMove.isActive = false;
  pendingMove.pieceLifted = false;
  pendingMove.piecePlaced = false;
  pendingMove.readyToConfirm = false;
  changeLogCount = 0;
  digitalWrite(LED_PIN, HIGH);
}

void printBoardStatusTask() {
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    Serial.println("\n=== DUAL-CORE BOARD STATUS ===");
    
    if (sensorsDisabled) {
      Serial.println("🔘 SENSORS DISABLED - Press button to activate");
    } else {
      Serial.println("🎯 SENSOR DETECTION ACTIVE - Core 1");
    }
    
    // Show move status
    if (pendingMove.isActive) {
      String fromSquare = squareName(pendingMove.fromRow, pendingMove.fromCol);
      Serial.println("📋 ACTIVE MOVE:");
      Serial.println("   From: " + fromSquare);
      Serial.println("   Lifted: " + String(pendingMove.pieceLifted ? "✅" : "❌"));
      Serial.println("   Placed: " + String(pendingMove.piecePlaced ? "✅" : "❌"));
      if (pendingMove.readyToConfirm) {
        String toSquare = squareName(pendingMove.toRow, pendingMove.toCol);
        Serial.println("   To: " + toSquare);
        Serial.println("🔘 PRESS BUTTON TO CONFIRM MOVE!");
      } else {
        Serial.println("   Status: Waiting for piece placement...");
      }
    }
    
    for (byte row = 7; row != 255; row--) {
      Serial.print(row + 1);
      Serial.print(" | ");
      for (byte col = 0; col < 8; col++) {
        // Highlight active move squares
        char marker = sensors[row][col].stableState ? '.' : 'X';
        if (pendingMove.isActive) {
          if (row == pendingMove.fromRow && col == pendingMove.fromCol) {
            marker = 'F'; // From square
          } else if (pendingMove.piecePlaced && row == pendingMove.toRow && col == pendingMove.toCol) {
            marker = 'T'; // To square
          }
        }
        Serial.print(marker);
        Serial.print(" ");
      }
      Serial.println();
    }
    Serial.println("  +----------------");
    Serial.println("    a b c d e f g h");
    Serial.println("Legend: . = empty, X = piece, F = from, T = to");
    Serial.println("============================\n");
    xSemaphoreGive(serialMutex);
  }
}

// ==========================================
// 🔄 RESET BUTTON HANDLER - RESETS ESP32 + ARDUINO
// ==========================================
void checkResetButtonTask() {
  bool currentResetButtonState = digitalRead(RESET_BUTTON_PIN);
  
  // Reset button pressed (HIGH to LOW transition)
  if (lastResetButtonState == HIGH && currentResetButtonState == LOW) {
    unsigned long currentTime = millis();
    
    // Debounce reset button press
    if (currentTime - lastResetButtonPress > RESET_BUTTON_DEBOUNCE) {
      lastResetButtonPress = currentTime;
      
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println();
        Serial.println("🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴");
        Serial.println("🔄 RESET BUTTON PRESSED! 🔄");
        Serial.println("🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴");
        Serial.println("🤖 Sending RESET command to Arduino...");
        Serial.println("🔄 ESP32 will restart in 5 seconds...");
        Serial.println("🎮 WHOLE GAME RESET INITIATED!");
        Serial.println("🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴");
        xSemaphoreGive(serialMutex);
      }
      
      // Turn off all outputs before reset
      digitalWrite(MAGNET_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      
      // Send reset command multiple times to ensure Arduino receives it
      for (int i = 0; i < 5; i++) {
        ARDUINO_SERIAL.println("RESET_ARDUINO");
        ARDUINO_SERIAL.flush(); // Force send immediately
        delay(300); // 300ms between each command
        
        if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          Serial.printf("🤖 Reset command sent to Arduino (attempt %d/5)\n", i + 1);
          xSemaphoreGive(serialMutex);
        }
      }
      
      // Extra delay to ensure Arduino has time to process reset
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println("⏳ Waiting for Arduino to reset...");
        Serial.println("🔄 ESP32 will restart in 2 seconds...");
        xSemaphoreGive(serialMutex);
      }
      
      delay(2000); // Wait 2 more seconds for Arduino reset
      
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println("🔄 ESP32 RESTARTING NOW...");
        Serial.flush(); // Ensure message is sent
        xSemaphoreGive(serialMutex);
      }
      
      // Final flush and small delay before restart
      Serial.flush();
      delay(100);
      
      // Reset ESP32
      ESP.restart();
    }
  }
  
  lastResetButtonState = currentResetButtonState;
}

// ==========================================
// 🌐 WIFI CONNECTION MANAGEMENT - MEMORY CORRUPTION FIX
// ==========================================
void checkWiFiConnectionNonBlocking() {
  // MEMORY CORRUPTION FIX: Much less aggressive checking
  if (millis() - lastWiFiCheck >= wifiCheckInterval) {
    lastWiFiCheck = millis();
    
    // Only reconnect if truly disconnected AND not already reconnecting
    if (WiFi.status() != WL_CONNECTED && !wifiReconnecting) {
      Serial.println("❌ WiFi disconnected! Attempting gentle reconnection...");
      wifiReconnecting = true;
      networkStable = false; // Mark network as unstable during reconnection
      
      // MEMORY CORRUPTION FIX: Gentle reconnection - don't disconnect first
      WiFi.begin(ssid, password);
      Serial.println("🔄 Gentle reconnection started...");
      
      // Reset error counters
      consecutiveErrors = 0;
    }
  }
  
  // Check if reconnection completed (non-blocking)
  if (wifiReconnecting) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✅ WiFi Reconnected!");
      consecutiveErrors = 0;
      wifiReconnecting = false;
      networkStable = true; // Mark network as stable again
      
      // MEMORY CORRUPTION FIX: Small delay after reconnection
      delay(100);
    } else {
      // MEMORY CORRUPTION FIX: Timeout reconnection attempts to prevent infinite loops
      static unsigned long reconnectionStartTime = 0;
      if (reconnectionStartTime == 0) {
        reconnectionStartTime = millis();
      }
      
      // If reconnection takes more than 30 seconds, reset and try again
      if (millis() - reconnectionStartTime > 30000) {
        Serial.println("⚠️ Reconnection timeout, resetting WiFi...");
        WiFi.disconnect();
        delay(1000);
        WiFi.begin(ssid, password);
        reconnectionStartTime = millis();
      }
    }
  } else {
    // Reset reconnection timer when not reconnecting
    static unsigned long reconnectionStartTime = 0;
    reconnectionStartTime = 0;
  }
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
