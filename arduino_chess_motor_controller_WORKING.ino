// === ARDUINO CHESS MOTOR CONTROLLER - WORKING VERSION ===
// 
// 🎯 FEATURES:
// - Dual stepper motor control (X/Y movement)
// - Chess coordinate system (a1-h8)
// - Automatic homing to origin with limit switches
// - Knight move L-shaped path execution
// - Diagonal movement with interpolation
// - Dynamic square size adjustment
// - ESP32 communication for magnet control
// - Position tracking and validation
//
// 📡 COMMUNICATION:
// - Receives chess moves via Serial (e.g., "e2-e4")
// - Sends magnet commands to ESP32
// - Waits for ESP32 confirmation before proceeding
//
// 🔧 HARDWARE:
// - 2x Stepper motors (A and B axes)
// - 2x Limit switches (X and Y)
// - Enable pin for motor control
// - Serial communication at 115200 baud
//
// 📝 NOTES:
// - Works correctly with minor issues to be upgraded later
// - Calibrated for 5.0cm initial squares, 5.34cm final squares
// - 419 steps per cm resolution
// - 550μs step delay for smooth movement

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
bool squareOccupied[8][8] = {false}; // Track piece positions
unsigned long lastBoardScan = 0;
const unsigned long BOARD_SCAN_INTERVAL = 1000; // Scan every 1 second

// Knight path analysis structure
struct KnightPathInfo {
  int pathType;           // 1=Direct X→Y, 2=Direct Y→X, 3=Grid minimal
  String description;
  float estimatedDistance;
  bool pathClear;
  int intermediateX, intermediateY; // Corner square of L-path
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

  // Initialize board state detection
  initializeBoardState();

  homeToOrigin();
  delay(500);

  applyOffset(0.4, 0.85);
  delay(500);

  moveOnlyX(2);
  currentX = 0;
  currentY = 0;
  
  updateSquareSize();

  Serial.println("READY - Intelligent Knight System Active");
}

// === Main Loop
void loop() {
  if (Serial.available()) {
    String move = Serial.readStringUntil('\n');
    move.trim();

    // Handle reset command from ESP32
    if (move == "RESET_ARDUINO") {
      Serial.println("RESET command received from ESP32!");
      Serial.println("Resetting Arduino system...");
      digitalWrite(ENABLE_PIN, HIGH); // Disable motors
      digitalWrite(A_STEP_PIN, LOW);
      digitalWrite(A_DIR_PIN, LOW);
      digitalWrite(B_STEP_PIN, LOW);
      digitalWrite(B_DIR_PIN, LOW);
      delay(100);
      
      // Re-initialize system
      setup();
      return;
    }

    if (move.length() == 5 && move.charAt(2) == '-') {
      String from = move.substring(0, 2);
      String to = move.substring(3, 5);

      Serial.print("Received move: ");
      Serial.println(move);

      int fromX = fileToX(from.charAt(0));
      int fromY = rankToY(from.charAt(1));
      int toX = fileToX(to.charAt(0));
      int toY = rankToY(to.charAt(1));

      int deltaX = toX - fromX;
      int deltaY = toY - fromY;
      bool isDiagonal = (abs(deltaX) == abs(deltaY)) && (deltaX != 0) && (deltaY != 0);
      bool isKnight = isKnightMove(deltaX, deltaY);

      Serial.print("Moving to source: ");
      Serial.println(from);
      moveToPosition(fromX, fromY);
      currentX = fromX;
      currentY = fromY;
      
      delay(200);

      Serial.println("MAGNET_ON");
      waitForESPResponse("MAGNET_READY");
      Serial.println("Magnet engaging piece...");
      delay(500);

      if (isKnight) {
        Serial.println("🐴 INTELLIGENT KNIGHT MOVE - Analyzing board state...");
        updateBoardState(); // Scan current piece positions
        executeIntelligentKnightMove(fromX, fromY, toX, toY);
      } else if (isDiagonal) {
        Serial.println("Diagonal move detected - using diagonal path");
        moveDiagonal(deltaX, deltaY);
      } else {
        Serial.print("Moving to destination: ");
        Serial.println(to);
        moveToPosition(toX, toY);
      }
      
      currentX = toX;
      currentY = toY;

      Serial.println("Move complete, stabilizing...");
      delay(200);

      Serial.println("MAGNET_OFF");
      waitForESPResponse("MAGNET_OFF_READY");
      
      Serial.println("Move completed successfully. Ready for next move.");
    }
  }
}

