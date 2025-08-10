// Socket.IO Client Test - Simulate Frontend AI Move Emission
const io = require('socket.io-client');

async function testSocketIOAIMoves() {
  console.log('🎯 Testing Socket.IO AI Move Flow (Simulating Frontend)...\n');
  
  try {
    // Connect to the Socket.IO server
    console.log('📡 Connecting to Socket.IO server...');
    const socket = io('http://localhost:3001');
    
    await new Promise((resolve) => {
      socket.on('connect', () => {
        console.log('✅ Connected to Socket.IO server');
        console.log(`🔗 Socket ID: ${socket.id}`);
        resolve();
      });
    });
    
    // Step 1: Join the singleplayer room (like frontend does)
    console.log('\n1️⃣ Joining singleplayer-default room...');
    socket.emit('joinGame', 'singleplayer-default');
    
    await new Promise(resolve => setTimeout(resolve, 500)); // Wait for room join
    
    // Step 2: Simulate physical move from ESP32 (already happened in your test)
    console.log('\n2️⃣ Simulating physical move reception...');
    socket.emit('move', {
      roomId: 'singleplayer-default',
      move: 'a2-a4',
      playerType: 'physical',
      playerColor: 'white',
      source: 'physical'
    });
    
    await new Promise(resolve => setTimeout(resolve, 1000)); // Wait for processing
    
    // Step 3: Simulate AI response (what frontend should do)
    console.log('\n3️⃣ Simulating AI move emission (frontend AI response)...');
    socket.emit('move', {
      roomId: 'singleplayer-default',
      move: 'g8f6',  // AI response move (no dash format)
      playerType: 'ai',
      playerColor: 'ai',
      fen: 'rnbqkb1r/pppppppp/5n2/8/P7/8/1PPPPPPP/RNBQKBNR w KQkq - 1 2'
    });
    
    console.log('✅ AI move emitted to server');
    
    // Step 4: Wait and test ESP32 polling
    console.log('\n4️⃣ Waiting 2 seconds for server to process...');
    await new Promise(resolve => setTimeout(resolve, 2000));
    
    // Step 5: Test ESP32 polling endpoints
    console.log('\n5️⃣ Testing ESP32 polling (what ESP32 should find)...');
    
    const http = require('http');
    
    function makeRequest(method, path) {
      return new Promise((resolve, reject) => {
        const options = {
          hostname: 'localhost',
          port: 3001,
          path: path,
          method: method
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
        req.end();
      });
    }
    
    // Test SinglePlayer room endpoint (what ESP32 should use after fix)
    const singlePlayerResult = await makeRequest('GET', '/getLastMove?roomId=singleplayer-default');
    console.log('\n📍 SinglePlayer room polling result:');
    console.log(`  Status: ${singlePlayerResult.status}`);
    console.log(`  Success: ${singlePlayerResult.data.success || false}`);
    console.log(`  Move: ${singlePlayerResult.data.move || 'none'}`);
    console.log(`  Source: ${singlePlayerResult.data.source || 'none'}`);
    
    // Test Unified endpoint
    const unifiedResult = await makeRequest('GET', '/getAnyMove');
    console.log('\n📍 Unified endpoint polling result:');
    console.log(`  Status: ${unifiedResult.status}`);
    console.log(`  Success: ${unifiedResult.data.success || false}`);
    console.log(`  Move: ${unifiedResult.data.move || 'none'}`);
    console.log(`  Source: ${unifiedResult.data.source || 'none'}`);
    
    // Results analysis
    console.log('\n📊 RESULTS ANALYSIS:');
    if (singlePlayerResult.data.success && singlePlayerResult.data.move) {
      console.log('🎉 SUCCESS! AI move found in SinglePlayer room');
      console.log('✅ ESP32 should be able to find this move after uploading the fix');
      console.log(`🎯 Move to execute: ${singlePlayerResult.data.move}`);
    } else if (unifiedResult.data.success && unifiedResult.data.move) {
      console.log('⚠️  AI move found in Unified endpoint only');
      console.log('🔧 ESP32 needs the room polling fix to find it');
    } else {
      console.log('❌ No AI move found in either endpoint');
      console.log('🔍 Check server Socket.IO move handling logic');
    }
    
    console.log('\n🔧 NEXT STEPS:');
    console.log('1. Upload the fixed ESP32 code (with room memory logic)');
    console.log('2. Open http://localhost:5174/ and start a real Single Player game');
    console.log('3. Make a physical move on the board');
    console.log('4. Watch ESP32 serial monitor for "🔍 Polling: SinglePlayer room"');
    console.log('5. ESP32 should find and execute the AI response move');
    
    socket.disconnect();
    
  } catch (error) {
    console.error('❌ Test failed:', error.message);
  }
}

// Check if socket.io-client is available
try {
  require.resolve('socket.io-client');
  testSocketIOAIMoves();
} catch (e) {
  console.log('❌ socket.io-client not found');
  console.log('ℹ️  This test simulates what the frontend does via Socket.IO');
  console.log('🔧 The real solution is to:');
  console.log('   1. Upload the ESP32 fix');
  console.log('   2. Use the actual frontend at http://localhost:5174/');
  console.log('   3. Start a Single Player game');
  console.log('   4. Make physical moves to trigger AI responses');
}
