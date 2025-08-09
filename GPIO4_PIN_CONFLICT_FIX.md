# GPIO4 Pin Conflict Fix - G and H Column Detection Issue

## 🚨 Critical Issue Identified
**GPIO4 pin was being used by both:**
- `ARDUINO_RESET_PIN 4` (Arduino hardware reset)
- `MUX1_SIG 4` (Sensor multiplexer 1 signal pin)

## 🎯 Impact on Chess Board
- **MUX1 controls columns g(6) and h(7)**
- **Pin conflict caused g and h columns to not show magnet status**
- **All other columns (a,b,c,d,e,f) worked correctly via MUX2, MUX3, MUX4**

## 🔧 Solution Applied
**Changed Arduino reset pin from GPIO4 to GPIO5:**

```cpp
// BEFORE (CONFLICTED):
#define ARDUINO_RESET_PIN 4  // CONFLICT with MUX1_SIG 4
#define MUX1_SIG 4

// AFTER (FIXED):
#define ARDUINO_RESET_PIN 5  // No conflict - GPIO5 available
#define MUX1_SIG 4           // Now exclusively for MUX1 sensors
```

## 📊 MUX Channel Mapping (Now Fixed)
- **MUX1 (GPIO4)**: Columns h(7), g(6) ✅ **FIXED**
- **MUX2 (GPIO16)**: Columns f(5), e(4) ✅ Working
- **MUX3 (GPIO32)**: Columns d(3), c(2) ✅ Working  
- **MUX4 (GPIO33)**: Columns b(1), a(0) ✅ Working

## 🔌 Hardware Update Required
**If you have Arduino reset wire connected to GPIO4:**
- **Disconnect reset wire from GPIO4**
- **Connect Arduino reset wire to GPIO5**
- **Or leave disconnected (software reset still works via Serial)**

## ✅ Expected Results
- **All 64 squares (8x8) should now detect magnets properly**
- **g and h columns will show "X" when magnet present, "." when absent**
- **Chess moves involving g/h columns (like g1-f3, h7-h5) will be detected**

## 🧪 Testing Verification
1. **Upload fixed ESP32 code**
2. **Check serial monitor board status**
3. **Test magnet detection on g1, g8, h1, h8 squares**
4. **Verify all columns show proper "X" and "." patterns**

## 📋 GPIO Usage Summary (Post-Fix)
- GPIO4: MUX1_SIG (g,h columns) - **EXCLUSIVE**
- GPIO5: ARDUINO_RESET_PIN - **NEW**
- GPIO16: MUX2_SIG (e,f columns)
- GPIO32: MUX3_SIG (c,d columns)  
- GPIO33: MUX4_SIG (a,b columns)
- GPIO14,27,26,25: MUX control pins S0,S1,S2,S3

**The g and h column detection should now work perfectly!** 🎯