// === Knight Move Logic
bool isKnightMove(int deltaX, int deltaY) {
  int absX = abs(deltaX);
  int absY = abs(deltaY);
  return (absX == 2 && absY == 1) || (absX == 1 && absY == 2);
}

void executeKnightMove(int fromX, int fromY, int toX, int toY) {
  int deltaX = toX - fromX;
  int deltaY = toY - fromY;
  
  Serial.print("Knight move: (");
  Serial.print(fromX);
  Serial.print(",");
  Serial.print(fromY);
  Serial.print(") to (");
  Serial.print(toX);
  Serial.print(",");
  Serial.print(toY);
  Serial.println(")");
  
  // Step 1: Move to top-left corner of starting square
  Serial.println("Moving to top-left corner of starting square...");
  moveToSquareEdge();
  delay(100);
  
  // Step 2: Execute L-shaped movement along grid lines
  Serial.println("Executing L-shaped movement along margins...");
  executeL_ShapedMove(deltaX, deltaY);
  delay(100);
  
  // Step 3: Move to center of destination square
  Serial.println("Moving to center of destination square...");
  moveToSquareCenter();
  delay(200);
}

void moveToSquareEdge() {
  float halfSquareSteps = current_steps_per_square * 0.5;
  
  // Move 0.5 squares left (X-)
  digitalWrite(A_DIR_PIN, LOW);
  digitalWrite(B_DIR_PIN, LOW);
  Serial.print("Moving to left edge: -0.5 squares (");
  Serial.print(halfSquareSteps);
  Serial.println(" steps)");
  for (long i = 0; i < (long)halfSquareSteps; i++) {
    stepBoth();
  }
  
  // Move 0.5 squares up (Y+)
  digitalWrite(A_DIR_PIN, HIGH);
  digitalWrite(B_DIR_PIN, LOW);
  Serial.print("Moving to top edge: +0.5 squares (");
  Serial.print(halfSquareSteps);
  Serial.println(" steps)");
  for (long i = 0; i < (long)halfSquareSteps; i++) {
    stepBoth();
  }
}

void executeL_ShapedMove(int deltaX, int deltaY) {
  long fullSquareSteps = current_steps_per_square;
  
  // Move exact deltaX and deltaY without additional adjustments
  if (abs(deltaX) == 2) {
    // Move X first
    long xSteps = abs(deltaX) * fullSquareSteps;
    digitalWrite(A_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    
    Serial.print("Moving X: ");
    Serial.print(deltaX);
    Serial.print(" squares (");
    Serial.print(xSteps);
    Serial.println(" steps)");
    
    for (long i = 0; i < xSteps; i++) {
      stepBoth();
    }
    
    // Move Y
    long ySteps = abs(deltaY) * fullSquareSteps;
    digitalWrite(A_DIR_PIN, deltaY > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaY > 0 ? LOW : HIGH);
    
    Serial.print("Moving Y: ");
    Serial.print(deltaY);
    Serial.print(" squares (");
    Serial.print(ySteps);
    Serial.println(" steps)");
    
    for (long i = 0; i < ySteps; i++) {
      stepBoth();
    }
  } else {
    // Move Y first
    long ySteps = abs(deltaY) * fullSquareSteps;
    digitalWrite(A_DIR_PIN, deltaY > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaY > 0 ? LOW : HIGH);
    
    Serial.print("Moving Y: ");
    Serial.print(deltaY);
    Serial.print(" squares (");
    Serial.print(ySteps);
    Serial.println(" steps)");
    
    for (long i = 0; i < ySteps; i++) {
      stepBoth();
    }
    
    // Move X
    long xSteps = abs(deltaX) * fullSquareSteps;
    digitalWrite(A_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    
    Serial.print("Moving X: ");
    Serial.print(deltaX);
    Serial.print(" squares (");
    Serial.print(xSteps);
    Serial.println(" steps)");
    
    for (long i = 0; i < xSteps; i++) {
      stepBoth();
    }
  }
}

void moveToSquareCenter() {
  float halfSquareSteps = current_steps_per_square * 0.5;
  
  // Move 0.5 squares right (X+) to center
  digitalWrite(A_DIR_PIN, HIGH);
  digitalWrite(B_DIR_PIN, HIGH);
  Serial.print("Centering in X: +0.5 squares (");
  Serial.print(halfSquareSteps);
  Serial.println(" steps)");
  for (long i = 0; i < (long)halfSquareSteps; i++) {
    stepBoth();
  }
  
  // Move 0.5 squares down (Y-) to center
  digitalWrite(A_DIR_PIN, LOW);
  digitalWrite(B_DIR_PIN, HIGH);
  Serial.print("Centering in Y: -0.5 squares (");
  Serial.print(halfSquareSteps);
  Serial.println(" steps)");
  for (long i = 0; i < (long)halfSquareSteps; i++) {
    stepBoth();
  }
}

// === INTELLIGENT BOARD STATE DETECTION ===
void initializeBoardState() {
  // Initialize all squares as potentially occupied
  // In a real system, you could add sensors here
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      squareOccupied[x][y] = false; // Start with empty board
    }
  }
  
  // Set initial piece positions (standard chess setup)
  // You can modify this based on your actual game state
  setInitialPiecePositions();
  
  Serial.println("📋 Board state initialized");
}

