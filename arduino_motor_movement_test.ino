/*
 * ARDUINO COMMUNICATION TEST - Standalone Motor Movement Test
 * 
 * 🎯 PURPOSE: Test Arduino motor movements WITHOUT ESP32
 * - Verify coordinate system works correctly
 * - Test basic movements step by step
 * - Ensure position tracking is accurate
 * 
 * 🔧 USAGE:
 * 1. Upload this to Arduino
 * 2. Open Serial Monitor (115200 baud)
 * 3. Type moves manually: h1-h2, g1-f3, e2-e4, etc.
 * 4. Watch debug output to verify correct movements
 */

#include <Arduino.h>

// === Pin definitions (SAME AS MAIN CODE)
#define A_STEP_PIN 2
#define A_DIR_PIN 5
#define B_STEP_PIN 3
#define B_DIR_PIN 6
#define ENABLE_PIN 8
#define X_LIMIT_PIN 9
#define Y_LIMIT_PIN 10

// === Settings (SAME AS MAIN CODE)
const int step_delay = 450;
const long steps_per_cm = 419;
const float square_size_cm = 5.34;
const long steps_per_square = steps_per_cm * square_size_cm;

// === Current position
int currentX = 0;  // Start at (0,0) = h1
int currentY = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(A_STEP_PIN, OUTPUT);
  pinMode(A_DIR_PIN, OUTPUT);
  pinMode(B_STEP_PIN, OUTPUT);
  pinMode(B_DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(X_LIMIT_PIN, INPUT_PULLUP);
  pinMode(Y_LIMIT_PIN, INPUT_PULLUP);
  
  digitalWrite(ENABLE_PIN, LOW);  // Enable motors
  
  Serial.println("🧪 ARDUINO MOTOR MOVEMENT TEST");
  Serial.println("===============================");
  Serial.println("📍 Starting at position (0,0) = h1");
  Serial.println("🎯 Coordinate System: (0,0)=h1, (1,0)=g1, (7,0)=a1");
  Serial.println("📝 Type moves like: h1-h2, g1-f3, e2-e4");
  Serial.println("🔧 Enhanced debugging enabled");
  Serial.println("===============================");
  Serial.println();
}

void loop() {
  if (Serial.available()) {
    String move = Serial.readStringUntil('\n');
    move.trim();
    
    Serial.println("================================================");
    Serial.print("📨 RECEIVED: '");
    Serial.print(move);
    Serial.println("'");
    
    if (move.length() == 5 && move.charAt(2) == '-') {
      String from = move.substring(0, 2);
      String to = move.substring(3, 5);
      
      Serial.print("✅ PARSING: ");
      Serial.print(from);
      Serial.print(" → ");
      Serial.println(to);
      
      // Convert to coordinates
      int fromX = fileToX(from.charAt(0));
      int fromY = rankToY(from.charAt(1));
      int toX = fileToX(to.charAt(0));
      int toY = rankToY(to.charAt(1));
      
      Serial.print("📍 COORDINATES: ");
      Serial.print(from);
      Serial.print("(");
      Serial.print(fromX);
      Serial.print(",");
      Serial.print(fromY);
      Serial.print(") → ");
      Serial.print(to);
      Serial.print("(");
      Serial.print(toX);
      Serial.print(",");
      Serial.print(toY);
      Serial.println(")");
      
      Serial.print("🎯 CURRENT POSITION: (");
      Serial.print(currentX);
      Serial.print(",");
      Serial.print(currentY);
      Serial.println(")");
      
      // Calculate deltas
      int deltaX = toX - fromX;
      int deltaY = toY - fromY;
      
      Serial.print("📐 MOVEMENT: Delta(");
      Serial.print(deltaX);
      Serial.print(",");
      Serial.print(deltaY);
      Serial.println(")");
      
      // Move to source position
      Serial.print("🚀 MOVING TO SOURCE: ");
      Serial.println(from);
      moveToPosition(fromX, fromY);
      currentX = fromX;
      currentY = fromY;
      
      Serial.print("✅ AT SOURCE: (");
      Serial.print(currentX);
      Serial.print(",");
      Serial.print(currentY);
      Serial.println(")");
      
      delay(500);  // Pause to "pick up piece"
      
      // Move to destination
      Serial.print("🎯 MOVING TO DESTINATION: ");
      Serial.println(to);
      moveToPosition(toX, toY);
      currentX = toX;
      currentY = toY;
      
      Serial.print("🏁 AT DESTINATION: (");
      Serial.print(currentX);
      Serial.print(",");
      Serial.print(currentY);
      Serial.print(") = ");
      Serial.println(getSquareName(currentX, currentY));
      
      Serial.println("✅ MOVE COMPLETED SUCCESSFULLY!");
      
    } else {
      Serial.print("❌ INVALID FORMAT: '");
      Serial.print(move);
      Serial.println("' - Use format: XX-XX (e.g., h1-h2)");
    }
    Serial.println("================================================");
    Serial.println();
  }
}

