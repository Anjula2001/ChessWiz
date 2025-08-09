/*
 * ESP32 Chess Board Bridge - Physical ↔ Web Communication
 * DUAL-CORE FREERTOS IMPLEMENTATION - RESTORED ORIGINAL LOGIC
 * 
 * 🎯 FUNCTION:
 * - Receives physical moves from hall sensors → Sends to web
 * - Receives web/AI moves from server → Sends to Arduino motors
 * 
 * 🔄 DUAL-CORE ARCHITECTURE:
 * - CORE 0: WiFi communications, HTTP requests, Arduino communication
 * - CORE 1: Physical sensor detection, button handling, real-time operations
 * 
 * 🎮 GAME MODE LOGIC:
 * - SINGLE PLAYER: Physical moves → BOTTOM player (web player = physical player)
 * - MULTIPLAYER: Physical moves → TOP player (physical = top, web = bottom)
 * 
 * 🔘 BUTTON WORKFLOW:
 * 1. Web move received → Sensors DISABLED automatically
 * 2. Arduino moves piece → Motors complete
 * 3. Physical player presses GPIO 19 button → Sensors ENABLED
 * 4. Physical player makes move → Sensors detect → Send to web
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

// Server configuration - CORRECTED IP
const char* serverIP = "192.168.170.94";  // Your computer's IP address
const char* getMovesURL = "http://192.168.170.94:3001/getAnyMove";        
const char* sendMoveURL = "http://192.168.170.94:3001/physicalMove";      

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

// Debouncing constants
const unsigned long DEBOUNCE_TIME = 30;     // 30ms debounce
const byte DEBOUNCE_THRESHOLD = 2;          // Need 2 consistent readings
const unsigned long MOVE_TIMEOUT = 10000;   // 10 second timeout for incomplete moves
const unsigned long SCAN_INTERVAL = 20;     // Scan every 20ms
const unsigned long PRINT_INTERVAL = 1000; // Print status every 1s

// Button variables - ORIGINAL LOGIC: Button enables sensors after motor moves
bool lastButtonState = HIGH;               // Button is pulled up
unsigned long lastButtonPress = 0;
const unsigned long BUTTON_DEBOUNCE = 200; // 200ms button debounce
bool sensorsDisabled = false;              // Sensors disabled when motors start, enabled by button

// Reset button variables
bool lastResetButtonState = HIGH;         
unsigned long lastResetButtonPress = 0;
const unsigned long RESET_BUTTON_DEBOUNCE = 300; 

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
unsigned long pollInterval = 5000; // Poll every 5 seconds
unsigned long lastSensorRead = 0;   
String lastReceivedMove = "";
String lastReceivedSource = "";  // Track the source of received moves
String webPlayerColor = "";      // Track the web player's chosen color
String lastSentMove = "";
int moveCount = 0;

// Connection stability
// WiFi check timing removed - continuous operation mode
// WiFi check interval removed - continuous operation without reconnection
int consecutiveErrors = 0;
const int maxConsecutiveErrors = 5; 
// WiFi reconnection feature removed - continuous operation prioritized
bool networkStable = true; 

// Arduino communication via standard UART pins (GPIO1/GPIO3)
#define ARDUINO_SERIAL Serial   // Use Serial for Arduino communication (GPIO1/3)
// Note: Debug output will be limited to avoid conflicts

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
  delayMicroseconds(300); // Allow channel to settle
}

void setup() {
  Serial.begin(115200);  
  
  // Using Serial for Arduino communication (GPIO1/3)
  delay(2000);  // Wait for Arduino to boot
  
  // Test Arduino communication
  Serial.println("ESP32_TEST");
  Serial.flush();
  
  // Wait for Arduino response
  delay(1000);
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
  
  Serial.println("🔄 Starting DUAL-CORE FreeRTOS Chess System...");
  Serial.println("📡 Core 0: WiFi communications & Arduino coordination - CONTINUOUS MODE");
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
  initializeSensors();
  
  Serial.println("Chess Board System Starting...");
  Serial.println("Initializing sensors... Please wait 3 seconds.");
  
  // Calibration phase
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
  
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // Disabled for continuous operation
  WiFi.persistent(true);
  WiFi.setSleep(false); // Disable WiFi sleep
  
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
    Serial.println("📡 Polling every 5 seconds...");
    Serial.println("👁️ Monitoring hall sensors continuously...");
    Serial.println("🔘 Push button on GPIO19 to activate sensor detection after motor moves...");
    Serial.println("🔄 Push button on GPIO18 to RESET whole game (ESP32 + Arduino)...");
    Serial.println("🤖 Arduino communication on default Serial (GPIO1/GPIO3) at 115200 baud");
    
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
  vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for 1 second
}

// ==========================================
// 🌐 CORE 0 TASK: WiFi Communications & Arduino
// ==========================================
void wifiTask(void *parameter) {
  // Initialize WiFi on Core 0
  connectToWiFi();
  
  unsigned long lastPollTime = 0;
  
  Serial.println("📡 Core 0: WiFi task started - Continuous operation mode");
  
  while (true) {
    // Poll for web/AI moves only if connected
    if (WiFi.status() == WL_CONNECTED) {
      if (millis() - lastPollTime >= pollInterval) {
        checkForWebMovesTask();
        lastPollTime = millis();
      }
    }
    
    // Handle physical moves from Core 1 - ALWAYS AVAILABLE
    PhysicalMoveMsg physicalMove;
    if (xQueueReceive(physicalMoveQueue, &physicalMove, 0) == pdTRUE) {
      sendPhysicalMoveTask(String(physicalMove.move));
    }
    
    // Handle Arduino communication - ALWAYS AVAILABLE
    handleArduinoCommunicationTask();
    
    vTaskDelay(pdMS_TO_TICKS(50)); // 50ms delay
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
    
    vTaskDelay(pdMS_TO_TICKS(10));
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
  
  if (!http.begin(client, getMovesURL)) {
    Serial.println("❌ HTTP begin failed - skipping poll");
    return;
  }
  
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(1500); 
  
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
      networkStable = false;
      if (consecutiveErrors >= maxConsecutiveErrors + 10) {
        consecutiveErrors = 0;
        networkStable = true;
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
  http.setTimeout(1500);
  
  // GAME MODE LOGIC - RESTORED ORIGINAL REQUIREMENTS
  String targetPlayerSide;
  String targetRoomId;
  String targetPlayerInfo;
  
  if (lastReceivedSource == "ai" || lastReceivedSource == "singleplayer-default") {
    // SINGLE PLAYER MODE: Physical moves → BOTTOM player
    targetRoomId = "singleplayer-default";
    targetPlayerSide = webPlayerColor.length() > 0 ? webPlayerColor : "white";
    String upperPlayerSide = targetPlayerSide;
    upperPlayerSide.toUpperCase();
    targetPlayerInfo = "BOTTOM (" + upperPlayerSide + ") - Single Player";
  } else {
    // MULTIPLAYER MODE: Physical moves → TOP player
    targetRoomId = "default";
    if (webPlayerColor == "white") {
      targetPlayerSide = "black"; // Physical is opposite of web player
      targetPlayerInfo = "TOP (BLACK) - Multiplayer";
    } else {
      targetPlayerSide = "white";
      targetPlayerInfo = "TOP (WHITE) - Multiplayer";
    }
  }
  
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
  
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    Serial.println("================================================");
    Serial.println("📤 SENDING PHYSICAL MOVE TO WEB");
    Serial.println("================================================");
    Serial.println("🎯 Move: " + move);
    Serial.println("🎮 Game Mode: " + lastReceivedSource);
    Serial.println("🎨 Web Player Color: " + webPlayerColor);
    Serial.println("👤 Physical Player: " + targetPlayerInfo);
    Serial.print("📤 URL: ");
    Serial.println(sendMoveURL);
    Serial.print("📤 Payload: ");
    Serial.println(payload);
    xSemaphoreGive(serialMutex);
  }
  
  int httpResponseCode = http.POST(payload);
  
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    Serial.print("📥 HTTP Response Code: ");
    Serial.println(httpResponseCode);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      
      if (response.length() > 500) {
        response = response.substring(0, 500);
      }
      
      Serial.print("📥 Server Response: ");
      Serial.println(response);
      
      if (httpResponseCode == 200) {
        Serial.println("✅ Physical move sent successfully to web!");
        Serial.println("🎯 " + targetPlayerInfo + " should see move on web interface");
        lastSentMove = move;
        
        // LED feedback
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
      consecutiveErrors++;
      if (consecutiveErrors >= 3) {
        networkStable = false;
      }
    }
    Serial.println("================================================");
    xSemaphoreGive(serialMutex);
  }
  
  http.end();
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
      arduinoMsg.message[sizeof(arduinoMsg.message) - 1] = '\0';
      xQueueSend(arduinoMsgQueue, &arduinoMsg, 0);
      
      // Handle magnet control on Core 0 (immediate response)
      if (msg == "MAGNET_ON") {
        digitalWrite(MAGNET_PIN, HIGH);
        ARDUINO_SERIAL.println("MAGNET_READY");
      } else if (msg == "MAGNET_OFF") {
        digitalWrite(MAGNET_PIN, LOW);
        ARDUINO_SERIAL.println("MAGNET_OFF_READY");
      }
    }
  }
}

// ==========================================
// 🎯 CORE 1 TASK FUNCTIONS: Sensor & Button Handling
// ==========================================

void processWebMoveTask(String move, String source) {
  // ORIGINAL LOGIC: Automatically disable sensors when web move received
  sensorsDisabled = true;
  
  // Send move to Arduino immediately (minimal debug to avoid conflicts)
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    // Send move to Arduino via Serial (GPIO1/3)
    ARDUINO_SERIAL.println(move);
    ARDUINO_SERIAL.flush();
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
      Serial.println("🔘 Press BUTTON on GPIO19 to re-enable sensor detection");
    } else if (msg == "MAGNET_ON") {
      digitalWrite(MAGNET_PIN, HIGH);
      Serial.println("🧲 Magnet ON - Arduino request");
      ARDUINO_SERIAL.println("MAGNET_READY");
    } else if (msg == "MAGNET_OFF") {
      digitalWrite(MAGNET_PIN, LOW);
      Serial.println("🧲 Magnet OFF - Arduino request");
      ARDUINO_SERIAL.println("MAGNET_OFF_READY");
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
                  Serial.println("🎯 PHYSICAL MOVE: " + moveStr + " → Sent to Core 0 immediately");
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
        delay(300);
        
        // Send alternative reset commands
        ARDUINO_SERIAL.println("SYSTEM_RESET");
        ARDUINO_SERIAL.flush();
        delay(200);
        
        ARDUINO_SERIAL.println("RESTART");
        ARDUINO_SERIAL.flush();
        delay(200);
      }
      
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println("📤 Serial reset commands sent successfully");
        Serial.println("⏳ Waiting for Arduino to restart...");
        xSemaphoreGive(serialMutex);
      }
      
      // Wait for Arduino to process reset and restart
      delay(2000);
      
      // Test communication after reset
      ARDUINO_SERIAL.println("ESP32_TEST");
      ARDUINO_SERIAL.flush();
      delay(500);
      
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
      delay(100);
      digitalWrite(ARDUINO_RESET_PIN, HIGH);  // Release reset
      delay(500);
      
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println("🔄 ESP32 will restart in 2 seconds...");
        xSemaphoreGive(serialMutex);
      }
      
      delay(2000);
      
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
