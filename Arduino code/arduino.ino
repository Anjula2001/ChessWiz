// === ARDUINO CHESS MOTOR CONTROLLER - MEMORY OPTIMIZED ===
// 
// 🎯 ENHANCED FEATURES:
// - Intelligent piece detection system
// - Smart knight path analysis
// - Direct L-path when clear
// - Minimal grid avoidance when blocked
// - Real-time board state tracking
// - Optimal distance calculation
// - SIMPLIFIED MOVE HANDLING with direct execution
//
// 🐴 KNIGHT OPTIMIZATION:
// - Analyzes all 64 squares for piece positions
// - Chooses shortest clear path automatically
// - Uses quarter-square margins when grid needed
// - Eliminates unnecessary movements
//
// 🎯 SIMPLIFIED SYSTEM:
// - All moves execute directly without capture detection
// - No piece removal sequences
// - Direct movement for all pieces

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
const int step_delay = 350; // SPEED OPTIMIZED: Reduced from 450 to 350 microseconds for faster motor movement
const long steps_per_cm = 419;
const float initial_square_size_cm = 4.6;    // Square size before reaching (0,0) - for offset
const float final_square_size_cm = 5.34;     // Square size after reaching (0,0) - for moves
float current_square_size_cm = initial_square_size_cm;
long current_steps_per_square = steps_per_cm * initial_square_size_cm;
long steps_per_square = current_steps_per_square; // For compatibility with existing functions

// === Current position (chess grid coordinate)
int currentX = -2;
int currentY = 0;
bool hasReachedOrigin = false;

// === INTELLIGENT PIECE DETECTION SYSTEM ===
// Optimized: Use bit array instead of bool array to save memory
byte squareOccupied[8] = {0}; // 8 bytes instead of 64 bytes - each bit represents one square
unsigned long lastBoardScan = 0;
const unsigned long BOARD_SCAN_INTERVAL = 1000; // Scan every 1 second

// Memory-optimized Knight path analysis structure
struct KnightPathInfo {
  byte pathType;           // 1=Direct X→Y, 2=Direct Y→X, 3=Grid minimal
  float estimatedDistance;
  bool pathClear;
  byte intermediateX, intermediateY; // Corner square of L-path
};

// === Setup
void setup() {
  Serial.begin(115200); // Keep optimized serial speed

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
  delay(100); // SPEED OPTIMIZED: Reduced from 200ms for faster initialization

  applyOffset(0.4, 0.85);
  delay(100); // SPEED OPTIMIZED: Reduced from 200ms

  moveOnlyX(2);
  currentX = 0;
  currentY = 0;
  
  updateSquareSize(); // Update to final square size after reaching origin
  
  // ENHANCED DEBUGGING: Confirm initial position
  Serial.print(F("🎯 INITIAL POSITION SET: ("));
  Serial.print(currentX);
  Serial.print(F(","));
  Serial.print(currentY);
  Serial.print(F(") = "));
  Serial.println(getSquareName(currentX, currentY));
  Serial.println(F("📍 COORDINATE SYSTEM: (0,0)=h1, (1,0)=g1, (7,0)=a1"));
  
  Serial.println(F("READY - Memory Optimized System Active"));
  Serial.println(F("🐴 Knight moves optimized"));
  Serial.println(F("📡 Board state tracking enabled"));
  Serial.println(F("🔧 Enhanced debugging enabled"));
  Serial.println(F("================================================"));
}