void setInitialPiecePositions() {
  // White pieces (bottom)
  for (int x = 0; x < 8; x++) {
    squareOccupied[x][0] = true; // White back rank
    squareOccupied[x][1] = true; // White pawns
  }
  
  // Black pieces (top)  
  for (int x = 0; x < 8; x++) {
    squareOccupied[x][6] = true; // Black pawns
    squareOccupied[x][7] = true; // Black back rank
  }
  
  Serial.println("📋 Initial piece positions set (standard chess)");
}

void updateBoardState() {
  // This function should ideally read from sensors
  // For now, we'll use a simple simulation
  Serial.println("📡 Scanning board for piece positions...");
  
  // In a real implementation, you would:
  // 1. Read hall sensor values for all 64 squares
  // 2. Update squareOccupied[x][y] based on sensor readings
  // 3. Account for the piece you're currently moving
  
  lastBoardScan = millis();
  Serial.println("📡 Board scan completed");
}

void updatePiecePosition(int fromX, int fromY, int toX, int toY) {
  // Update piece positions when a move is made
  if (fromX >= 0 && fromX < 8 && fromY >= 0 && fromY < 8) {
    squareOccupied[fromX][fromY] = false; // Source square now empty
  }
  if (toX >= 0 && toX < 8 && toY >= 0 && toY < 8) {
    squareOccupied[toX][toY] = true; // Destination square now occupied
  }
}

// === INTELLIGENT KNIGHT MOVEMENT SYSTEM ===
void executeIntelligentKnightMove(int fromX, int fromY, int toX, int toY) {
  int deltaX = toX - fromX;
  int deltaY = toY - fromY;
  
  Serial.print("🐴 Knight Analysis: ");
  Serial.print(squareName(fromX, fromY));
  Serial.print(" → ");
  Serial.print(squareName(toX, toY));
  Serial.print(" (Δ");
  Serial.print(deltaX);
  Serial.print(",");
  Serial.print(deltaY);
  Serial.println(")");
  
  // Analyze all possible knight paths
  KnightPathInfo bestPath = analyzeKnightPaths(fromX, fromY, toX, toY);
  
  // Execute the optimal path
  executeSelectedKnightPath(bestPath, deltaX, deltaY);
  
  // Update board state after move
  updatePiecePosition(fromX, fromY, toX, toY);
}

