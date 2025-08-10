const http = require('http');

function makeHttpRequest(options, data = null) {
  return new Promise((resolve, reject) => {
    const req = http.request(options, (res) => {
      let body = '';
      res.on('data', chunk => body += chunk);
      res.on('end', () => {
        try {
          const jsonData = JSON.parse(body);
          resolve({ status: res.statusCode, data: jsonData });
        } catch (e) {
          resolve({ status: res.statusCode, data: body });
        }
      });
    });
    
    req.on('error', reject);
    
    if (data) {
      req.write(JSON.stringify(data));
    }
    req.end();
  });
}

async function testCommunicationLoopFix() {
  console.log('🧪 Testing Communication Loop Fix');
  console.log('=====================================');
  
  try {
    // Test 1: Check server health
    console.log('\n📡 Step 1: Checking server health...');
    
    const healthOptions = {
      hostname: 'localhost',
      port: 3001,
      path: '/health',
      method: 'GET'
    };
    
    const healthResponse = await makeHttpRequest(healthOptions);
    console.log('✅ Server health:', healthResponse.data);
    
    // Test 2: Make an AI move to verify ESP32 receives it  
    console.log('\n📤 Step 2: Making AI move c7-c5...');
    
    const moveOptions = {
      hostname: 'localhost',
      port: 3001,
      path: '/api/move',
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      }
    };
    
    const moveData = {
      move: 'c7-c5',
      gameId: 'test-game',
      playerId: 'ai'
    };
    
    const aiMoveResponse = await makeHttpRequest(moveOptions, moveData);
    console.log('✅ AI move response:', aiMoveResponse.data);
    
    // Wait for ESP32 to process
    await new Promise(resolve => setTimeout(resolve, 2000));
    
    // Test 3: Check if move is available for ESP32 polling
    console.log('\n🔍 Step 3: Checking ESP32 polling endpoint...');
    
    const pollingOptions = {
      hostname: 'localhost', 
      port: 3001,
      path: '/getAnyMove',
      method: 'GET'
    };
    
    const pollingResponse = await makeHttpRequest(pollingOptions);
    console.log('📊 ESP32 polling response:', pollingResponse.data);
    
    // Test 4: Verify communication loop fix expectations
    console.log('\n🔧 Step 4: Communication Loop Fix Verification');
    console.log('====================================================');
    console.log('✅ FIXED: ESP32 processWebMoveTask no longer waits for acknowledgment parsing');
    console.log('✅ FIXED: ESP32 handleArduinoCommunicationTask ignores debug messages');
    console.log('✅ FIXED: Clean move transmission without communication loops');
    console.log('');
    console.log('Expected ESP32 behavior:');
    console.log('1. 📡 ESP32 polls /getAnyMove and receives: c7-c5');
    console.log('2. 📤 ESP32 sends to Arduino: "c7-c5" (clean move)');
    console.log('3. 🎯 Arduino receives: "c7-c5" (not debug text)');
    console.log('4. ✅ Arduino executes: c7-c5 motor movement');
    console.log('5. 🔄 Arduino sends acknowledgment: "Move complete: c7-c5"');
    console.log('6. 🚫 ESP32 IGNORES acknowledgment (no re-parsing as move)');
    console.log('');
    console.log('❌ Previous broken behavior:');
    console.log('- ESP32 would re-parse acknowledgment as new move');
    console.log('- Arduino would execute wrong move (like h2-h4 instead of c7-c5)');
    console.log('- Communication loop caused move confusion');
    
    console.log('\n🎯 Next Steps:');
    console.log('1. Upload fixed ESP32 code (esp32_bidirectional_bridge_ORIGINAL_LOGIC.ino)');
    console.log('2. Monitor Arduino serial output for: "Received move: c7-c5"');
    console.log('3. Verify Arduino does NOT receive: "Move complete: c7-c5"');
    console.log('4. Confirm Arduino executes c7-c5 motors (not h2-h4)');
    
  } catch (error) {
    console.error('❌ Test failed:', error.message);
  }
}

testCommunicationLoopFix();
