// === ARDUINO CHESS MOTOR CONTROLLER - MEMORY OPTIMIZED ===
// 
// 🎯 ENHANCED FEATURES:
// - Intelligent piece detection system
// - Smart knight path analysis
// - Direct L-path when clear
// - Minimal grid avoidance when blocked
// - Real-time board state tracking
// - Optimal distance calculation
// - CAPTURE MOVE HANDLING with boundary removal
//
// 🐴 KNIGHT OPTIMIZATION:
// - Analyzes all 64 squares for piece positions
// - Chooses shortest clear path automatically
// - Uses quarter-square margins when grid needed
// - Eliminates unnecessary movements
//
// 🎯 CAPTURE SYSTEM:
// - Detects capture moves (e4xf5)
// - First: Remove captured piece to boundary
// - Then: Execute normal move
// - Edge-based movement for captured pieces

#include <Arduino.h>

// === Pin definitions
#define A_STEP_PIN 2
#define A_DIR_PIN 5
#define B_STEP_PIN 3
#define B_DIR_PIN 6
#define ENABLE_PIN 8

#define X_LIMIT_PIN 9
#define Y_LIMIT_PIN 10

// === Stepper motor and board settings
const int step_delay = 550; // microseconds
const long steps_per_cm = 419;
const float initial_square_size_cm = 5.0;    // Square size before reaching (0,0)
const float final_square_size_cm = 5.34;     // Square size after reaching (0,0)
float current_square_size_cm = initial_square_size_cm;
long current_steps_per_square = steps_per_cm * initial_square_size_cm;

// === Current position (chess grid coordinate)
int currentX = -2;
int currentY = 0;
bool hasReachedOrigin = false;

// === INTELLIGENT PIECE DETECTION SYSTEM ===
// Optimized: Use bit array instead of bool array to save memory
byte squareOccupied[8] = {0}; // 8 bytes instead of 64 bytes - each bit represents one square
unsigned long lastBoardScan = 0;
const unsigned long BOARD_SCAN_INTERVAL = 1000; // Scan every 1 second

// === CAPTURE MOVE DETECTION ===
bool isCaptureMove = false;
byte captureTargetX = 255; // Use byte instead of int, 255 = invalid
byte captureTargetY = 255;

// Memory-optimized Knight path analysis structure
struct KnightPathInfo {
  byte pathType;           // 1=Direct X→Y, 2=Direct Y→X, 3=Grid minimal
  float estimatedDistance;
  bool pathClear;
  byte intermediateX, intermediateY; // Corner square of L-path
};

// === Setup
void setup() {
  Serial.begin(115200);

  pinMode(A_STEP_PIN, OUTPUT);
  pinMode(A_DIR_PIN, OUTPUT);
  pinMode(B_STEP_PIN, OUTPUT);
  pinMode(B_DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);

  pinMode(X_LIMIT_PIN, INPUT_PULLUP);
  pinMode(Y_LIMIT_PIN, INPUT_PULLUP);

  digitalWrite(ENABLE_PIN, LOW);

  // Initialize intelligent board state
  initializeBoardState();

  homeToOrigin();
  delay(500);

  applyOffset(0.4, 0.85);
  delay(500);

  moveOnlyX(2);
  currentX = 0;
  currentY = 0;
  
  updateSquareSize();

  Serial.println(F("READY - Memory Optimized System Active"));
  Serial.println(F("🐴 Knight moves optimized"));
  Serial.println(F("📡 Board state tracking enabled"));
}

