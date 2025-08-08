cd/*
 * Simple ESP32 Chess Move Receiver
 * Only receives moves from web and displays them
 */

#include <WiFi.h>
#include <HTTPClient.h>

// WiFi credentials
const char* ssid = "POCO X3 NFC";
const char* password = "123456789";

// Server configuration
String pollingUrl = "http://192.168.45.94:3001/getLastMove";
String roomId = "singleplayer-default"; // Match ChessGame.jsx room ID

// Timing variables
unsigned long lastPollTime = 0;
unsigned long pollInterval = 1000; // Poll every 1 second
String lastReceivedMove = "";

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.println("Commands:");
  Serial.println("- single (switch to single player)");
  Serial.println("- multi (switch to multiplayer)");
  Serial.println("- status (show current room)");
  Serial.println("- test (test both room types)");
  Serial.println("Waiting for moves...");
}

void loop() {
  // Poll for moves every second
  if (millis() - lastPollTime >= pollInterval) {
    checkForMoves();
    lastPollTime = millis();
  }
  
  // Check for serial commands
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "single") {
      roomId = "singleplayer-default";
      lastReceivedMove = ""; // Reset to see new moves
      Serial.println("Switched to Single Player mode");
    } else if (command == "multi") {
      roomId = "default";
      lastReceivedMove = ""; // Reset to see new moves
      Serial.println("Switched to Multiplayer mode");
    } else if (command == "status") {
      Serial.print("Current room: ");
      Serial.println(roomId);
    } else if (command == "test") {
      Serial.println("Testing both room types...");
      
      // Test single player room
      Serial.println("=== TESTING SINGLE PLAYER ROOM ===");
      testRoom("singleplayer-default");
      
      delay(1000);
      
      // Test multiplayer room  
      Serial.println("=== TESTING MULTIPLAYER ROOM ===");
      testRoom("default");
    }
  }
  
  delay(50);
}

void checkForMoves() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  WiFiClient client;
  
  String fullUrl = pollingUrl + "?roomId=" + roomId;
  
  http.begin(client, fullUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);
  
  int httpCode = http.GET();
  
  // DEBUG: Show what we're getting
  Serial.print("HTTP: ");
  Serial.print(httpCode);
  Serial.print(" | Room: ");
  Serial.print(roomId);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.print(" | Response: ");
    Serial.println(response);
    
    if (httpCode == 200) {
      // Simple JSON parsing for move
      int moveStart = response.indexOf("\"move\":\"");
      if (moveStart != -1) {
        moveStart += 8;
        int moveEnd = response.indexOf("\"", moveStart);
        if (moveEnd != -1) {
          String move = response.substring(moveStart, moveEnd);
          
          // Only show if it's a new move and valid
          if (move != lastReceivedMove && move.length() >= 4 && move != "null") {
            lastReceivedMove = move;
            
            // SIMPLE OUTPUT - ONLY SHOW THE MOVE
            Serial.print("MOVE: ");
            Serial.println(move);
          }
        }
      }
    }
  } else {
    Serial.println(" | Error connecting");
  }
  
  http.end();
}

void testRoom(String testRoomId) {
  HTTPClient http;
  WiFiClient client;
  
  String testUrl = pollingUrl + "?roomId=" + testRoomId;
  
  Serial.print("Testing Room: ");
  Serial.print(testRoomId);
  Serial.print(" | URL: ");
  Serial.println(testUrl);
  
  http.begin(client, testUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);
  
  int httpCode = http.GET();
  
  Serial.print("HTTP: ");
  Serial.print(httpCode);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.print(" | Response: ");
    Serial.println(response);
  } else {
    Serial.println(" | Error connecting");
  }
  
  http.end();
  Serial.println("---");
}
