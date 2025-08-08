/*
 * ESP32/ESP8266 Chess Move Sender
 * For Chesswizzz Physical Chess Integration
 * 
 * This sketch connects to WiFi and sends chess moves to the Chesswizzz backend server.
 * It demonstrates how to implement a simple physical chess interface
 * that can communicate with the web application.
 */

#include <Arduino.h>

// Choose your ESP board type
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
  #include <WiFiClient.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <HTTPClient.h>
  #include <WiFiClient.h>
#else
  #error "This sketch works only with ESP8266 or ESP32 boards"
#endif

// WiFi credentials - Replace with your network information
const char* ssid = "POCO X3 NFC";
const char* password = "123456789";

// Server configuration
const char* serverUrl = "http://192.168.45.94:3001/physicalMove";
String pollingUrl = "http://192.168.45.94:3001/getLastMove"; // New endpoint for polling moves
String roomId = "default"; // Game room ID (if using multiple boards)

// Pin definitions for demonstration
// In a real chess board, you might use reed switches, hall effect sensors, or other detection methods
const int MOVE_BUTTON_PIN = 19;  // Button to simulate making a move

// Variables for move tracking
String currentMove = "e2-e4";   // Default move for testing
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int buttonState = HIGH;
int lastButtonState = HIGH;

// Variables for receiving moves from web
unsigned long lastPollTime = 0;
unsigned long pollInterval = 1000; // Poll every 1 second
String lastReceivedMove = "";
bool enableMoveReceiving = true; // Flag to enable/disable move receiving

// Sample moves for demonstration
const char* sampleMoves[] = {
  "e2-e4",  // King's pawn opening
  "e7-e5",  // King's pawn response
  "g1-f3",  // Knight to f3
  "b8-c6",  // Knight to c6
  "f1-c4",  // Bishop to c4 (Italian game)
  "f8-c5",  // Bishop to c5
  "O-O",    // Castling
  "d7-d6",  // Pawn to d6
};
int moveIndex = 0;
const int TOTAL_SAMPLE_MOVES = 8;

void setup() {
  Serial.begin(115200);
  pinMode(MOVE_BUTTON_PIN, INPUT_PULLUP);
  
  Serial.println();
  Serial.println("==========================================");
  Serial.println("    ESP Chess Controller for Chesswizzz   ");
  Serial.println("==========================================");
  
  // Connect to WiFi
  connectToWiFi();
}

void loop() {
  // Check if the button is pressed (debounced)
  int reading = digitalRead(MOVE_BUTTON_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      
      // If the button is pressed (LOW due to pull-up)
      if (buttonState == LOW && !buttonPressed) {
        buttonPressed = true;
        
        // Select the next move from samples
        currentMove = sampleMoves[moveIndex];
        moveIndex = (moveIndex + 1) % TOTAL_SAMPLE_MOVES;
        
        // Send the move to the server
        sendMoveToServer(currentMove);
        
        // Wait a moment before allowing another button press
        delay(500);
        buttonPressed = false;
      }
    }
  }
  
  lastButtonState = reading;
  
  // Poll for incoming moves from web interface
  if (enableMoveReceiving && (millis() - lastPollTime) >= pollInterval) {
    checkForIncomingMoves();
    lastPollTime = millis();
  }
  
  // Process any serial commands
  processSerialCommands();
  
  delay(20); // Small delay to prevent excessive CPU usage
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Failed to connect to WiFi. Please check credentials.");
  }
}