// === Main Loop
void loop() {
  if (Serial.available()) {
    String move = Serial.readStringUntil('\n');
    move.trim();

    // Handle reset command from ESP32
    if (move == F("RESET_ARDUINO")) {
      Serial.println(F("RESET command received from ESP32!"));
      Serial.println(F("Resetting Arduino system..."));
      
      // Disable all motors immediately
      digitalWrite(ENABLE_PIN, HIGH); // Disable motors
      digitalWrite(A_STEP_PIN, LOW);
      digitalWrite(A_DIR_PIN, LOW);
      digitalWrite(B_STEP_PIN, LOW);
      digitalWrite(B_DIR_PIN, LOW);
      
      // Reset all variables
      currentX = -2;
      currentY = 0;
      hasReachedOrigin = false;
      current_square_size_cm = initial_square_size_cm;
      current_steps_per_square = steps_per_cm * initial_square_size_cm;
      
      // Reset capture flags
      resetCaptureFlags();
      
      // Send confirmation to ESP32
      Serial.println(F("ARDUINO_RESET_COMPLETE"));
      delay(100);
      
      // Re-initialize system
      setup();
      return;
    }

    // Handle ESP32 communication test
    if (move == F("ESP32_TEST")) {
      Serial.println(F("ARDUINO_READY"));
      Serial.println(F("✅ Arduino responding to ESP32 test"));
      return;  // Don't continue processing
    }

    if (move.length() == 5 && move.charAt(2) == '-') {
      String from = move.substring(0, 2);
      String to = move.substring(3, 5);

      Serial.print(F("Received move: "));
      Serial.println(move);
      
      // Check if this is a capture move
      checkForCaptureMove(from, to);

      int fromX = fileToX(from.charAt(0));
      int fromY = rankToY(from.charAt(1));
      int toX = fileToX(to.charAt(0));
      int toY = rankToY(to.charAt(1));

      // Handle capture sequence if needed
      if (isCaptureMove) {
        Serial.println(F("🎯 CAPTURE MOVE DETECTED!"));
        
        // Step 1: Remove captured piece first
        executeCaptureRemoval(captureTargetX, captureTargetY);
        
        Serial.println(F("✅ Captured piece removed"));
        delay(500);
      }

      int deltaX = toX - fromX;
      int deltaY = toY - fromY;
      bool isDiagonal = (abs(deltaX) == abs(deltaY)) && (deltaX != 0) && (deltaY != 0);
      bool isKnight = isKnightMove(deltaX, deltaY);

      Serial.print(F("Moving to source: "));
      Serial.println(from);
      moveToPosition(fromX, fromY);
      currentX = fromX;
      currentY = fromY;
      
      // Update board state - piece lifted from source
      updatePiecePosition(fromX, fromY, -1, -1);
      
      delay(200);

      Serial.println(F("MAGNET_ON"));
      waitForESPResponse(F("MAGNET_READY"));
      delay(500);

      if (isKnight) {
        Serial.println(F("🐴 INTELLIGENT KNIGHT MOVE"));
        executeIntelligentKnightMove(fromX, fromY, toX, toY);
      } else if (isDiagonal) {
        Serial.println(F("Diagonal move detected"));
        moveDiagonal(deltaX, deltaY);
      } else {
        Serial.print(F("Moving to destination: "));
        Serial.println(to);
        moveToPosition(toX, toY);
      }
      
      currentX = toX;
      currentY = toY;
      
      // Update board state - piece placed at destination
      updatePiecePosition(-1, -1, toX, toY);

      Serial.println(F("Move complete"));
      delay(200);

      Serial.println(F("MAGNET_OFF"));
      waitForESPResponse(F("MAGNET_OFF_READY"));
      
      // Reset capture flags
      resetCaptureFlags();
      
      Serial.println(F("Move completed successfully. Ready for next move."));
    }
  }
}

// === MEMORY OPTIMIZED BOARD STATE FUNCTIONS ===
// Helper functions for bit-packed board state
bool getSquareOccupied(byte x, byte y) {
  if (x >= 8 || y >= 8) return false;
  byte row = squareOccupied[y];
  return (row >> x) & 1;
}

void setSquareOccupied(byte x, byte y, bool occupied) {
  if (x >= 8 || y >= 8) return;
  if (occupied) {
    squareOccupied[y] |= (1 << x);
  } else {
    squareOccupied[y] &= ~(1 << x);
  }
}

void initializeBoardState() {
  // Initialize all squares as empty
  for (byte i = 0; i < 8; i++) {
    squareOccupied[i] = 0;
  }
  
  // Set initial piece positions (standard chess setup)
  setInitialPiecePositions();
  
  Serial.println(F("📋 Board state initialized"));
}

void setInitialPiecePositions() {
  // White pieces (bottom) - ranks 0,1
  squareOccupied[0] = 0xFF; // White back rank - all squares occupied
  squareOccupied[1] = 0xFF; // White pawns - all squares occupied
  
  // Black pieces (top) - ranks 6,7
  squareOccupied[6] = 0xFF; // Black pawns - all squares occupied
  squareOccupied[7] = 0xFF; // Black back rank - all squares occupied
  
  Serial.println(F("📋 Initial piece positions set"));
}

void updatePiecePosition(int fromX, int fromY, int toX, int toY) {
  // Clear source square
  if (fromX >= 0 && fromX < 8 && fromY >= 0 && fromY < 8) {
    setSquareOccupied(fromX, fromY, false);
  }
  
  // Set destination square
  if (toX >= 0 && toX < 8 && toY >= 0 && toY < 8) {
    setSquareOccupied(toX, toY, true);
  }
}

