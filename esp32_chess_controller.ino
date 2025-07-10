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
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Server configuration
const char* serverUrl = "http://YOUR_SERVER_IP:3001/physicalMove";
const char* roomId = "default"; // Game room ID (if using multiple boards)

// Pin definitions for demonstration
// In a real chess board, you might use reed switches, hall effect sensors, or other detection methods
const int MOVE_BUTTON_PIN = 2;  // Button to simulate making a move

// Variables for move tracking
String currentMove = "e2-e4";   // Default move for testing
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int buttonState = HIGH;
int lastButtonState = HIGH;

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
    }
    else if (command == "status") {
      Serial.print("WiFi status: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Server URL: ");
      Serial.println(serverUrl);
    }
    else {
      Serial.println("Unknown command. Type 'help' for available commands.");
    }
  }
}
