// Test AI Move Emission - Verify frontend emits AI moves correctly
const http = require('http');

const SERVER_URL = 'http://localhost:3000';

function makeRequest(method, path, data = null) {
  return new Promise((resolve, reject) => {
    const options = {
      hostname: 'localhost',
      port: 3001,  // Server runs on 3001
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

async function testAIEmission() {
  console.log('🎯 Testing AI Move Emission from Frontend...\n');
  
  try {
    // Step 1: Check if server is running
    console.log('📡 Checking server status...');
    const serverTest = await makeRequest('GET', '/health').catch(() => null);
    if (!serverTest || serverTest.status !== 200) {
      console.log('❌ Server not running! Start with: cd backend && node server.js');
      return;
    }
    console.log('✅ Server is running\n');

    // Step 2: Clear any existing moves in singleplayer-default room
    console.log('🧹 Clearing existing moves...');
    const clearResponse = await makeRequest('GET', '/getAnyMove');
    console.log(`Cleared: ${clearResponse.data.move || 'no moves'}\n`);
    
    // Step 3: Simulate a physical player move that would trigger AI response
    console.log('👤 Simulating physical player move (e2-e4) to trigger AI...');
    const physicalMove = await makeRequest('POST', '/physicalMove', {
      move: 'e2-e4',  // Correct format with dash
      roomId: 'singleplayer-default'
    });
    console.log(`Physical move response: ${physicalMove.status === 200 ? '✅' : '❌'}\n`);
    
    // Step 4: Wait a moment for AI to potentially respond
    console.log('⏳ Waiting 3 seconds for AI to respond...');
    await new Promise(resolve => setTimeout(resolve, 3000));
    
    // Step 5: Check if AI move was stored
    console.log('🔍 Checking for AI move in server storage...');
    const aiMoveCheck = await makeRequest('GET', '/getAnyMove');
    
    if (aiMoveCheck.data.success && aiMoveCheck.data.move) {
      console.log('🎉 SUCCESS! AI move found:');
      console.log(`   Move: ${aiMoveCheck.data.move}`);
      console.log(`   Source: ${aiMoveCheck.data.source}`);
      console.log(`   This proves the frontend→server AI emission is working! ✅`);
    } else {
      console.log('⚠️  No AI move found. This could mean:');
      console.log('   1. AI is not enabled in single player mode');
      console.log('   2. Frontend is not connected to server'); 
      console.log('   3. AI response is not being emitted properly');
      console.log('   4. No active single player game session');
    }
    
    // Step 6: Monitor moves for next 10 seconds
    console.log('\n🔄 Monitoring for moves in next 10 seconds...');
    for (let i = 0; i < 10; i++) {
      await new Promise(resolve => setTimeout(resolve, 1000));
      const monitor = await makeRequest('GET', '/getAnyMove');
      if (monitor.data.success && monitor.data.move) {
        console.log(`⚡ Move detected at ${i+1}s: ${monitor.data.move} from ${monitor.data.source}`);
        break;
      } else {
        process.stdout.write('.');
      }
    }
    
    console.log('\n\n📋 Test Summary:');
    console.log('  - To properly test, open http://localhost:5174/');
    console.log('  - Start a Single Player game');
    console.log('  - Make a move as the human player');
    console.log('  - The AI should respond and emit a move to the server');
    console.log('  - ESP32 should then poll and find that AI move');
    
  } catch (error) {
    console.error('❌ Test failed:', error.message);
  }
}

testAIEmission();