// === OPTIMIZED CAPTURE MOVE HANDLING ===
void checkForCaptureMove(String from, String to) {
  byte toX = fileToX(to.charAt(0));
  byte toY = rankToY(to.charAt(1));
  
  // Check if destination square is occupied (indicates capture)
  if (getSquareOccupied(toX, toY)) {
    isCaptureMove = true;
    captureTargetX = toX;
    captureTargetY = toY;
    
    Serial.print(F("🎯 CAPTURE DETECTED: Piece at "));
    Serial.println(to);
  } else {
    isCaptureMove = false;
  }
}

void executeCaptureRemoval(int captureX, int captureY) {
  Serial.println(F("📤 CAPTURE SEQUENCE - Removing piece"));
  
  // Move to captured piece location
  moveToPosition(captureX, captureY);
  currentX = captureX;
  currentY = captureY;
  
  delay(200);
  
  // Engage magnet with captured piece
  Serial.println(F("MAGNET_ON"));
  waitForESPResponse(F("MAGNET_READY"));
  delay(500);
  
  // Simple boundary removal - move to nearest edge
  if (captureX < 4) {
    moveOnlyX(-captureX - 1); // Move to left edge
  } else {
    moveOnlyX(7 - captureX + 1); // Move to right edge  
  }
  
  // Drop captured piece
  Serial.println(F("MAGNET_OFF"));
  waitForESPResponse(F("MAGNET_OFF_READY"));
  
  // Update board state - captured piece removed
  updatePiecePosition(captureX, captureY, -1, -1);
  
  delay(300);
}

void resetCaptureFlags() {
  isCaptureMove = false;
  captureTargetX = 255; // Invalid position
  captureTargetY = 255;
}

// === INTELLIGENT KNIGHT MOVEMENT SYSTEM ===
bool isKnightMove(int deltaX, int deltaY) {
  int absX = abs(deltaX);
  int absY = abs(deltaY);
  return (absX == 2 && absY == 1) || (absX == 1 && absY == 2);
}

void executeIntelligentKnightMove(int fromX, int fromY, int toX, int toY) {
  int deltaX = toX - fromX;
  int deltaY = toY - fromY;
  
  Serial.print("🐴 Knight Analysis: ");
  Serial.print(getSquareName(fromX, fromY));
  Serial.print(" → ");
  Serial.print(getSquareName(toX, toY));
  Serial.print(" (Δ");
  Serial.print(deltaX);
  Serial.print(",");
  Serial.print(deltaY);
  Serial.println(")");
  
  // Analyze all possible knight paths
  KnightPathInfo bestPath = analyzeKnightPaths(fromX, fromY, toX, toY);
  
  // Execute the optimal path
  executeSelectedKnightPath(bestPath, deltaX, deltaY);
}

KnightPathInfo analyzeKnightPaths(int fromX, int fromY, int toX, int toY) {
  KnightPathInfo bestPath;
  int deltaX = toX - fromX;
  int deltaY = toY - fromY;
  
  // Path 1: Direct L-shape (X first, then Y)
  bestPath.pathType = 1;
  bestPath.estimatedDistance = abs(deltaX) + abs(deltaY);
  bestPath.intermediateX = fromX + deltaX;
  bestPath.intermediateY = fromY;
  bestPath.pathClear = isSquareFree(bestPath.intermediateX, bestPath.intermediateY);
  
  // Path 2: Direct L-shape (Y first, then X) - check if better
  byte altX = fromX;
  byte altY = fromY + deltaY;
  if (isSquareFree(altX, altY)) {
    bestPath.pathType = 2;
    bestPath.intermediateX = altX;
    bestPath.intermediateY = altY;
    bestPath.pathClear = true;
    Serial.println(F("🎯 CLEAR Y→X PATH"));
  } else if (bestPath.pathClear) {
    Serial.println(F("🎯 CLEAR X→Y PATH"));
  } else {
    // Path 3: Grid avoidance
    bestPath.pathType = 3;
    bestPath.estimatedDistance = abs(deltaX) + abs(deltaY) + 0.5;
    bestPath.intermediateX = 255; // No specific intermediate
    bestPath.intermediateY = 255;
    bestPath.pathClear = true;
    Serial.println(F("⚠️ BLOCKED - Using grid path"));
  }
  
  return bestPath;
}

bool isSquareFree(int x, int y) {
  if (x < 0 || x >= 8 || y < 0 || y >= 8) {
    return false; // Out of bounds
  }
  return !getSquareOccupied(x, y);
}

