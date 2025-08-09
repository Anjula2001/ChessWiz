# Enhanced Arduino Serial Reset System

## 🔄 Reset Button Function (GPIO18)
When you press the reset button on GPIO18, the ESP32 will reset the Arduino using **default RX/TX pins (GPIO1/GPIO3)** with an enhanced serial communication protocol.

## 📡 Serial Reset Protocol
**ESP32 ↔ Arduino communication via default Serial (GPIO1/GPIO3 at 115200 baud)**

### Reset Sequence:
1. **Immediate Safety**: Turn off magnet and LED
2. **Multiple Reset Commands**: Send 3 different reset commands with retries
3. **Communication Test**: Verify Arduino responds after reset
4. **Hardware Backup**: Optional GPIO5 reset pulse
5. **ESP32 Restart**: Complete system reset

## 📤 Serial Commands Sent:
```
RESET_ARDUINO    // Primary reset command
SYSTEM_RESET     // Alternative reset command  
RESTART          // Third reset command
ESP32_TEST       // Communication test
```

## 🔧 Arduino Code Requirements
**Your Arduino code should handle these reset commands:**

```cpp
void setup() {
  Serial.begin(115200);
  // Arduino initialization
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "RESET_ARDUINO" || 
        command == "SYSTEM_RESET" || 
        command == "RESTART") {
      
      // Reset Arduino software state
      resetChessBoard();
      resetMotorPositions();
      
      // Respond to ESP32
      Serial.println("ARDUINO_READY");
      
      // Optional: Software reset
      // asm volatile ("  jmp 0");  // AVR software reset
    }
    else if (command == "ESP32_TEST") {
      Serial.println("ARDUINO_READY");
    }
  }
}

void resetChessBoard() {
  // Reset your chess board state
  // Reset motor positions
  // Reset any game variables
}
```

## 🎯 Advantages of Serial Reset:
- ✅ **No extra wiring needed** - Uses existing RX/TX connection
- ✅ **Software-controlled reset** - Can reset specific functions
- ✅ **Multiple retry attempts** - More reliable than single command
- ✅ **Communication verification** - Tests if Arduino responds
- ✅ **Fallback hardware reset** - GPIO5 backup if connected

## 🔌 Pin Usage:
- **GPIO1 (TX)**: ESP32 → Arduino communication
- **GPIO3 (RX)**: Arduino → ESP32 communication  
- **GPIO5**: Optional hardware reset (backup)
- **GPIO18**: Reset button input

## 📋 Reset Process Flow:
1. Press GPIO18 button
2. ESP32 sends serial reset commands via GPIO1/GPIO3
3. Arduino receives commands and resets software state
4. Arduino responds with "ARDUINO_READY"
5. ESP32 verifies communication
6. Optional GPIO5 hardware reset pulse
7. ESP32 restarts completely

**This provides a robust reset system using your default RX/TX pins!** 🚀
