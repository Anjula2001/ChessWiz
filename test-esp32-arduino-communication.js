// Test ESP32-Arduino Communication Flow
const http = require('http');

console.log('🔍 Testing ESP32-Arduino Communication Flow...');
console.log('');

async function testCommunicationFlow() {
  try {
    // Step 1: Send an AI move to trigger ESP32 → Arduino communication
    console.log('📤 Step 1: Sending AI move to backend...');
    
    const moveData = {
      move: 'e7-e5',
      source: 'ai',
      roomId: 'singleplayer-default'
    };
    
    const postData = JSON.stringify(moveData);
    
    const options = {
      hostname: 'localhost',
      port: 3001,
      path: '/physicalMove',
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(postData)
      }
    };
    
    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', (chunk) => {
        data += chunk;
      });
      res.on('end', () => {
        console.log(`✅ Backend response: ${res.statusCode}`);
        console.log(`📥 Response data: ${data}`);
        
        // Step 2: Check if ESP32 can poll the move
        setTimeout(checkESP32Polling, 1000);
      });
    });
    
    req.on('error', (e) => {
      console.error(`❌ Error sending move: ${e.message}`);
    });
    
    req.write(postData);
    req.end();
    
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
          console.log('');
          console.log('🔍 ANALYSIS:');
          console.log('✅ Backend is storing moves correctly');
          console.log('✅ ESP32 polling endpoint is working');
          console.log('🎯 If ESP32 receives this move but Arduino motors don\'t execute:');
          console.log('   - Check Serial communication timing on ESP32');
          console.log('   - Verify Arduino is receiving Serial data');
          console.log('   - Look for Serial conflicts or timing issues');
          console.log('   - Check if Arduino is stuck waiting for responses');
        } else {
          console.log('⚠️ No move found in response');
        }
      } catch (e) {
        console.log('⚠️ Could not parse response as JSON');
      }
    });
  }).on('error', (e) => {
    console.error(`❌ Error checking ESP32 polling: ${e.message}`);
  });
}

console.log('🚀 Starting communication flow test...');
testCommunicationFlow();
