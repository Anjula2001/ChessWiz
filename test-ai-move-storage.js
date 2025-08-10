// Test AI Move Storage and ESP32 Polling - CORRECT METHOD
const http = require('http');
const io = require('socket.io-client');

console.log('🔍 Testing AI Move Storage and ESP32 Polling...');
console.log('');

async function testAIMoveFlow() {
  try {
    // Step 1: Connect to Socket.IO and send AI move (the correct way)
    console.log('📤 Step 1: Connecting to Socket.IO and sending AI move...');
    
    const socket = io('http://localhost:3001');
    
    socket.on('connect', () => {
      console.log('✅ Connected to Socket.IO server');
      
      // Send AI move via Socket.IO (this is how the frontend does it)
      const moveData = {
        roomId: 'singleplayer-default',
        move: 'c7-c5',
        fen: 'rnbqkbnr/pp1ppppp/8/2p5/8/8/PPPPPPPP/RNBQKBNR w KQkq c6 0 2',
        playerType: 'ai',
        playerColor: 'black'
      };
      
      console.log('📡 Sending AI move via Socket.IO:', moveData);
      socket.emit('move', moveData);
      
      // Wait a bit for the move to be processed
      setTimeout(() => {
        socket.disconnect();
        checkESP32Polling();
      }, 1000);
    });
    
    socket.on('connect_error', (error) => {
      console.error('❌ Socket.IO connection error:', error);
    });
    
  } catch (error) {
    console.error('❌ Test failed:', error);
  }
}

function checkESP32Polling() {
  console.log('');
  console.log('📡 Step 2: Checking ESP32 polling endpoint...');
  
  http.get('http://localhost:3001/getAnyMove', (res) => {
    let data = '';
    res.on('data', (chunk) => {
      data += chunk;
    });
    res.on('end', () => {
      console.log(`✅ ESP32 polling response: ${res.statusCode}`);
      console.log(`📥 Move available for ESP32: ${data}`);
      
      // Parse and check the move
      try {
        const moveResponse = JSON.parse(data);
        if (moveResponse.move && moveResponse.move !== 'null') {
          console.log(`🎯 Move found: ${moveResponse.move}`);
          console.log(`📍 Source: ${moveResponse.source}`);
          console.log(`📍 Timestamp: ${moveResponse.timestamp}`);
          console.log('');
          console.log('🔍 ANALYSIS:');
          console.log('✅ AI move is correctly stored via Socket.IO');
          console.log('✅ ESP32 polling endpoint returns the AI move');
          console.log('🎯 Now ESP32 should receive this move and send to Arduino');
          console.log('');
          console.log('🔧 If Arduino motors still don\'t execute:');
          console.log('   1. Check ESP32 Serial output for Arduino communication');
          console.log('   2. Verify Arduino Serial.available() is working');
          console.log('   3. Look for Arduino waiting for ESP32 responses');
          console.log('   4. Check for Serial timing conflicts');
        } else {
          console.log('⚠️ No move found in response');
          console.log('🔧 This means AI move was not stored correctly');
        }
      } catch (e) {
        console.log('⚠️ Could not parse response as JSON');
      }
    });
  }).on('error', (e) => {
    console.error(`❌ Error checking ESP32 polling: ${e.message}`);
  });
}

console.log('🚀 Starting AI move flow test...');
testAIMoveFlow();
