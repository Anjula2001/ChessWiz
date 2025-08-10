// Test ESP32 Room Polling Fix
const http = require('http');

function makeRequest(method, path, data = null) {
  return new Promise((resolve, reject) => {
    const options = {
      hostname: 'localhost',
      port: 3001,
      path: path,
      method: method,
      headers: {
        'Content-Type': 'application/json'
      }
    };

    const req = http.request(options, (res) => {
      let body = '';
      res.on('data', (chunk) => body += chunk);
      res.on('end', () => {
        try {
          const parsed = JSON.parse(body);
          resolve({ status: res.statusCode, data: parsed });
        } catch (e) {
          resolve({ status: res.statusCode, data: body });
        }
      });
    });

    req.on('error', (err) => reject(err));
    
    if (data) {
      req.write(JSON.stringify(data));
    }
    req.end();
  });
}

async function testESP32RoomPolling() {
  console.log('🎯 Testing ESP32 Room Polling Fix...\n');
  
  try {
    // Step 1: Clear any existing moves
    console.log('🧹 Clearing existing moves...');
    await makeRequest('GET', '/getAnyMove');
    
    // Step 2: Simulate AI storing a move in singleplayer-default room
    // (This simulates what the frontend would do when AI responds)
    console.log('🤖 Simulating AI storing move in singleplayer-default room...');
    
    // We'll use the server's internal move storage by sending a move with playerType='ai'
    // This should be stored for ESP32 consumption
    const aiMove = await makeRequest('POST', '/physicalMove', {
      move: 'g8-f6',
      roomId: 'singleplayer-default',
      playerType: 'ai',  // Mark as AI move
      source: 'ai'
    });
    
    console.log(`AI move simulation: ${aiMove.status === 200 ? '✅' : '❌'}`);
    
    // Step 3: Test ESP32 polling endpoints
    console.log('\n🔍 Testing ESP32 polling endpoints...');
    
    // Test unified endpoint (what ESP32 was using before)
    const unifiedResult = await makeRequest('GET', '/getAnyMove');
    console.log('Unified endpoint (/getAnyMove):');
    console.log(`  Success: ${unifiedResult.data.success}`);
    console.log(`  Move: ${unifiedResult.data.move || 'none'}`);
    console.log(`  Source: ${unifiedResult.data.source || 'none'}`);
    
    // Test singleplayer room endpoint (what ESP32 should use after fix)
    const singlePlayerResult = await makeRequest('GET', '/getLastMove?roomId=singleplayer-default');
    console.log('\nSinglePlayer room (/getLastMove?roomId=singleplayer-default):');
    console.log(`  Success: ${singlePlayerResult.data.success}`);
    console.log(`  Move: ${singlePlayerResult.data.move || 'none'}`);
    console.log(`  Source: ${singlePlayerResult.data.source || 'none'}`);
    
    // Step 4: Test the fix
    console.log('\n📋 ESP32 Fix Analysis:');
    
    if (unifiedResult.data.success && unifiedResult.data.move) {
      console.log('✅ Unified endpoint has the AI move');
      console.log('✅ ESP32 fix: ESP32 will now poll SinglePlayer room instead');
    }
    
    if (singlePlayerResult.data.success && singlePlayerResult.data.move) {
      console.log('✅ SinglePlayer room has the AI move');
      console.log('✅ ESP32 should find this move after the fix is uploaded');
    } else {
      console.log('⚠️  SinglePlayer room empty - may need active frontend session');
    }
    
    console.log('\n🔧 Next Steps:');
    console.log('  1. Upload the fixed ESP32 code');
    console.log('  2. Open http://localhost:5174/ and start single player game');
    console.log('  3. Make a physical move (a2-a4)');
    console.log('  4. Watch ESP32 serial: should show "🔍 Polling: SinglePlayer room"');
    console.log('  5. AI should respond and ESP32 should catch it');
    
  } catch (error) {
    console.error('❌ Test failed:', error.message);
  }
}

testESP32RoomPolling();
