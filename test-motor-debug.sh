#!/bin/bash

echo "🎯 MOTOR DEBUGGING SEQUENCE"
echo "=========================="

echo ""
echo "📊 Step 1: Generate and store AI move for ESP32..."

# Generate AI move
AI_MOVE=$(curl -s -X POST http://localhost:3001/getBestMove \
  -H "Content-Type: application/json" \
  -d '{"fen": "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"}' \
  | jq -r '.bestMove')

echo "🤖 AI generated move: $AI_MOVE"

# Convert to our format (e7e6 -> e7-e6)
FORMATTED_MOVE="${AI_MOVE:0:2}-${AI_MOVE:2:2}"
echo "📤 Formatted move: $FORMATTED_MOVE"

# Store AI move
curl -s -X POST http://localhost:3001/physicalMove \
  -H "Content-Type: application/json" \
  -d "{\"move\": \"$FORMATTED_MOVE\", \"roomId\": \"singleplayer-default\", \"source\": \"ai\"}" > /dev/null

echo "💾 AI move stored in system"

echo ""
echo "📡 Step 2: Simulating ESP32 polling..."

# Poll for move like ESP32 would
POLL_RESULT=$(curl -s http://localhost:3001/getAnyMove)
echo "🔍 ESP32 polling result: $POLL_RESULT"

# Extract move from JSON
ESP32_MOVE=$(echo "$POLL_RESULT" | jq -r '.move // "null"')

if [ "$ESP32_MOVE" != "null" ]; then
    echo "✅ SUCCESS: ESP32 would receive move: $ESP32_MOVE"
    echo ""
    echo "🎯 Expected Hardware Flow:"
    echo "1. ESP32 receives: '$ESP32_MOVE'"
    echo "2. ESP32 sends to Arduino: '$ESP32_MOVE'"
    echo "3. Arduino parses: '$ESP32_MOVE'"
    echo "4. Arduino executes motor movement"
    echo "5. Arduino sends: 'Move complete: $ESP32_MOVE'"
    echo "6. ESP32 ignores acknowledgment (fixed communication loop)"
    echo ""
    echo "🔧 NEXT STEPS TO GET MOTORS WORKING:"
    echo "1. 📤 Upload ESP32 code: esp32_bidirectional_bridge_ORIGINAL_LOGIC.ino"
    echo "2. 📤 Upload Arduino code: arduino_chess_motor_controller_OPTIMIZED.ino"
    echo "3. 🔌 Connect ESP32-Arduino serial (GPIO1→Arduino RX, GPIO3→Arduino TX)"
    echo "4. 🔋 Ensure motor power supply is connected"
    echo "5. 📊 Monitor both serial outputs"
    echo "6. 🧪 Test with this exact move: $ESP32_MOVE"
else
    echo "❌ PROBLEM: ESP32 polling returned no move"
    echo "🔍 Server response: $POLL_RESULT"
fi

echo ""
echo "🛠️ MOTOR TROUBLESHOOTING CHECKLIST:"
echo "- [ ] ESP32 code uploaded and running"
echo "- [ ] Arduino code uploaded and running"
echo "- [ ] Serial connection ESP32↔Arduino (115200 baud)"
echo "- [ ] Motor drivers powered and enabled"
echo "- [ ] ENABLE_PIN set to LOW (motors enabled)"
echo "- [ ] Step delay appropriate (550 microseconds)"
echo "- [ ] No loose connections on stepper motors"
echo "- [ ] Both devices have stable power supply"