String getSquareName(int x, int y) {
  if (x < 0 || x >= 8 || y < 0 || y >= 8) {
    return "??";
  }
  char file = 'a' + (7 - x); // Convert to chess notation
  char rank = '1' + y;
  return String(file) + String(rank);
}

void executeSelectedKnightPath(KnightPathInfo path, int deltaX, int deltaY) {
  switch (path.pathType) {
    case 1:
      // Direct L-path: X first, then Y
      Serial.println(F("🚀 EXECUTING: Direct L-path (X→Y)"));
      moveOnlyX(deltaX);
      moveOnlyY(deltaY);
      break;
      
    case 2:
      // Direct L-path: Y first, then X
      Serial.println(F("🚀 EXECUTING: Direct L-path (Y→X)"));
      moveOnlyY(deltaY);
      moveOnlyX(deltaX);
      break;
      
    case 3:
      // Grid avoidance with minimal margins
      Serial.println(F("🚀 EXECUTING: Grid minimal path"));
      executeGridMinimalPath(deltaX, deltaY);
      break;
  }
  
  Serial.println(F("✅ Knight movement completed"));
}

void executeGridMinimalPath(int deltaX, int deltaY) {
  // Use minimal grid margins (quarter-square instead of half-square)
  float quarterSquareSteps = current_steps_per_square * 0.25;
  
  Serial.println("📐 Step 1: Minimal margin offset (0.25 squares)...");
  
  // Move to minimal top-left margin
  digitalWrite(A_DIR_PIN, LOW);  // X-
  digitalWrite(B_DIR_PIN, LOW);
  for (long i = 0; i < (long)quarterSquareSteps; i++) {
    stepBoth();
  }
  
  digitalWrite(A_DIR_PIN, HIGH); // Y+
  digitalWrite(B_DIR_PIN, LOW);
  for (long i = 0; i < (long)quarterSquareSteps; i++) {
    stepBoth();
  }
  
  Serial.println("📐 Step 2: Main L-movement along grid...");
  
  // Execute the L-movement with grid clearance
  long fullSquareSteps = current_steps_per_square;
  
  if (abs(deltaX) == 2) {
    // X movement first
    long xSteps = abs(deltaX) * fullSquareSteps;
    digitalWrite(A_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    
    Serial.print("   X movement: ");
    Serial.print(deltaX);
    Serial.println(" squares");
    
    for (long i = 0; i < xSteps; i++) {
      stepBoth();
    }
    
    // Y movement
    long ySteps = abs(deltaY) * fullSquareSteps;
    digitalWrite(A_DIR_PIN, deltaY > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaY > 0 ? LOW : HIGH);
    
    Serial.print("   Y movement: ");
    Serial.print(deltaY);
    Serial.println(" squares");
    
    for (long i = 0; i < ySteps; i++) {
      stepBoth();
    }
  } else {
    // Y movement first
    long ySteps = abs(deltaY) * fullSquareSteps;
    digitalWrite(A_DIR_PIN, deltaY > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaY > 0 ? LOW : HIGH);
    
    Serial.print("   Y movement: ");
    Serial.print(deltaY);
    Serial.println(" squares");
    
    for (long i = 0; i < ySteps; i++) {
      stepBoth();
    }
    
    // X movement
    long xSteps = abs(deltaX) * fullSquareSteps;
    digitalWrite(A_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    
    Serial.print("   X movement: ");
    Serial.print(deltaX);
    Serial.println(" squares");
    
    for (long i = 0; i < xSteps; i++) {
      stepBoth();
    }
  }
  
  Serial.println("📐 Step 3: Return to center (0.25 squares)...");
  
  // Return to center with minimal offset
  digitalWrite(A_DIR_PIN, HIGH); // X+
  digitalWrite(B_DIR_PIN, HIGH);
  for (long i = 0; i < (long)quarterSquareSteps; i++) {
    stepBoth();
  }
  
  digitalWrite(A_DIR_PIN, LOW);  // Y-
  digitalWrite(B_DIR_PIN, HIGH);
  for (long i = 0; i < (long)quarterSquareSteps; i++) {
    stepBoth();
  }
}

// === Utility Functions
void waitForESPResponse(const __FlashStringHelper* expectedResponse) {
  unsigned long startTime = millis();
  const unsigned long timeout = 10000;
  
  while (millis() - startTime < timeout) {
    if (Serial.available()) {
      String response = Serial.readStringUntil('\n');
      response.trim();
      if (response.equals(expectedResponse)) {
        return;
      }
    }
    delay(10);
  }
  Serial.println(F("Timeout waiting for ESP32!"));
}

void updateSquareSize() {
  if ((currentX >= 0 && currentY >= 0) && !hasReachedOrigin) {
    current_square_size_cm = final_square_size_cm;
    current_steps_per_square = steps_per_cm * current_square_size_cm;
    hasReachedOrigin = true;
    Serial.print("Square size updated to: ");
    Serial.print(current_square_size_cm);
    Serial.println(" cm");
  }
}

void moveDiagonal(int deltaX, int deltaY) {
  long stepsX = abs(deltaX) * current_steps_per_square;
  long stepsY = abs(deltaY) * current_steps_per_square;

  bool xDir = deltaX > 0 ? HIGH : LOW;
  bool yDirA = deltaY > 0 ? HIGH : LOW;
  bool yDirB = deltaY > 0 ? LOW : HIGH;

  digitalWrite(A_DIR_PIN, xDir);
  digitalWrite(B_DIR_PIN, xDir);

  long totalSteps = max(stepsX, stepsY);
  long error = 0;
  long deltaError = min(stepsX, stepsY);
  long majorSteps = max(stepsX, stepsY);
  bool xMajor = stepsX >= stepsY;

  Serial.print("Diagonal move: deltaX=");
  Serial.print(deltaX);
  Serial.print(", deltaY=");
  Serial.print(deltaY);
  Serial.print(", square size=");
  Serial.print(current_square_size_cm);
  Serial.println(" cm");

  for (long i = 0; i < totalSteps; i++) {
    if (xMajor) {
      digitalWrite(A_DIR_PIN, xDir);
      digitalWrite(B_DIR_PIN, xDir);
      stepBoth();
      error += deltaError;
      if (error >= majorSteps) {
        digitalWrite(A_DIR_PIN, yDirA);
        digitalWrite(B_DIR_PIN, yDirB);
        stepBoth();
        error -= majorSteps;
      }
    } else {
      digitalWrite(A_DIR_PIN, yDirA);
      digitalWrite(B_DIR_PIN, yDirB);
      stepBoth();
      error += deltaError;
      if (error >= majorSteps) {
        digitalWrite(A_DIR_PIN, xDir);
        digitalWrite(B_DIR_PIN, xDir);
        stepBoth();
        error -= majorSteps;
      }
    }
  }
}

void moveToPosition(int targetX, int targetY) {
  int deltaX = targetX - currentX;
  int deltaY = targetY - currentY;
  
  if (deltaX != 0) {
    moveOnlyX(deltaX);
    updateSquareSize();
  }
  if (deltaY != 0) {
    moveOnlyY(deltaY);
    updateSquareSize();
  }
}

int fileToX(char file) {
  return 7 - (file - 'a');
}

int rankToY(char rank) {
  return rank - '1';
}

void moveOnlyX(int squares) {
  if (squares == 0) return;
  digitalWrite(A_DIR_PIN, squares > 0 ? HIGH : LOW);
  digitalWrite(B_DIR_PIN, squares > 0 ? HIGH : LOW);
  long steps = abs(squares) * current_steps_per_square;
  
  for (long i = 0; i < steps; i++) {
    stepBoth();
  }
}

void moveOnlyY(int squares) {
  if (squares == 0) return;
  digitalWrite(A_DIR_PIN, squares > 0 ? HIGH : LOW);
  digitalWrite(B_DIR_PIN, squares > 0 ? LOW : HIGH);
  long steps = abs(squares) * current_steps_per_square;
  
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

void homeToOrigin() {
  digitalWrite(A_DIR_PIN, LOW);
  digitalWrite(B_DIR_PIN, HIGH);
  while (digitalRead(Y_LIMIT_PIN) == HIGH) {
    stepBoth();
  }
  delay(300);

  digitalWrite(A_DIR_PIN, LOW);
  digitalWrite(B_DIR_PIN, LOW);
  while (digitalRead(X_LIMIT_PIN) == HIGH) {
    stepBoth();
  }
  delay(500);

  currentX = -2;
  currentY = 0;
  Serial.println("Homed to (-2, 0)");
}

void applyOffset(float offsetX_cm, float offsetY_cm) {
  long dx_steps = offsetX_cm * steps_per_cm;
  long dy_steps = offsetY_cm * steps_per_cm;

  digitalWrite(A_DIR_PIN, HIGH);
  digitalWrite(B_DIR_PIN, HIGH);
  for (long i = 0; i < dx_steps; i++) {
    stepBoth();
  }

  digitalWrite(A_DIR_PIN, HIGH);
  digitalWrite(B_DIR_PIN, LOW);
  for (long i = 0; i < dy_steps; i++) {
    stepBoth();
  }

  Serial.println("Offset applied");
}