void sendMoveToServer(String move) {
  // Only proceed if WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot send move.");
    connectToWiFi();
    return;
  }
  
  Serial.print("Sending chess move: ");
  Serial.println(move);
  
  // Create HTTP client
  HTTPClient http;
  WiFiClient client;
  
  // Start connection
  http.begin(client, serverUrl);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload
  String jsonPayload = "{\"move\":\"" + move + "\",\"roomId\":\"" + roomId + "\"}";
  
  // Send POST request
  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.print("Error sending HTTP request. Error code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}

void checkForIncomingMoves() {
  // Only proceed if WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi not connected - skipping move check");
    return;
  }
  
  // Create HTTP client
  HTTPClient http;
  WiFiClient client;
  
  // Build the polling URL with room ID
  String fullPollingUrl = pollingUrl + "?roomId=" + roomId;
  
  Serial.print("🔍 Polling: ");
  Serial.println(fullPollingUrl);
  
  // Start connection
  http.begin(client, fullPollingUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000); // 5 second timeout
  
  // Send GET request
  int httpResponseCode = http.GET();
  
  Serial.print("📡 HTTP Response: ");
  Serial.println(httpResponseCode);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("📄 Response: ");
    Serial.println(response);
    
    if (httpResponseCode == 200) {
      // Parse the JSON response to extract the move
      // Simple parsing for move field
      int moveStart = response.indexOf("\"move\":\"");
      if (moveStart != -1) {
        moveStart += 8; // Length of "\"move\":\""
        int moveEnd = response.indexOf("\"", moveStart);
        if (moveEnd != -1) {
          String incomingMove = response.substring(moveStart, moveEnd);
          
          Serial.print("🎯 Parsed move: ");
          Serial.println(incomingMove);
          
          // Only process if it's a new move
          if (incomingMove != lastReceivedMove && incomingMove.length() >= 4) {
            lastReceivedMove = incomingMove;
            
            // Display the received move prominently
            Serial.println();
            Serial.println("======================================");
            Serial.println("📥 MOVE RECEIVED FROM WEB INTERFACE!");
            Serial.println("======================================");
            Serial.print("🎯 Move: ");
            Serial.println(incomingMove);
            Serial.print("🕒 Time: ");
            Serial.println(millis());
            Serial.println("--------------------------------------");
            Serial.println("This move was made on the web by:");
            Serial.println("- AI player (in single player mode)");
            Serial.println("- Bottom player (in multiplayer mode)");
            Serial.println("======================================");
            Serial.println();
            
            // Here you would implement the physical board update
            // For example, light up LEDs, move servo motors, etc.
            handlePhysicalBoardUpdate(incomingMove);
          } else if (incomingMove == lastReceivedMove) {
            Serial.println("🔄 Same move as before, ignoring");
          } else {
            Serial.println("❌ Move too short or invalid");
          }
        } else {
          Serial.println("❌ Could not find move end quote");
        }
      } else {
        Serial.println("❌ No move field found in response");
      }
    } else if (httpResponseCode == 404) {
      // No new moves available - this is normal
      Serial.println("📭 No new moves available (404)");
    } else {
      Serial.print("⚠️  Error polling for moves. HTTP Code: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.print("❌ Error connecting to server for move polling. Error code: ");
    Serial.println(httpResponseCode);
    Serial.println("🔧 Try 'url' command to test server connectivity");
  }
  
  http.end();
}

void handlePhysicalBoardUpdate(String move) {
  Serial.println("🔄 Updating physical board...");
  Serial.print("📍 Move to implement: ");
  Serial.println(move);
  
  // Parse the move (e.g., "e2-e4" or "e2e4")
  String fromSquare, toSquare;
  
  if (move.indexOf("-") != -1) {
    // Format: "e2-e4"
    int dashPos = move.indexOf("-");
    fromSquare = move.substring(0, dashPos);
    toSquare = move.substring(dashPos + 1);
  } else if (move.length() >= 4) {
    // Format: "e2e4"
    fromSquare = move.substring(0, 2);
    toSquare = move.substring(2, 4);
  } else {
    Serial.println("❌ Invalid move format");
    return;
  }
  
  Serial.print("📤 From: ");
  Serial.print(fromSquare);
  Serial.print(" → To: ");
  Serial.println(toSquare);
  
  // TODO: Implement your physical board update logic here
  // Examples:
  // - Light up LEDs on from/to squares
  // - Move servo motors
  // - Activate electromagnets
  // - Sound buzzer patterns
  
  Serial.println("✅ Physical board update complete!");
}

void processSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("move:")) {
      // Extract the move from the command
      String moveCommand = command.substring(5);
      moveCommand.trim();
      
      // Send the custom move
      if (moveCommand.length() >= 4) {
        sendMoveToServer(moveCommand);
      } else {
        Serial.println("Invalid move format. Use 'move: e2-e4'");
      }
    } 
    else if (command == "help") {
      Serial.println("Available commands:");
      Serial.println("- move: e2-e4    (Send a specific chess move)");
      Serial.println("- help           (Show this help message)");
      Serial.println("- status         (Show connection status)");
      Serial.println("- poll           (Manually check for incoming moves)");
      Serial.println("- toggle         (Enable/disable automatic move receiving)");
      Serial.println("- test           (Show test instructions)");
      Serial.println("- debug          (Enable detailed HTTP debugging)");
      Serial.println("- url            (Test server URL connectivity)");
      Serial.println("- room: default  (Change room ID)");
      Serial.println("- rooms          (Test different room IDs)");
    }
    else if (command == "status") {
      Serial.print("WiFi status: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Server URL: ");
      Serial.println(serverUrl);
      Serial.print("Polling URL: ");
      Serial.println(pollingUrl);
      Serial.print("Room ID: ");
      Serial.println(roomId);
      Serial.print("Move receiving: ");
      Serial.println(enableMoveReceiving ? "Enabled" : "Disabled");
      Serial.print("Last received move: ");
      Serial.println(lastReceivedMove.length() > 0 ? lastReceivedMove : "None");
    }
    else if (command == "poll") {
      Serial.println("🔍 Manually checking for incoming moves...");
      checkForIncomingMoves();
    }
    else if (command == "toggle") {
      enableMoveReceiving = !enableMoveReceiving;
      Serial.print("Move receiving ");
      Serial.println(enableMoveReceiving ? "ENABLED" : "DISABLED");
    }
    else if (command == "test") {
      Serial.println();
      Serial.println("==========================================");
      Serial.println("         ESP MOVE RECEIVING TEST          ");
      Serial.println("==========================================");
      Serial.println("1. Make sure your backend server is running");
      Serial.println("2. Open the Chesswizzz web application");
      Serial.println("3. Start a chess game (single or multiplayer)");
      Serial.println("4. Make moves on the web interface:");
      Serial.println("   - Single Player: AI moves will appear here");
      Serial.println("   - Multiplayer: Bottom player moves appear here");
      Serial.println("5. Watch this Serial Monitor for incoming moves");
      Serial.println();
      Serial.println("Commands to use during testing:");
      Serial.println("- 'poll' to manually check for moves");
      Serial.println("- 'toggle' to enable/disable auto-polling");
      Serial.println("- 'status' to check connection");
      Serial.println("- 'debug' to enable HTTP debugging");
      Serial.println("- 'url' to test server connectivity");
      Serial.println("==========================================");
    }
    else if (command == "debug") {
      Serial.println("🔧 Starting debug mode...");
      Serial.println("Testing multiple room IDs...");
      
      String testRoomIds[] = {"default", "singleplayer-default", "multiplayer-default"};
      
      for (int i = 0; i < 3; i++) {
        HTTPClient http;
        WiFiClient client;
        String testUrl = pollingUrl + "?roomId=" + testRoomIds[i];
        
        Serial.print("🌐 Testing Room: ");
        Serial.print(testRoomIds[i]);
        Serial.print(" URL: ");
        Serial.println(testUrl);
        
        http.begin(client, testUrl);
        http.addHeader("Content-Type", "application/json");
        
        int httpResponseCode = http.GET();
        String response = http.getString();
        
        Serial.print("📡 HTTP Code: ");
        Serial.print(httpResponseCode);
        Serial.print(" | Response: ");
        Serial.println(response);
        Serial.println("---");
        
        http.end();
        delay(500);
      }
    }
    else if (command == "url") {
      Serial.println("🌐 Testing server connectivity...");
      
      HTTPClient http;
      WiFiClient client;
      
      // Test basic server connection
      http.begin(client, serverUrl);
      http.setTimeout(5000);
      
      Serial.print("🔗 Testing: ");
      Serial.println(serverUrl);
      
      int httpResponseCode = http.GET();
      
      if (httpResponseCode > 0) {
        Serial.print("✅ Server reachable! HTTP Code: ");
        Serial.println(httpResponseCode);
      } else {
        Serial.print("❌ Server unreachable! Error: ");
        Serial.println(httpResponseCode);
        Serial.println("📝 Check:");
        Serial.println("   - Backend server is running");
        Serial.println("   - IP address is correct");
        Serial.println("   - Port 3001 is open");
        Serial.println("   - WiFi connection is stable");
      }
      
      http.end();
    }
    else if (command.startsWith("room: ")) {
      String newRoomId = command.substring(6);
      newRoomId.trim();
      if (newRoomId.length() > 0) {
        roomId = newRoomId;
        Serial.print("🏠 Room ID changed to: ");
        Serial.println(roomId);
        
        // Update polling URL with new room ID
        pollingUrl = "http://192.168.45.94:3001/getLastMove";
        
        Serial.print("📡 New polling URL: ");
        Serial.print(pollingUrl);
        Serial.print("?roomId=");
        Serial.println(roomId);
      } else {
        Serial.println("❌ Invalid room ID format");
        Serial.println("📝 Usage: room: your-room-name");
      }
    }
    else if (command == "rooms") {
      Serial.println("🏠 Testing different room IDs:");
      
      String testRoomIds[] = {"default", "singleplayer-default", "multiplayer-default"};
      
      for (int i = 0; i < 3; i++) {
        HTTPClient http;
        WiFiClient client;
        String testUrl = pollingUrl + "?roomId=" + testRoomIds[i];
        
        Serial.print("🌐 Testing Room: ");
        Serial.print(testRoomIds[i]);
        Serial.print(" | URL: ");
        Serial.println(testUrl);
        
        http.begin(client, testUrl);
        http.addHeader("Content-Type", "application/json");
        
        int httpResponseCode = http.GET();
        String response = http.getString();
        
        Serial.print("📡 HTTP Code: ");
        Serial.print(httpResponseCode);
        Serial.print(" | Response: ");
        Serial.println(response);
        Serial.println("---");
        
        http.end();
        delay(500);
      }
    }
    else {
      Serial.println("Unknown command. Type 'help' for available commands.");
    }
  }
}