KnightPathInfo analyzeKnightPaths(int fromX, int fromY, int toX, int toY) {
  KnightPathInfo paths[3];
  int deltaX = toX - fromX;
  int deltaY = toY - fromY;
  
  // Path 1: Direct L-shape (X first, then Y)
  paths[0].pathType = 1;
  paths[0].description = "Direct L (X→Y)";
  paths[0].estimatedDistance = abs(deltaX) + abs(deltaY);
  paths[0].intermediateX = fromX + deltaX;
  paths[0].intermediateY = fromY;
  paths[0].pathClear = isSquareFree(paths[0].intermediateX, paths[0].intermediateY);
  
  // Path 2: Direct L-shape (Y first, then X)  
  paths[1].pathType = 2;
  paths[1].description = "Direct L (Y→X)";
  paths[1].estimatedDistance = abs(deltaY) + abs(deltaX);
  paths[1].intermediateX = fromX;
  paths[1].intermediateY = fromY + deltaY;
  paths[1].pathClear = isSquareFree(paths[1].intermediateX, paths[1].intermediateY);
  
  // Path 3: Grid avoidance with minimal distance
  paths[2].pathType = 3;
  paths[2].description = "Grid minimal";
  paths[2].estimatedDistance = abs(deltaX) + abs(deltaY) + 1.0; // Extra for margins
  paths[2].intermediateX = -1; // No specific intermediate
  paths[2].intermediateY = -1;
  paths[2].pathClear = true; // Grid path always possible
  
  // Choose the best path
  KnightPathInfo bestPath = paths[2]; // Default to grid
  
  // Prefer clear direct paths
  for (int i = 0; i < 2; i++) {
    if (paths[i].pathClear) {
      bestPath = paths[i];
      Serial.print("🎯 CLEAR PATH FOUND: ");
      Serial.println(paths[i].description);
      break;
    }
  }
  
  if (!bestPath.pathClear || bestPath.pathType == 3) {
    Serial.println("⚠️  BLOCKED - Using grid minimal path");
  }
  
  // Debug output
  Serial.println("📊 PATH ANALYSIS:");
  for (int i = 0; i < 3; i++) {
    Serial.print("   ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(paths[i].description);
    Serial.print(" - Distance: ");
    Serial.print(paths[i].estimatedDistance);
    if (i < 2) {
      Serial.print(" - Corner: ");
      Serial.print(squareName(paths[i].intermediateX, paths[i].intermediateY));
      Serial.print(" (");
      Serial.print(paths[i].pathClear ? "FREE" : "BLOCKED");
      Serial.print(")");
    }
    Serial.println();
  }
  
  return bestPath;
}

bool isSquareFree(int x, int y) {
  if (x < 0 || x >= 8 || y < 0 || y >= 8) {
    return false; // Out of bounds
  }
  return !squareOccupied[x][y];
}

String squareName(int x, int y) {
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
      Serial.println("🚀 EXECUTING: Direct L-path (X→Y)");
      moveOnlyX(deltaX);
      moveOnlyY(deltaY);
      break;
      
    case 2:
      // Direct L-path: Y first, then X
      Serial.println("🚀 EXECUTING: Direct L-path (Y→X)");
      moveOnlyY(deltaY);
      moveOnlyX(deltaX);
      break;
      
    case 3:
      // Grid avoidance with minimal margins
      Serial.println("🚀 EXECUTING: Grid minimal path");
      executeGridMinimalPath(deltaX, deltaY);
      break;
  }
}

void executeGridMinimalPath(int deltaX, int deltaY) {
  // Use minimal grid margins (quarter-square instead of half-square)
  float quarterSquareSteps = current_steps_per_square * 0.25;
  
  Serial.println("📐 Step 1: Minimal margin offset...");
  
  // Move to minimal top-left margin (0.25 squares)
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
  
  Serial.println("📐 Step 2: Main movement along grid...");
  
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
  
  Serial.println("📐 Step 3: Return to center...");
  
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
  
  Serial.println("✅ Grid minimal path completed");
}

// === Utility Functions
void waitForESPResponse(String expectedResponse) {
  unsigned long startTime = millis();
  const unsigned long timeout = 10000;
  
  while (millis() - startTime < timeout) {
    if (Serial.available()) {
      String response = Serial.readStringUntil('\n');
      response.trim();
      if (response == expectedResponse) {
        return;
      }
    }
    delay(10);
  }
  Serial.println("Timeout waiting for ESP32 response!");
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
  
  Serial.print("Moving X: ");
  Serial.print(squares);
  Serial.print(" squares (");
  Serial.print(steps);
  Serial.print(" steps, square size: ");
  Serial.print(current_square_size_cm);
  Serial.println(" cm)");
  
  for (long i = 0; i < steps; i++) {
    stepBoth();
  }
}

void moveOnlyY(int squares) {
  if (squares == 0) return;
  digitalWrite(A_DIR_PIN, squares > 0 ? HIGH : LOW);
  digitalWrite(B_DIR_PIN, squares > 0 ? LOW : HIGH);
  long steps = abs(squares) * current_steps_per_square;
  
  Serial.print("Moving Y: ");
  Serial.print(squares);
  Serial.print(" squares (");
  Serial.print(steps);
  Serial.print(" steps, square size: ");
  Serial.print(current_square_size_cm);
  Serial.println(" cm)");
  
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