// === COORDINATE CONVERSION FUNCTIONS (SAME AS MAIN CODE)
int fileToX(char file) {
  return 7 - (file - 'a');  // a=7, b=6, c=5, d=4, e=3, f=2, g=1, h=0
}

int rankToY(char rank) {
  return rank - '1';  // 1=0, 2=1, 3=2, ..., 8=7
}

String getSquareName(int x, int y) {
  if (x < 0 || x >= 8 || y < 0 || y >= 8) {
    return "??";
  }
  char file = 'a' + (7 - x);  // x=0 -> 'h', x=1 -> 'g', ..., x=7 -> 'a'
  char rank = '1' + y;  // y=0 -> '1', y=1 -> '2', ..., y=7 -> '8'
  return String(file) + String(rank);
}

// === MOVEMENT FUNCTIONS (SAME AS MAIN CODE)
void moveToPosition(int targetX, int targetY) {
  int deltaX = targetX - currentX;
  int deltaY = targetY - currentY;
  
  Serial.print("📍 POSITION MOVE: (");
  Serial.print(currentX);
  Serial.print(",");
  Serial.print(currentY);
  Serial.print(") → (");
  Serial.print(targetX);
  Serial.print(",");
  Serial.print(targetY);
  Serial.print(") | Delta: (");
  Serial.print(deltaX);
  Serial.print(",");
  Serial.print(deltaY);
  Serial.println(")");
  
  if (deltaX != 0) moveOnlyX(deltaX);
  if (deltaY != 0) moveOnlyY(deltaY);
}

void moveOnlyX(int squares) {
  if (squares == 0) return;
  
  Serial.print("🔧 X MOVEMENT: ");
  Serial.print(squares);
  Serial.print(" squares ");
  Serial.print(squares > 0 ? "RIGHT (+X)" : "LEFT (-X)");
  Serial.print(" | Motors: A=");
  Serial.print(squares > 0 ? "HIGH" : "LOW");
  Serial.print(", B=");
  Serial.println(squares > 0 ? "HIGH" : "LOW");
  
  // Both motors same direction for X movement
  digitalWrite(A_DIR_PIN, squares > 0 ? HIGH : LOW);
  digitalWrite(B_DIR_PIN, squares > 0 ? HIGH : LOW);
  long steps = abs(squares) * steps_per_square;
  
  for (long i = 0; i < steps; i++) {
    stepBoth();
  }
}

void moveOnlyY(int squares) {
  if (squares == 0) return;
  
  Serial.print("🔧 Y MOVEMENT: ");
  Serial.print(squares);
  Serial.print(" squares ");
  Serial.print(squares > 0 ? "UP (+Y)" : "DOWN (-Y)");
  Serial.print(" | Motors: A=");
  Serial.print(squares > 0 ? "HIGH" : "LOW");
  Serial.print(", B=");
  Serial.println(squares > 0 ? "LOW" : "HIGH");
  
  // A motor direction, B motor opposite direction for Y movement
  digitalWrite(A_DIR_PIN, squares > 0 ? HIGH : LOW);
  digitalWrite(B_DIR_PIN, squares > 0 ? LOW : HIGH);
  long steps = abs(squares) * steps_per_square;
  
  for (long i = 0; i < steps; i++) {
    stepBoth();
  }
}

void stepBoth() {
  digitalWrite(A_STEP_PIN, HIGH);
  digitalWrite(B_STEP_PIN, HIGH);
  delayMicroseconds(step_delay);
  digitalWrite(A_STEP_PIN, LOW);
  digitalWrite(B_STEP_PIN, LOW);
  delayMicroseconds(step_delay);
}
