// test-physical-move-corrected.ino - ESP32 code to send chess moves to server

#include <WiFi.h>
#include <HTTPClient.h>

// WiFi credentials
const char* ssid = "POCO X3 NFC";
const char* password = "123456789";

// Server endpoint for physical moves - CORRECTED
const char* serverUrl = "http://192.168.121.94:3001/physicalMove";

// Button pin - connects to GND when pressed
const int buttonPin = 2;
int lastButtonState = HIGH;  // Pulled up by default

void setup() {
  Serial.begin(115200);
  
  // Setup button pin with internal pullup resistor
  pinMode(buttonPin, INPUT_PULLUP);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Ready to send moves to server: " + String(serverUrl));
}

void loop() {
  // Read button state
  int buttonState = digitalRead(buttonPin);
  
  // Check if button was just pressed
  if (buttonState == LOW && lastButtonState == HIGH) {
    Serial.println("Button pressed - sending move e2-e4");
    sendMoveToServer("e2-e4");
    delay(1000);  // Longer debounce to prevent multiple sends
  }
  
  lastButtonState = buttonState;
  delay(10);  // Small delay for stability
}

void sendMoveToServer(String move) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Configure the request
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON payload with proper format for your backend
    String jsonPayload = "{\"move\":\"" + move + "\",\"roomId\":\"default\",\"playerSide\":\"white\"}";
    
    Serial.println("Sending: " + jsonPayload);
    
    // Send POST request
    int httpResponseCode = http.POST(jsonPayload);
    
    // Check response
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("✅ HTTP Response code: " + String(httpResponseCode));
      Serial.println("✅ Response: " + response);
    } else {
      Serial.println("❌ Error sending HTTP request: " + String(httpResponseCode));
    }
    
    http.end();
  } else {
    Serial.println("❌ WiFi disconnected");
  }
}