// === Main Loop
void loop() {
  if (Serial.available()) {
    String move = Serial.readStringUntil('\n');
    move.trim();

    // ENHANCED DEBUGGING: Log all received communication
    Serial.print(F("📨 RAW RECEIVED: '"));
    Serial.print(move);
    Serial.print(F("' (Length: "));
    Serial.print(move.length());
    Serial.println(F(")"));

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
      
      // Send confirmation to ESP32
      Serial.println(F("ARDUINO_RESET_COMPLETE"));
      delay(50); // OPTIMIZED: Reduced from 100ms for faster reset response
      
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

    // Handle board state update from ESP32
    if (move.startsWith("BOARD_STATE:")) {
      updateBoardStateFromESP32(move);
      return;
    }

    // ENHANCED DEBUGGING: Check move format and parse
    Serial.print(F("🔍 PARSING MOVE: "));
    Serial.println(move);
    
    if (move.length() == 5 && move.charAt(2) == '-') {
      String from = move.substring(0, 2);
      String to = move.substring(3, 5);

      Serial.print(F("✅ VALID MOVE FORMAT - From: "));
      Serial.print(from);
      Serial.print(F(", To: "));
      Serial.println(to);
      
      // ENHANCED DEBUGGING: Show coordinate conversion
      int fromX = fileToX(from.charAt(0));
      int fromY = rankToY(from.charAt(1));
      int toX = fileToX(to.charAt(0));
      int toY = rankToY(to.charAt(1));
      
      Serial.print(F("📍 COORDINATE CONVERSION:"));
      Serial.print(F(" From "));
      Serial.print(from);
      Serial.print(F("("));
      Serial.print(fromX);
      Serial.print(F(","));
      Serial.print(fromY);
      Serial.print(F(") → To "));
      Serial.print(to);
      Serial.print(F("("));
      Serial.print(toX);
      Serial.print(F(","));
      Serial.print(toY);
      Serial.println(F(")"));
      
      // ENHANCED DEBUGGING: Show current position before move
      Serial.print(F("🎯 CURRENT POSITION BEFORE MOVE: ("));
      Serial.print(currentX);
      Serial.print(F(","));
      Serial.print(currentY);
      Serial.println(F(")"));

      int deltaX = toX - fromX;
      int deltaY = toY - fromY;
      bool isDiagonal = (abs(deltaX) == abs(deltaY)) && (deltaX != 0) && (deltaY != 0);
      bool isKnight = isKnightMove(deltaX, deltaY);

      // ENHANCED DEBUGGING: Movement analysis
      Serial.print(F("📐 MOVEMENT ANALYSIS: Delta("));
      Serial.print(deltaX);
      Serial.print(F(","));
      Serial.print(deltaY);
      Serial.print(F(") - "));
      if (isKnight) {
        Serial.println(F("KNIGHT MOVE"));
      } else if (isDiagonal) {
        Serial.println(F("DIAGONAL MOVE"));
      } else {
        Serial.println(F("STRAIGHT MOVE"));
      }

      // Request current board state from ESP32 before moving (only if sensors are enabled)
      Serial.println(F("REQUEST_BOARD_STATE"));
      waitForBoardStateResponse();

      Serial.print(F("🚀 STARTING MOVE EXECUTION: "));
      Serial.println(move);
      Serial.print(F("Moving to source: "));
      Serial.println(from);
      moveToPosition(fromX, fromY);
      currentX = fromX;
      currentY = fromY;
      
      // ENHANCED DEBUGGING: Position after moving to source
      Serial.print(F("✅ REACHED SOURCE - Current Position: ("));
      Serial.print(currentX);
      Serial.print(F(","));
      Serial.print(currentY);
      Serial.println(F(")"));
      
      // Update board state - piece lifted from source
      updatePiecePosition(fromX, fromY, -1, -1);
      
      delay(50); // SPEED OPTIMIZED: Reduced from 100ms for faster piece pickup

      Serial.println(F("MAGNET_ON"));
      waitForESPResponse(F("MAGNET_READY"));
      delay(50); // SPEED OPTIMIZED: Reduced from 200ms for faster magnet engagement

      if (isKnight) {
        Serial.println(F("🐴 EXECUTING INTELLIGENT KNIGHT MOVE"));
        executeIntelligentKnightMove(fromX, fromY, toX, toY);
      } else if (isDiagonal) {
        Serial.println(F("📐 EXECUTING DIAGONAL MOVE"));
        moveDiagonal(deltaX, deltaY);
      } else {
        Serial.print(F("➡ EXECUTING STRAIGHT MOVE to: "));
        Serial.println(to);
        moveToPosition(toX, toY);
      }
      
      currentX = toX;
      currentY = toY;
      
      // ENHANCED DEBUGGING: Final position confirmation
      Serial.print(F("🏁 FINAL POSITION: ("));
      Serial.print(currentX);
      Serial.print(F(","));
      Serial.print(currentY);
      Serial.print(F(") = "));
      Serial.println(getSquareName(currentX, currentY));
      
      // Update board state - piece placed at destination
      updatePiecePosition(-1, -1, toX, toY);

      Serial.println(F("Move complete"));
      delay(25); // SPEED OPTIMIZED: Reduced from 100ms for faster move completion

      Serial.println(F("MAGNET_OFF"));
      waitForESPResponse(F("MAGNET_OFF_READY"));
      
      // 🏰 CHECK IF THIS WAS A CASTLING MOVE AFTER KING IS MOVED
      if (isCastlingMove(move)) {
        Serial.println(F("🏰 CASTLING DETECTED - Now moving rook automatically"));
        handleCastlingAfterKingMove(move);
      }
      
      Serial.println(F("✅ MOVE COMPLETED SUCCESSFULLY. Ready for next move."));
      Serial.println(F("================================================"));
    } else {
      // ENHANCED DEBUGGING: Invalid move format
      Serial.print(F("❌ INVALID MOVE FORMAT: '"));
      Serial.print(move);
      Serial.print(F("' - Expected format: XX-XX (e.g., e2-e4)"));
      Serial.print(F(" - Length: "));
      Serial.print(move.length());
      if (move.length() > 0) {
        Serial.print(F(" - Char at 2: '"));
        if (move.length() > 2) {
          Serial.print(move.charAt(2));
        } else {
          Serial.print(F("N/A"));
        }
        Serial.print(F("'"));
      }
      Serial.println();
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

void updateBoardStateFromESP32(String boardStateMessage) {
  // Parse board state message from ESP32
  // Format: "BOARD_STATE:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
  // Each character represents one square (0=empty, 1=occupied)
  
  if (boardStateMessage.length() < 75) { // "BOARD_STATE:" + 64 characters
    Serial.println(F("❌ Invalid board state message"));
    return;
  }
  
  String boardData = boardStateMessage.substring(12); // Remove "BOARD_STATE:" prefix
  
  if (boardData.length() != 64) {
    Serial.println(F("❌ Board state data must be 64 characters"));
    return;
  }
  
  Serial.println(F("📡 Updating board state from ESP32..."));
  
  // Clear current board state
  for (byte i = 0; i < 8; i++) {
    squareOccupied[i] = 0;
  }
  
  // Update board state from ESP32 sensor data
  for (int i = 0; i < 64; i++) {
    int x = i % 8;
    int y = i / 8;
    bool occupied = (boardData.charAt(i) == '1');
    setSquareOccupied(x, y, occupied);
  }
  
  Serial.println(F("✅ Board state synchronized with ESP32"));
  
  // Print board state for debugging
  printBoardState();
}

void printBoardState() {
  Serial.println(F("📋 Current Board State:"));
  Serial.println(F("   a b c d e f g h"));
  
  for (int y = 7; y >= 0; y--) { // Print from rank 8 to rank 1
    Serial.print(y + 1);
    Serial.print(F("  "));
    
    for (int x = 0; x < 8; x++) {
      Serial.print(getSquareOccupied(x, y) ? "X" : ".");
      Serial.print(" ");
    }
    Serial.println();
  }
  Serial.println();
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

// CRITICAL: Add missing updateSquareSize function from working version
void updateSquareSize() {
  if (!hasReachedOrigin) {
    hasReachedOrigin = true;
    current_square_size_cm = final_square_size_cm;
    current_steps_per_square = steps_per_cm * final_square_size_cm;
    steps_per_square = current_steps_per_square; // Update compatibility variable
    Serial.println(F("✅ Updated to final square size: 5.34cm"));
  }
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
  
  Serial.print(F("🐴 Knight Analysis: "));
  Serial.print(getSquareName(fromX, fromY));
  Serial.print(F(" → "));
  Serial.print(getSquareName(toX, toY));
  Serial.print(F(" (Δ"));
  Serial.print(deltaX);
  Serial.print(F(","));
  Serial.print(deltaY);
  Serial.println(F(")"));
  
  // Analyze all possible knight paths
  KnightPathInfo bestPath = analyzeKnightPaths(fromX, fromY, toX, toY);
  
  // Execute the optimal path
  executeSelectedKnightPath(bestPath, deltaX, deltaY);
}

KnightPathInfo analyzeKnightPaths(int fromX, int fromY, int toX, int toY) {
  KnightPathInfo bestPath;
  int deltaX = toX - fromX;
  int deltaY = toY - fromY;
  
  Serial.print(F("🔍 Analyzing knight paths: "));
  Serial.print(getSquareName(fromX, fromY));
  Serial.print(F("→"));
  Serial.print(getSquareName(toX, toY));
  Serial.print(F(" Δ("));
  Serial.print(deltaX);
  Serial.print(F(","));
  Serial.print(deltaY);
  Serial.println(F(")"));
  
  // PRIORITY 1: Check if there are pieces in the knight's path area FIRST
  bool pathAreaClear = checkKnightPathArea(fromX, fromY, toX, toY);
  
  if (!pathAreaClear) {
    // FORCE edge movement when ANY pieces detected in path area
    bestPath.pathType = 3;
    bestPath.pathClear = false;
    bestPath.estimatedDistance = abs(deltaX) + abs(deltaY);
    bestPath.intermediateX = 255; // No specific intermediate for edge movement
    bestPath.intermediateY = 255;
    Serial.println(F("⚠ PIECES DETECTED in knight path area - FORCING edge movement"));
    return bestPath; // Exit early - don't check corner squares
  }
  
  // PRIORITY 2: Only check corner squares if path area is completely clear
  Serial.println(F("✅ Path area completely clear - analyzing corner squares for optimal L-path"));
  
  // Check Path 1: Direct L-shape (X first, then Y)
  int cornerX1 = fromX + deltaX;
  int cornerY1 = fromY;
  bool path1Clear = isSquareFree(cornerX1, cornerY1);
  
  // Check Path 2: Direct L-shape (Y first, then X)
  int cornerX2 = fromX;
  int cornerY2 = fromY + deltaY;
  bool path2Clear = isSquareFree(cornerX2, cornerY2);
  
  Serial.print(F("🎯 Path 1 (X→Y): "));
  Serial.print(getSquareName(fromX, fromY));
  Serial.print(F("→"));
  Serial.print(getSquareName(cornerX1, cornerY1));
  Serial.print(F("→"));
  Serial.print(getSquareName(toX, toY));
  Serial.print(F(" - "));
  Serial.println(path1Clear ? F("CLEAR ✅") : F("BLOCKED ❌"));
  
  Serial.print(F("🎯 Path 2 (Y→X): "));
  Serial.print(getSquareName(fromX, fromY));
  Serial.print(F("→"));
  Serial.print(getSquareName(cornerX2, cornerY2));
  Serial.print(F("→"));
  Serial.print(getSquareName(toX, toY));
  Serial.print(F(" - "));
  Serial.println(path2Clear ? F("CLEAR ✅") : F("BLOCKED ❌"));
  
  // Choose best direct path when area is clear
  if (path1Clear && path2Clear) {
    bestPath.pathType = (abs(deltaX) >= abs(deltaY)) ? 1 : 2;
    bestPath.pathClear = true;
    Serial.println(F("✅ Both corner paths clear - using optimal direct L-path"));
  } else if (path1Clear) {
    bestPath.pathType = 1;
    bestPath.pathClear = true;
    Serial.println(F("✅ Using Path 1 (X→Y) - corner clear"));
  } else if (path2Clear) {
    bestPath.pathType = 2;
    bestPath.pathClear = true;
    Serial.println(F("✅ Using Path 2 (Y→X) - corner clear"));
  } else {
    // Both corner squares blocked even though area is clear
    bestPath.pathType = 3;
    bestPath.pathClear = false;
    Serial.println(F("⚠ Both corner squares BLOCKED - using edge movement"));
  }
  
  bestPath.estimatedDistance = abs(deltaX) + abs(deltaY);
  bestPath.intermediateX = (bestPath.pathType == 1) ? cornerX1 : cornerX2;
  bestPath.intermediateY = (bestPath.pathType == 1) ? cornerY1 : cornerY2;
  
  return bestPath;
}

// New function to check if pieces are in the knight's path area
bool checkKnightPathArea(int fromX, int fromY, int toX, int toY) {
  int deltaX = toX - fromX;
  int deltaY = toY - fromY;
  
  // Define the rectangular area between source and destination
  int minX = min(fromX, toX);
  int maxX = max(fromX, toX);
  int minY = min(fromY, toY);
  int maxY = max(fromY, toY);
  
  Serial.print(F("🔍 Checking path area: "));
  Serial.print(getSquareName(minX, minY));
  Serial.print(F(" to "));
  Serial.println(getSquareName(maxX, maxY));
  
  // Check all squares in the path rectangle (excluding source and destination)
  for (int x = minX; x <= maxX; x++) {
    for (int y = minY; y <= maxY; y++) {
      if ((x == fromX && y == fromY) || (x == toX && y == toY)) {
        continue; // Skip source and destination squares
      }
      
      if (!isSquareFree(x, y)) {
        Serial.print(F("❌ Piece found at "));
        Serial.print(getSquareName(x, y));
        Serial.println(F(" - path area blocked"));
        return false; // Path area has pieces
      }
    }
  }
  
  Serial.println(F("✅ Path area is clear of pieces"));
  return true; // Path area is clear
}

bool isSquareFree(int x, int y) {
  if (x < 0 || x >= 8 || y < 0 || y >= 8) {
    return false; // Out of bounds
  }
  return !getSquareOccupied(x, y);
}

String getSquareName(int x, int y) {
  if (x < 0 || x >= 8 || y < 0 || y >= 8) {
    return F("??");
  }
  char file = 'a' + (7 - x);  // ORIGINAL WORKING: x=0 -> 'h', x=1 -> 'g', ..., x=7 -> 'a'
  char rank = '1' + y;  // y=0 -> '1', y=1 -> '2', ..., y=7 -> '8'
  return String(file) + String(rank);
}

void executeSelectedKnightPath(KnightPathInfo path, int deltaX, int deltaY) {
  Serial.println(F("🚀 EXECUTING KNIGHT MOVEMENT:"));
  
  switch (path.pathType) {
    case 1:
      // Direct L-path: X first, then Y (corner square is clear)
      Serial.println(F("     Direct L-path: X→Y movement"));
      Serial.print(F("   Step 1: Move "));
      Serial.print(deltaX);
      Serial.print(F(" squares "));
      Serial.println(deltaX > 0 ? F("right (X+)") : F("left (X-)"));
      moveOnlyX(deltaX);
      
      Serial.print(F("   Step 2: Move "));
      Serial.print(deltaY);
      Serial.print(F(" squares "));
      Serial.println(deltaY > 0 ? F("up (Y+)") : F("down (Y-)"));
      moveOnlyY(deltaY);
      break;
      
    case 2:
      // Direct L-path: Y first, then X (corner square is clear)
      Serial.println(F("     Direct L-path: Y→X movement"));
      Serial.print(F("   Step 1: Move "));
      Serial.print(deltaY);
      Serial.print(F(" squares "));
      Serial.println(deltaY > 0 ? F("up (Y+)") : F("down (Y-)"));
      moveOnlyY(deltaY);
      
      Serial.print(F("   Step 2: Move "));
      Serial.print(deltaX);
      Serial.print(F(" squares "));
      Serial.println(deltaX > 0 ? F("right (X+)") : F("left (X-)"));
      moveOnlyX(deltaX);
      break;
      
    case 3:
      // Edge movement between squares (both corner squares blocked)
      Serial.println(F("     Edge movement: Traveling between squares to avoid pieces"));
      executeGridMinimalPath(deltaX, deltaY);
      break;
      
    default:
      Serial.println(F("❌ ERROR: Unknown knight path type"));
      break;
  }
  
  Serial.println(F("✅ Knight movement sequence completed successfully"));
}

void executeGridMinimalPath(int deltaX, int deltaY) {
  // Enhanced knight edge movement - moves between squares when L-path blocked
  Serial.println(F("🐴 EDGE MOVEMENT: Knight moving between squares to avoid pieces"));
  
  long halfSquareSteps = steps_per_square / 2; // ORIGINAL WORKING: Use simple steps_per_square
  long fullSquareSteps = steps_per_square;
  
  // Example: g1→f3 (deltaX=-1, deltaY=+2) when g2 is blocked
  // Solution: g1 → (g1+f1)/2 → f3 (move to edge between g1-f1, then straight up)
  
  Serial.println(F("📐 Step 1: Move to edge between squares..."));
  
  // Move to the edge (halfway point) in the shorter direction first
  if (abs(deltaX) == 1 && abs(deltaY) == 2) {
    // Move halfway in X direction (to edge between current and target file)
    Serial.print(F("   Moving "));
    Serial.print(deltaX > 0 ? F("right") : F("left"));
    Serial.println(F(" to edge (0.5 squares)"));
    
    // X MOVEMENT: Both motors same direction
    digitalWrite(A_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    for (long i = 0; i < halfSquareSteps; i++) {
      stepBoth();
    }
    
    // Now move full distance in Y direction along the edge
    Serial.print(F("   Moving "));
    Serial.print(deltaY > 0 ? F("up") : F("down"));
    Serial.print(F(" along edge ("));
    Serial.print(deltaY);
    Serial.println(F(" squares)"));
    
    // Y MOVEMENT: Motors opposite directions
    digitalWrite(A_DIR_PIN, deltaY > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaY > 0 ? LOW : HIGH);
    for (long i = 0; i < abs(deltaY) * fullSquareSteps; i++) {
      stepBoth();
    }
    
    // Complete the final half square in X direction
    Serial.println(F("   Completing final 0.5 squares to destination"));
    
    // X MOVEMENT: Both motors same direction
    digitalWrite(A_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    for (long i = 0; i < halfSquareSteps; i++) {
      stepBoth();
    }
    
  } else if (abs(deltaX) == 2 && abs(deltaY) == 1) {
    // Move halfway in Y direction (to edge between current and target rank)
    Serial.print(F("   Moving "));
    Serial.print(deltaY > 0 ? F("up") : F("down"));
    Serial.println(F(" to edge (0.5 squares)"));
    
    // Y MOVEMENT: Motors opposite directions
    digitalWrite(A_DIR_PIN, deltaY > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaY > 0 ? LOW : HIGH);
    for (long i = 0; i < halfSquareSteps; i++) {
      stepBoth();
    }
    
    // Now move full distance in X direction along the edge
    Serial.print(F("   Moving "));
    Serial.print(deltaX > 0 ? F("right") : F("left"));
    Serial.print(F(" along edge ("));
    Serial.print(deltaX);
    Serial.println(F(" squares)"));
    
    // X MOVEMENT: Both motors same direction
    digitalWrite(A_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaX > 0 ? HIGH : LOW);
    for (long i = 0; i < abs(deltaX) * fullSquareSteps; i++) {
      stepBoth();
    }
    
    // Complete the final half square in Y direction
    Serial.println(F("   Completing final 0.5 squares to destination"));
    
    // Y MOVEMENT: Motors opposite directions
    digitalWrite(A_DIR_PIN, deltaY > 0 ? HIGH : LOW);
    digitalWrite(B_DIR_PIN, deltaY > 0 ? LOW : HIGH);
    for (long i = 0; i < halfSquareSteps; i++) {
      stepBoth();
    }
  }
  
  Serial.println(F("✅ Edge movement completed - Knight avoided blocked squares"));
}

// === Utility Functions
void waitForESPResponse(const __FlashStringHelper* expectedResponse) {
  unsigned long startTime = millis();
  const unsigned long timeout = 3000; // SPEED OPTIMIZED: Reduced from 10000ms to 3000ms
  
  while (millis() - startTime < timeout) {
    if (Serial.available()) {
      String response = Serial.readStringUntil('\n');
      response.trim();
      if (response.equals(expectedResponse)) {
        return;
      }
    }
    delay(5); // OPTIMIZED: Reduced from 10ms for faster ESP32 response checking
  }
  Serial.println(F("Timeout waiting for ESP32!"));
}

void waitForBoardStateResponse() {
  unsigned long startTime = millis();
  const unsigned long timeout = 2000; // SPEED OPTIMIZED: Reduced from 5000ms to 2000ms
  
  Serial.println(F("⏳ Waiting for board state from ESP32..."));
  
  while (millis() - startTime < timeout) {
    if (Serial.available()) {
      String response = Serial.readStringUntil('\n');
      response.trim();
      
      if (response.startsWith("BOARD_STATE:")) {
        updateBoardStateFromESP32(response);
        Serial.println(F("✅ Board state received and updated"));
        return;
      } else if (response == "SENSORS_DISABLED") {
        Serial.println(F("⚠ ESP32 sensors disabled - using cached board state"));
        return;
      } else if (response.startsWith("MAGNET") || response.startsWith("ARDUINO")) {
        // Handle other ESP32 responses but continue waiting for board state
        continue;
      }
    }
    delay(5); // OPTIMIZED: Reduced from 10ms for faster board state response
  }
  
  Serial.println(F("⚠ Timeout waiting for board state - using cached state"));
}

void moveDiagonal(int deltaX, int deltaY) {
  long stepsX = abs(deltaX) * steps_per_square;
  long stepsY = abs(deltaY) * steps_per_square;

  // ORIGINAL WORKING: Determine direction for each motor exactly like before
  bool xDir = deltaX > 0 ? HIGH : LOW; // A and B same direction for X
  bool yDirA = deltaY > 0 ? HIGH : LOW; // A motor for Y
  bool yDirB = deltaY > 0 ? LOW : HIGH; // B motor opposite for Y

  // ORIGINAL WORKING: Set directions based on quadrant
  digitalWrite(A_DIR_PIN, deltaX > 0 ? HIGH : LOW);
  digitalWrite(B_DIR_PIN, deltaX > 0 ? HIGH : LOW); // Start with X direction

  // ORIGINAL WORKING: Bresenham-like interpolation
  long totalSteps = max(stepsX, stepsY); // Use the larger number of steps
  long error = 0;
  long deltaError = min(stepsX, stepsY);
  long majorSteps = max(stepsX, stepsY);
  bool xMajor = stepsX >= stepsY;

  Serial.print(F("Diagonal move: deltaX="));
  Serial.print(deltaX);
  Serial.print(F(", deltaY="));
  Serial.print(deltaY);
  Serial.print(F(", square size="));
  Serial.print(current_square_size_cm);
  Serial.println(F(" cm"));

  for (long i = 0; i < totalSteps; i++) {
    if (xMajor) {
      // ORIGINAL WORKING: Step X (both motors same direction)
      digitalWrite(A_DIR_PIN, xDir);
      digitalWrite(B_DIR_PIN, xDir);
      stepBoth();
      error += deltaError;
      if (error >= majorSteps) {
        // ORIGINAL WORKING: Step Y (motors opposite directions)
        digitalWrite(A_DIR_PIN, yDirA);
        digitalWrite(B_DIR_PIN, yDirB);
        stepBoth();
        error -= majorSteps;
      }
    } else {
      // ORIGINAL WORKING: Step Y (motors opposite directions)
      digitalWrite(A_DIR_PIN, yDirA);
      digitalWrite(B_DIR_PIN, yDirB);
      stepBoth();
      error += deltaError;
      if (error >= majorSteps) {
        // ORIGINAL WORKING: Step X (both motors same direction)
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
  
  Serial.print(F("📍 Position Move: ("));
  Serial.print(currentX);
  Serial.print(F(","));
  Serial.print(currentY);
  Serial.print(F(") → ("));
  Serial.print(targetX);
  Serial.print(F(","));
  Serial.print(targetY);
  Serial.print(F(") | Delta: ("));
  Serial.print(deltaX);
  Serial.print(F(","));
  Serial.print(deltaY);
  Serial.println(F(")"));
  
  // ORIGINAL WORKING: L-shaped path: X first, then Y
  if (deltaX != 0) moveOnlyX(deltaX);
  if (deltaY != 0) moveOnlyY(deltaY);
}

int fileToX(char file) {
  return 7 - (file - 'a');  // ORIGINAL WORKING: a=7, b=6, c=5, d=4, e=3, f=2, g=1, h=0
}

int rankToY(char rank) {
  return rank - '1';  // 1=0, 2=1, 3=2, ..., 8=7
}

void moveOnlyX(int squares) {
  if (squares == 0) return;
  
  Serial.print(F("🔧 X Movement: "));
  Serial.print(squares);
  Serial.print(F(" squares "));
  Serial.print(squares > 0 ? F("RIGHT (+X)") : F("LEFT (-X)"));
  Serial.print(F(" | Motors: A="));
  Serial.print(squares > 0 ? F("HIGH") : F("LOW"));
  Serial.print(F(", B="));
  Serial.println(squares > 0 ? F("HIGH") : F("LOW"));
  
  // ORIGINAL WORKING: Both motors same direction for X movement
  digitalWrite(A_DIR_PIN, squares > 0 ? HIGH : LOW);
  digitalWrite(B_DIR_PIN, squares > 0 ? HIGH : LOW);  // Both motors same direction for X movement
  long steps = abs(squares) * steps_per_square;
  
  for (long i = 0; i < steps; i++) {
    stepBoth();
  }
}

void moveOnlyY(int squares) {
  if (squares == 0) return;
  
  Serial.print(F("🔧 Y Movement: "));
  Serial.print(squares);
  Serial.print(F(" squares "));
  Serial.print(squares > 0 ? F("UP (+Y)") : F("DOWN (-Y)"));
  Serial.print(F(" | Motors: A="));
  Serial.print(squares > 0 ? F("HIGH") : F("LOW"));
  Serial.print(F(", B="));
  Serial.println(squares > 0 ? F("LOW") : F("HIGH"));
  
  // ORIGINAL WORKING: A motor direction, B motor opposite direction for Y movement
  digitalWrite(A_DIR_PIN, squares > 0 ? HIGH : LOW);   // A motor direction
  digitalWrite(B_DIR_PIN, squares > 0 ? LOW : HIGH);   // B motor opposite direction for Y movement
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

void homeToOrigin() {
  digitalWrite(A_DIR_PIN, LOW);
  digitalWrite(B_DIR_PIN, HIGH);
  while (digitalRead(Y_LIMIT_PIN) == HIGH) {
    stepBoth();
  }
  delay(100); // SPEED OPTIMIZED: Reduced from 150ms for faster homing

  digitalWrite(A_DIR_PIN, LOW);
  digitalWrite(B_DIR_PIN, LOW);
  while (digitalRead(X_LIMIT_PIN) == HIGH) {
    stepBoth();
  }
  delay(150); // SPEED OPTIMIZED: Reduced from 250ms for faster homing

  currentX = -2;
  currentY = 0;
  Serial.println(F("Homed to (-2, 0)"));
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

  Serial.println(F("Offset applied"));
}

// ==========================================
// 🏰 CASTLING DETECTION AND EXECUTION
// ==========================================

bool isCastlingMove(String move) {
  return (move == F("e1-g1") || move == F("e1-c1") || 
          move == F("e8-g8") || move == F("e8-c8"));
}

void handleCastlingAfterKingMove(String kingMove) {
  Serial.print(F("🏰 CASTLING DETECTED AFTER KING MOVE: "));
  Serial.println(kingMove);
  
  if (kingMove == F("e1-g1")) {      // White kingside castling
    Serial.println(F("🏰 WHITE KINGSIDE CASTLING - Moving rook h1→f1"));
    executeRookCastlingMove(F("h1"), F("f1"));
  } 
  else if (kingMove == F("e1-c1")) { // White queenside castling
    Serial.println(F("🏰 WHITE QUEENSIDE CASTLING - Moving rook a1→d1"));
    executeRookCastlingMove(F("a1"), F("d1"));
  }
  else if (kingMove == F("e8-g8")) { // Black kingside castling
    Serial.println(F("🏰 BLACK KINGSIDE CASTLING - Moving rook h8→f8"));
    executeRookCastlingMove(F("h8"), F("f8"));
  }
  else if (kingMove == F("e8-c8")) { // Black queenside castling
    Serial.println(F("🏰 BLACK QUEENSIDE CASTLING - Moving rook a8→d8"));
    executeRookCastlingMove(F("a8"), F("d8"));
  }
  
  Serial.println(F("✅ CASTLING COMPLETED SUCCESSFULLY"));
}

void executeRookCastlingMove(String rookFrom, String rookTo) {
  Serial.print(F("🔰 ROOK CASTLING MOVE: "));
  Serial.print(rookFrom);
  Serial.print(F(" → "));
  Serial.print(rookTo);
  Serial.println(F(" (via top edge)"));
  
  int fromX = fileToX(rookFrom.charAt(0));
  int fromY = rankToY(rookFrom.charAt(1));
  int toX = fileToX(rookTo.charAt(0));
  int toY = rankToY(rookTo.charAt(1));
  
  // Step 1: Move to rook source position
  Serial.print(F("Step 1: Moving to rook source "));
  Serial.println(rookFrom);
  moveToPosition(fromX, fromY);
  currentX = fromX;
  currentY = fromY;
  
  // Step 2: Pick up rook
  Serial.println(F("Step 2: Picking up rook"));
  Serial.println(F("MAGNET_ON"));
  waitForESPResponse(F("MAGNET_READY"));
  delay(50);
  
  // Step 3: Move to top edge of source square
  Serial.println(F("Step 3: Moving to top edge of source square"));
  moveToTopEdge();
  
  // Step 4: Travel along top edge to destination file
  int fileDistance = toX - fromX;
  Serial.print(F("Step 4: Traveling along top edge ("));
  Serial.print(fileDistance);
  Serial.println(F(" squares)"));
  moveAlongTopEdge(fileDistance);
  
  // Step 5: Move down to center of destination square
  Serial.println(F("Step 5: Moving to center of destination square"));
  moveFromTopEdgeToCenter();
  
  // Step 6: Drop rook
  Serial.println(F("Step 6: Dropping rook"));
  Serial.println(F("MAGNET_OFF"));
  waitForESPResponse(F("MAGNET_OFF_READY"));
  delay(25);
  
  // Update position
  currentX = toX;
  currentY = toY;
  
  // Update board state
  updatePiecePosition(fromX, fromY, toX, toY);
  
  Serial.println(F("✅ Rook castling move completed via top edge"));
}

void moveToTopEdge() {
  Serial.println(F("   📍 Moving to top edge (0.5 squares up from center)"));
  
  // Move 0.5 squares up from square center
  long halfSquareSteps = steps_per_square / 2;
  
  // Y MOVEMENT: Move up (A motor HIGH, B motor LOW)
  digitalWrite(A_DIR_PIN, HIGH);  // A motor up
  digitalWrite(B_DIR_PIN, LOW);   // B motor up (opposite direction)
  
  for (long i = 0; i < halfSquareSteps; i++) {
    stepBoth();
  }
  
  Serial.println(F("   ✅ Reached top edge"));
}

void moveAlongTopEdge(int fileDistance) {
  if (fileDistance == 0) return;
  
  Serial.print(F("   📍 Moving along top edge: "));
  Serial.print(abs(fileDistance));
  Serial.print(F(" squares "));
  Serial.println(fileDistance > 0 ? F("right") : F("left"));
  
  long steps = abs(fileDistance) * steps_per_square;
  
  // X MOVEMENT: Both motors same direction
  digitalWrite(A_DIR_PIN, fileDistance > 0 ? HIGH : LOW);
  digitalWrite(B_DIR_PIN, fileDistance > 0 ? HIGH : LOW);
  
  for (long i = 0; i < steps; i++) {
    stepBoth();
  }
  
  Serial.println(F("   ✅ Reached destination file along top edge"));
}

void moveFromTopEdgeToCenter() {
  Serial.println(F("   📍 Moving from top edge to square center (0.5 squares down)"));
  
  // Move 0.5 squares down to square center
  long halfSquareSteps = steps_per_square / 2;
  
  // Y MOVEMENT: Move down (A motor LOW, B motor HIGH)
  digitalWrite(A_DIR_PIN, LOW);   // A motor down
  digitalWrite(B_DIR_PIN, HIGH);  // B motor down (opposite direction)
  
  for (long i = 0; i < halfSquareSteps; i++) {
    stepBoth();
  }
  
  Serial.println(F("   ✅ Reached square center"));
}