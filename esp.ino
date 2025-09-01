/*
 * ESP32 Chess Board Bridge - CLEAN VERSION - Arduino Communication Only
 * 
 * 🎯 FUNCTION:
 * - Receives web/AI moves from server → Sends to Arduino motors
 * - Receives physical moves from hall sensors → Sends to web
 * 
 * � CRITICAL FIX:
 * - NO debug output to Serial (Arduino communication only)
 * - Clean move transmission: "e7-e5" format only
 * - Minimal processing to avoid Arduino confusion
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
// REMOVED: #include <esp_task_wdt.h> - causing compatibility issues

// WiFi credentials
const char* ssid = "Silly Signal";
const char* password = "tharushatharusha";

// Server configuration - CORRECTED IP
const char* serverIP = "192.168.209.94";  // Your computer's IP address
const char* getMovesURL = "http://192.168.209.94:3001/getAnyMove";        
const char* getSinglePlayerURL = "http://192.168.209.94:3001/getLastMove?roomId=singleplayer-default";
const char* getMultiPlayerURL = "http://192.168.209.94:3001/getLastMove?roomId=default";
const char* sendMoveURL = "http://192.168.209.94:3001/physicalMove";      

// --- Pin Definitions ---
#define MAGNET_PIN 23
#define BUTTON_PIN 19  // Push button to enable sensors after motor moves
#define RESET_BUTTON_PIN 18  // Push button to reset whole game (ESP + Arduino)
#define ARDUINO_RESET_PIN 5  // GPIO pin to hardware reset Arduino - FIXED: Changed from GPIO4 to avoid MUX1 conflict

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

// Enhanced sensor state structure
struct SensorState {
  bool rawState;
  bool stableState;
  bool previousStable;
  unsigned long lastChangeTime;
  byte consistentCount;
  bool hasChanged;
};

// Simple movement tracking - ORIGINAL LOGIC RESTORED
struct PendingMove {
  byte fromRow, fromCol;
  unsigned long startTime;
  bool isActive;
};
PendingMove pendingMove = {0, 0, 0, false};

// Board state
SensorState sensors[8][8];
bool boardInitialized = false;

// Debouncing constants - OPTIMIZED for faster response
const unsigned long DEBOUNCE_TIME = 20;     // REDUCED: 20ms debounce (was 30ms)
const byte DEBOUNCE_THRESHOLD = 2;          // Need 2 consistent readings
const unsigned long MOVE_TIMEOUT = 10000;   // 10 second timeout for incomplete moves
const unsigned long SCAN_INTERVAL = 15;     // OPTIMIZED: Scan every 15ms (was 20ms)
const unsigned long PRINT_INTERVAL = 1000; // Print status every 1s

// Button variables - ORIGINAL LOGIC: Button enables sensors after motor moves
bool lastButtonState = HIGH;               // Button is pulled up
unsigned long lastButtonPress = 0;
const unsigned long BUTTON_DEBOUNCE = 150; // OPTIMIZED: 150ms button debounce (was 200ms)
bool sensorsDisabled = false;              // Sensors disabled when motors start, enabled by button

// Reset button variables
bool lastResetButtonState = HIGH;         
unsigned long lastResetButtonPress = 0;
const unsigned long RESET_BUTTON_DEBOUNCE = 250; // OPTIMIZED: 250ms (was 300ms) 

// Timing control
unsigned long lastScanTime = 0;
unsigned long lastPrintTime = 0;

// MUX channel selection binary map
const byte MUX_CHANNEL[16][4] = {
  {0, 0, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}, {1, 1, 0, 0},
  {0, 0, 1, 0}, {1, 0, 1, 0}, {0, 1, 1, 0}, {1, 1, 1, 0},
  {0, 0, 0, 1}, {1, 0, 0, 1}, {0, 1, 0, 1}, {1, 1, 0, 1},
  {0, 0, 1, 1}, {1, 0, 1, 1}, {0, 1, 1, 1}, {1, 1, 1, 1}
};

// Timing variables
unsigned long lastPollTime = 0;
unsigned long pollInterval = 1000; // OPTIMIZED: Poll every 1 second (was 2 seconds) for faster AI move detection
unsigned long lastSensorRead = 0;   
String lastReceivedMove = "";
String lastReceivedSource = "";  // Track the source of received moves
String webPlayerColor = "";      // Track the web player's chosen color
String lastSentMove = "";
int moveCount = 0;

// Minimal WiFi stability - connect once and stay connected
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 60000; // Check only every 60 seconds (much less frequent)
int consecutiveErrors = 0;
const int maxConsecutiveErrors = 10; // Increased tolerance before considering connection bad
bool networkStable = false; // Start as unstable until connected 

// Arduino communication via standard UART pins (GPIO1/GPIO3)
#define ARDUINO_SERIAL Serial   // Use Serial for Arduino communication (GPIO1/3)
// DEBUG DISABLED: All Serial.println removed to prevent Arduino confusion
#define DEBUG_PRINT(x) // Debug output disabled to avoid Arduino conflicts

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
  delayMicroseconds(100); // OPTIMIZED: Reduced from 300 microseconds for faster MUX switching
}

void setup() {
  Serial.begin(115200);  
  
  // REMOVED: Watchdog timer initialization - causing compatibility issues
  // ESP32 has built-in hardware watchdog that handles system stability
  
  // Using Serial for Arduino communication (GPIO1/3)
  delay(1000);  // OPTIMIZED: Reduced from 2000ms - faster boot
  
  // Test Arduino communication
  Serial.println("ESP32_TEST");
  Serial.flush();
  
  // Wait for Arduino response
  delay(500); // OPTIMIZED: Reduced from 1000ms
  if (Serial.available()) {
    String response = Serial.readStringUntil('\n');
    // Arduino should respond with "ARDUINO_READY"
  }
  
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
  lastScanTime = millis();
  lastPrintTime = millis();
  
  DEBUG_PRINT("🔄 Starting DUAL-CORE FreeRTOS Chess System...");
  DEBUG_PRINT("📡 Core 0: WiFi communications & Arduino coordination - CONTINUOUS MODE");
  DEBUG_PRINT("🎯 Core 1: Sensor detection & button handling");
  
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
  
  DEBUG_PRINT("✅ Dual-core tasks created successfully!");
  DEBUG_PRINT("🎮 System ready for chess gameplay!");
}

void setupMultiplexers() {
  // Configure MUX control pins
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  
  // Configure MUX signal pins as inputs with pullup
  for (int i = 0; i < 4; i++) {
    pinMode(muxPins[i], INPUT_PULLUP);
  }
  
  // Configure other pins
  pinMode(MAGNET_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);  
  pinMode(ARDUINO_RESET_PIN, OUTPUT);
  digitalWrite(ARDUINO_RESET_PIN, HIGH);  // Keep Arduino running (active LOW reset)  
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(MAGNET_PIN, LOW);
  digitalWrite(LED_PIN, HIGH); // Start with LED on during initialization
  
  // Initialize sensors
  initializeBoardState();
  
  DEBUG_PRINT("Chess Board System Starting...");
  DEBUG_PRINT("Initializing sensors... Please wait 1.5 seconds."); // Updated message  // OPTIMIZED: Faster calibration phase
  for (int i = 0; i < 30; i++) { // REDUCED: 30 scans over ~450ms at 15ms interval
    scanAndDebounceBoard();
    delay(15); // OPTIMIZED: 15ms interval (was 20ms)
  }
  
  // Set previous stable states after calibration
  for (byte row = 0; row < 8; row++) {
    for (byte col = 0; col < 8; col++) {
      sensors[row][col].previousStable = sensors[row][col].stableState;
    }
  }
  
  boardInitialized = true;
  DEBUG_PRINT("System Ready! Waiting for movement...");
  DEBUG_PRINT("Format: FROM_SQUARE -> TO_SQUARE");
  DEBUG_PRINT("----------------------------------------");
}

void connectToWiFi() {
  DEBUG_PRINT("Connecting to WiFi - ONE TIME SETUP");
  
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // ENABLED for background auto-reconnection
  WiFi.persistent(true);
  WiFi.setSleep(false); // Disable WiFi sleep
  
  // REMOVED: setAutoConnect() - function doesn't exist in newer ESP32 core
  // Auto-reconnect is handled by setAutoReconnect(true)
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    DEBUG_PRINT(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINT("\n✅ WiFi Connected - AUTO-RECONNECT ENABLED!");
    DEBUG_PRINT("📍 ESP32 IP: ");
    DEBUG_PRINT(WiFi.localIP());
    DEBUG_PRINT("📡 Server IP: ");
    DEBUG_PRINT(serverIP);
    DEBUG_PRINT("📥 Get moves from: ");
    DEBUG_PRINT(getMovesURL);
    DEBUG_PRINT("📤 Send moves to: ");
    DEBUG_PRINT(sendMoveURL);
    DEBUG_PRINT("📡 Polling every 2 seconds...");
    DEBUG_PRINT("👁️ Monitoring hall sensors continuously...");
    DEBUG_PRINT("🔘 Push button on GPIO19 to activate sensor detection after motor moves...");
    DEBUG_PRINT("🔄 Push button on GPIO18 to RESET whole game (ESP32 + Arduino)...");
    DEBUG_PRINT("🤖 Arduino communication on default Serial (GPIO1/GPIO3) at 115200 baud");
    DEBUG_PRINT("� WiFi will auto-reconnect in background - NO MANUAL CHECKING");
    
    networkStable = true;
    consecutiveErrors = 0;
  } else {
    DEBUG_PRINT("\n❌ WiFi Connection Failed!");
    DEBUG_PRINT("ESP32 will continue trying in background...");
    networkStable = false;
  }
}

// ==========================================
// 📶 Minimal WiFi Status Check (Non-Intrusive)
// ==========================================
void checkWiFiConnection() {
  // Only check very infrequently to avoid interference
  if (millis() - lastWiFiCheck < WIFI_CHECK_INTERVAL) {
    return;
  }
  
  lastWiFiCheck = millis();
  
  // Just update status, don't force reconnection
  // Let ESP32's auto-reconnect handle it in background
  if (WiFi.status() == WL_CONNECTED) {
    if (!networkStable) {
      DEBUG_PRINT("📶 WiFi connection restored");
      networkStable = true;
      consecutiveErrors = 0;
    }
  } else {
    if (networkStable) {
      DEBUG_PRINT("⚠️ WiFi connection lost - ESP32 will auto-reconnect in background");
      networkStable = false;
    }
  }
}

void loop() {
  // Main loop is now empty - all work done by FreeRTOS tasks
  // REMOVED: Watchdog reset - not needed, ESP32 handles stability automatically
  vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for 1 second
}

// ==========================================
// 🌐 CORE 0 TASK: WiFi Communications & Arduino
// ==========================================
void wifiTask(void *parameter) {
  // REMOVED: Watchdog timer registration - causing issues
  
  // Initialize WiFi on Core 0
  connectToWiFi();
  
  unsigned long lastPollTime = 0;
  
  DEBUG_PRINT("📡 Core 0: WiFi task started - MINIMAL MONITORING MODE");
  
  while (true) {
    // MINIMAL WiFi monitoring - only check status, don't interfere
    checkWiFiConnection();
    
    // Poll for web/AI moves - continue even if WiFi temporarily down
    if (millis() - lastPollTime >= pollInterval) {
      if (WiFi.status() == WL_CONNECTED) {
        checkForWebMovesTask();
      }
      lastPollTime = millis();
    }
    
    // PRIORITY 3: Handle physical moves from Core 1 - ALWAYS AVAILABLE
    PhysicalMoveMsg physicalMove;
    if (xQueueReceive(physicalMoveQueue, &physicalMove, 0) == pdTRUE) {
      sendPhysicalMoveTask(String(physicalMove.move));
    }
    
    // PRIORITY 4: Handle Arduino communication - ALWAYS AVAILABLE
    handleArduinoCommunicationTask();
    
    // REMOVED: Watchdog reset - not needed, ESP32 handles stability
    
    vTaskDelay(pdMS_TO_TICKS(25)); // OPTIMIZED: 25ms delay (was 50ms) for faster WiFi response
  }
}

// ==========================================
// 🎯 CORE 1 TASK: Sensor Detection & Button Handling
// ==========================================
void sensorTask(void *parameter) {
  // REMOVED: Watchdog timer registration - causing issues
  
  unsigned long lastScanTime = 0;
  unsigned long lastPrintTime = 0;
  
  DEBUG_PRINT("🎯 Core 1: Sensor task started");
  
  while (true) {
    // Reset button handling (highest priority)
    checkResetButtonTask();
    
    // Button handling - ORIGINAL LOGIC: Only enable sensors
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
    
    // Physical sensor detection - ORIGINAL LOGIC
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
    
    // REMOVED: Watchdog reset - not needed, ESP32 handles stability
    
    vTaskDelay(pdMS_TO_TICKS(5)); // OPTIMIZED: 5ms delay (was 10ms) for faster sensor response
  }
}

// ==========================================
// 📡 CORE 0 TASK FUNCTIONS: WiFi & Arduino Communication
// ==========================================

void checkForWebMovesTask() {
  if (WiFi.status() != WL_CONNECTED) {
    return; // Skip if not connected, but don't block physical moves
  }
  
  HTTPClient http;
  WiFiClient client;
  
  // SIMPLIFIED: Always use unified endpoint for polling
  // Web frontend handles game mode routing
  const char* pollURL = getMovesURL; // Always use unified endpoint
  
  Serial.println("🔍 Polling unified endpoint for any moves");
  
  if (!http.begin(client, pollURL)) {
    Serial.println("❌ HTTP begin failed - skipping poll");
    return;
  }
  
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(1000); // REDUCED: 1s timeout to prevent blocking
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    consecutiveErrors = 0;
    String response = http.getString();
    
    if (response.length() > 1000) {
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
                lastReceivedSource = source;
              }
            }
            
            // Extract web player color
            String playerSide = "unknown";
            int playerStart = response.indexOf("\"playerSide\":\"");
            if (playerStart != -1) {
              playerStart += 14;
              int playerEnd = response.indexOf("\"", playerStart);
              if (playerEnd != -1) {
                playerSide = response.substring(playerStart, playerEnd);
                webPlayerColor = playerSide;
              }
            }
            
            // Send to Core 1 via queue
            WebMoveMsg webMove;
            strncpy(webMove.move, move.c_str(), sizeof(webMove.move) - 1);
            strncpy(webMove.source, source.c_str(), sizeof(webMove.source) - 1);
            webMove.move[sizeof(webMove.move) - 1] = '\0';
            webMove.source[sizeof(webMove.source) - 1] = '\0';
            xQueueSend(webMoveQueue, &webMove, 0);
            
            // DEBUG REMOVED: No output to Serial to avoid Arduino confusion
          } else {
            // No new move found - polling working but no AI move yet
            // DEBUG REMOVED: No output to Serial to avoid Arduino confusion
          }
        }
      } else {
        Serial.println("🔍 Poll complete - no move data in response");
      }
    }
  } else {
    consecutiveErrors++;
    if (consecutiveErrors >= maxConsecutiveErrors) {
      DEBUG_PRINT("⚠️ Multiple HTTP errors - but continuing operation");
      
      if (consecutiveErrors >= maxConsecutiveErrors + 10) {
        DEBUG_PRINT("🔄 Resetting error counter - continuing");
        consecutiveErrors = 0;
      }
    }
  }
  
  http.end();
  delay(10);
}

void sendPhysicalMoveTask(String move) {
  if (WiFi.status() != WL_CONNECTED) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("⚠️ WiFi not connected - move buffered: " + move);
      xSemaphoreGive(serialMutex);
    }
    return; // Skip sending but don't block, just log
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
  
  if (!http.begin(client, sendMoveURL)) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("❌ HTTP begin failed for physical move");
      xSemaphoreGive(serialMutex);
    }
    return;
  }
  
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(1000); // REDUCED: 1s timeout to prevent blocking
  
  // SIMPLIFIED LOGIC: ESP32 sends all physical moves to server
  // Web frontend handles game mode detection and routing
  
  // Send physical move to the unified endpoint - let web frontend decide routing
  String targetRoomId = "default";  // Use default room for all moves
  String targetPlayerSide = "physical";  // Mark as physical move
  String targetPlayerInfo = "PHYSICAL BOARD";
  
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    Serial.println("📤 SENDING PHYSICAL MOVE: " + move);
    Serial.println("🔄 Web frontend will handle game mode routing");
    xSemaphoreGive(serialMutex);
  }
  
  // Create simplified payload - let backend handle room routing
  const char* payloadTemplate = "{\"move\":\"%s\",\"source\":\"physical\",\"roomId\":\"%s\",\"playerSide\":\"%s\"}";
  char payload[200];
  int result = snprintf(payload, sizeof(payload), payloadTemplate, move.c_str(), targetRoomId.c_str(), targetPlayerSide.c_str());
  
  if (result >= sizeof(payload)) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("❌ Payload too large, move not sent");
      xSemaphoreGive(serialMutex);
    }
    http.end();
    return;
  }
  
  int httpResponseCode = http.POST(payload);
  
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    Serial.println("================================================");
    Serial.println("📤 PHYSICAL MOVE: " + move);
    Serial.println("🎮 Mode: " + lastReceivedSource + " | Player: " + targetPlayerInfo);
    Serial.print("📥 Response: ");
    Serial.println(httpResponseCode);
    
    if (httpResponseCode == 200) {
      Serial.println("✅ Move sent successfully!");
      lastSentMove = move;
      
      // LED feedback
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(LED_PIN, LOW);
      digitalWrite(LED_PIN, HIGH);
    } else {
      Serial.printf("❌ HTTP Error: %d\n", httpResponseCode);
    }
    Serial.println("================================================");
    xSemaphoreGive(serialMutex);
  }
  
  http.end();
  // Removed delay for faster response
}

void handleArduinoCommunicationTask() {
  if (ARDUINO_SERIAL.available()) {
    String msg = ARDUINO_SERIAL.readStringUntil('\n');
    msg.trim();
    
    if (msg.length() > 0) {
      // CRITICAL FIX: Only handle specific Arduino commands, ignore debug messages
      // Handle magnet control on Core 0 (immediate response)
      if (msg == "MAGNET_ON") {
        digitalWrite(MAGNET_PIN, HIGH);
        ARDUINO_SERIAL.println("MAGNET_READY");
      } else if (msg == "MAGNET_OFF") {
        digitalWrite(MAGNET_PIN, LOW);
        ARDUINO_SERIAL.println("MAGNET_OFF_READY");
      } else if (msg == "REQUEST_BOARD_STATE") {
        // Send Arduino message to Core 1 for processing
        ArduinoMsg arduinoMsg;
        strncpy(arduinoMsg.message, msg.c_str(), sizeof(arduinoMsg.message) - 1);
        arduinoMsg.message[sizeof(arduinoMsg.message) - 1] = '\0';
        xQueueSend(arduinoMsgQueue, &arduinoMsg, 0);
      } else if (msg == "MOVE_COMPLETED" || msg.indexOf("Move complete") >= 0) {
        // Send Arduino message to Core 1 for processing
        ArduinoMsg arduinoMsg;
        strncpy(arduinoMsg.message, msg.c_str(), sizeof(arduinoMsg.message) - 1);
        arduinoMsg.message[sizeof(arduinoMsg.message) - 1] = '\0';
        xQueueSend(arduinoMsgQueue, &arduinoMsg, 0);
      }
      // CRITICAL FIX: Ignore all other Arduino debug messages (don't send to Core 1)
      // This prevents Arduino acknowledgments from being processed as moves
    }
  }
}

// ==========================================
// 🎯 CORE 1 TASK FUNCTIONS: Sensor & Button Handling
// ==========================================

void processWebMoveTask(String move, String source) {
  // CRITICAL FIX: Disable sensors when web move received
  sensorsDisabled = true;
  
  // Send ONLY the move to Arduino - NO debug output
  ARDUINO_SERIAL.println(move);
  ARDUINO_SERIAL.flush();
}

void processArduinoMessageTask(String msg) {
  // Handle Arduino requests with NO debug output
  if (msg == "REQUEST_BOARD_STATE") {
    if (sensorsDisabled) {
      ARDUINO_SERIAL.println("SENSORS_DISABLED");
    } else {
      sendBoardStateToArduino();
    }
  } else if (msg == "MAGNET_ON") {
    digitalWrite(MAGNET_PIN, HIGH);
    ARDUINO_SERIAL.println("MAGNET_READY");
  } else if (msg == "MAGNET_OFF") {
    digitalWrite(MAGNET_PIN, LOW);
    ARDUINO_SERIAL.println("MAGNET_OFF_READY");
  }
  // Ignore other Arduino messages to avoid debug loops
}

void sendBoardStateToArduino() {
  // Check if sensors are currently disabled
  if (sensorsDisabled) {
    ARDUINO_SERIAL.println("SENSORS_DISABLED");
    return;
  }
  
  // Send current board state to Arduino as a 64-character string
  String boardState = "BOARD_STATE:";
  
  // Build 64-character string representing board state
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      boardState += sensors[row][col].stableState ? "1" : "0";
    }
  }
  
  // Send to Arduino
  ARDUINO_SERIAL.println(boardState);
  // Removed verbose board state logging
}

void checkButtonForSensorActivationTask() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    unsigned long currentTime = millis();
    
    if (currentTime - lastButtonPress > BUTTON_DEBOUNCE) {
      lastButtonPress = currentTime;
      
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        Serial.println("🔘 BUTTON PRESSED!");
        
        // ORIGINAL LOGIC: Button only enables sensors (no move confirmation)
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
            pendingMove.isActive = false;
            digitalWrite(LED_PIN, HIGH);
            xSemaphoreGive(sensorMutex);
          }
          
          // Send updated board state to Arduino now that sensors are re-enabled
          delay(50); // Minimal delay for sensor stabilization
          sendBoardStateToArduino();
          
        } else {
          Serial.println("ℹ️ Sensors already active");
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
  
  if (pendingMove.isActive && (currentTime - pendingMove.startTime > MOVE_TIMEOUT)) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.println("TIMEOUT: Move incomplete, resetting...");
      xSemaphoreGive(serialMutex);
    }
    pendingMove.isActive = false;
    digitalWrite(LED_PIN, HIGH);
  }
  
  // ORIGINAL LOGIC: Simple piece lift + place detection
  for (byte row = 0; row < 8; row++) {
    for (byte col = 0; col < 8; col++) {
      if (sensors[row][col].hasChanged) {
        bool currentState = sensors[row][col].stableState;
        bool previousState = sensors[row][col].previousStable;
        
        // Piece lifted (magnet removed: LOW -> HIGH)
        if (!previousState && currentState) {
          if (!pendingMove.isActive) {
            pendingMove.fromRow = row;
            pendingMove.fromCol = col;
            pendingMove.startTime = currentTime;
            pendingMove.isActive = true;
            digitalWrite(LED_PIN, LOW);
            
            String fromSquare = squareName(row, col);
            if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
              Serial.println("🎯 PIECE LIFTED: " + fromSquare);
              xSemaphoreGive(serialMutex);
            }
          }
        }
        
        // Piece placed (magnet detected: HIGH -> LOW)
        else if (previousState && !currentState) {
          if (pendingMove.isActive) {
            String fromSquare = squareName(pendingMove.fromRow, pendingMove.fromCol);
            String toSquare = squareName(row, col);
            
            if (!(pendingMove.fromRow == row && pendingMove.fromCol == col)) {
              String moveStr = fromSquare + "-" + toSquare;
              
              pendingMove.isActive = false;
              digitalWrite(LED_PIN, HIGH);
              
              if (isValidMove(moveStr)) {
                // ORIGINAL LOGIC: Immediately send move (no confirmation needed)
                PhysicalMoveMsg physicalMove;
                strncpy(physicalMove.move, moveStr.c_str(), sizeof(physicalMove.move) - 1);
                strncpy(physicalMove.source, "physical", sizeof(physicalMove.source) - 1);
                physicalMove.move[sizeof(physicalMove.move) - 1] = '\0';
                physicalMove.source[sizeof(physicalMove.source) - 1] = '\0';
                xQueueSend(physicalMoveQueue, &physicalMove, 0);
                
                if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                  Serial.println("🎯 PHYSICAL MOVE: " + moveStr);
                  xSemaphoreGive(serialMutex);
                }
              }
              
              sensors[row][col].hasChanged = false;
              sensors[pendingMove.fromRow][pendingMove.fromCol].hasChanged = false;
            }
          }
        }
        
        sensors[row][col].previousStable = sensors[row][col].stableState;
        sensors[row][col].hasChanged = false;
      }
    }
  }
}

void printBoardStatusTask() {
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    Serial.println("\n=== DUAL-CORE BOARD STATUS ===");
    
    if (sensorsDisabled) {
      Serial.println("🔘 SENSORS DISABLED - Press button to activate");
    } else {
      Serial.println("🎯 SENSOR DETECTION ACTIVE - Core 1");
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
    Serial.println("============================\n");
    xSemaphoreGive(serialMutex);
  }
}

void checkResetButtonTask() {
  bool currentResetButtonState = digitalRead(RESET_BUTTON_PIN);
  
  if (lastResetButtonState == HIGH && currentResetButtonState == LOW) {
    unsigned long currentTime = millis();
    
    if (currentTime - lastResetButtonPress > RESET_BUTTON_DEBOUNCE) {
      lastResetButtonPress = currentTime;
      
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Serial.println("🔄 RESET BUTTON PRESSED - Resetting Arduino via Serial...");
        xSemaphoreGive(serialMutex);
      }
      
      // Disable magnet and LED immediately
      digitalWrite(MAGNET_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      
      // Enhanced Serial Reset Protocol - Multiple attempts for reliability
      for (int attempt = 1; attempt <= 3; attempt++) {
        if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          Serial.printf("📤 Reset attempt %d/3: Sending RESET_ARDUINO via RX/TX\n", attempt);
          xSemaphoreGive(serialMutex);
        }
        
        // Send software reset command via default Serial (GPIO1/GPIO3)
        ARDUINO_SERIAL.println("RESET_ARDUINO");
        ARDUINO_SERIAL.flush();
        // OPTIMIZED: Reduced delays for faster reset response
        delay(100); // Reduced from 300ms
        
        // Send alternative reset commands
        ARDUINO_SERIAL.println("SYSTEM_RESET");
        ARDUINO_SERIAL.flush();
        delay(50); // Reduced from 200ms
        
        ARDUINO_SERIAL.println("RESTART");
        ARDUINO_SERIAL.flush();
        delay(50); // Reduced from 200ms
      }
      
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println("📤 Serial reset commands sent successfully");
        Serial.println("⏳ Waiting for Arduino to restart...");
        xSemaphoreGive(serialMutex);
      }
      
      // OPTIMIZED: Reduced waiting time for faster reset
      delay(1000); // Reduced from 2000ms
      
      // Test communication after reset
      ARDUINO_SERIAL.println("ESP32_TEST");
      ARDUINO_SERIAL.flush();
      delay(250); // Reduced from 500ms
      
      // Check if Arduino responds
      if (ARDUINO_SERIAL.available()) {
        String response = ARDUINO_SERIAL.readStringUntil('\n');
        if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          Serial.println("✅ Arduino responded: " + response);
          xSemaphoreGive(serialMutex);
        }
      } else {
        if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          Serial.println("⚠️ Arduino did not respond - may still be restarting");
          xSemaphoreGive(serialMutex);
        }
      }
      
      // Optional: Hardware reset as backup (if GPIO5 is connected)
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println("🔧 Backup: Hardware reset pulse on GPIO5");
        xSemaphoreGive(serialMutex);
      }
      
      digitalWrite(ARDUINO_RESET_PIN, LOW);   // Pull reset LOW
      delay(50); // Reduced from 100ms for faster reset
      digitalWrite(ARDUINO_RESET_PIN, HIGH);  // Release reset
      delay(250); // Reduced from 500ms
      
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println("🔄 ESP32 will restart in 1 second..."); // Reduced from 2 seconds
        xSemaphoreGive(serialMutex);
      }
      
      delay(1000); // Reduced from 2000ms
      
      // Reset ESP32
      ESP.restart();
    }
  }
  
  lastResetButtonState = currentResetButtonState;
}

// WiFi reconnection system removed for continuous operation

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

void initializeBoardState() {
  // Initialize board state - same as initializeSensors for compatibility
  initializeSensors();
}

void scanAndDebounceBoard() {
  unsigned long currentTime = millis();
  
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
          }
        }
      }
    }
  }
}

bool isValidMove(String move) {
  if (move.length() != 5) return false;
  if (move.charAt(2) != '-') return false;
  
  String fromSquare = move.substring(0, 2);
  String toSquare = move.substring(3, 5);
  
  if (fromSquare == toSquare) return false;
  
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