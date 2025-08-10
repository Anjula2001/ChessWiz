// Test Frontend-Style AI Move
const io = require('socket.io-client');

async function testFrontendStyleAIMove() {
  console.log('🎯 Testing Frontend-Style AI Move Storage...\n');
  
  try {
    const socket = io('http://localhost:3001');
    
    await new Promise((resolve) => {
      socket.on('connect', () => {
        console.log('✅ Connected to Socket.IO server');
        resolve();
      });
    });
    
    // Join singleplayer room
    console.log('1️⃣ Joining singleplayer-default room...');
    socket.emit('joinGame', 'singleplayer-default');
    await new Promise(resolve => setTimeout(resolve, 500));
    
    // Emit AI move exactly like the frontend does
    console.log('2️⃣ Emitting AI move (frontend style)...');
    socket.emit('move', {
      roomId: 'singleplayer-default',
      move: 'g8-f6',  // Frontend sends with dash
      fen: 'rnbqkb1r/pppppppp/5n2/8/P7/8/1PPPPPPP/RNBQKBNR w KQkq - 1 2',
      playerType: 'ai',
      playerColor: 'ai'
    });
    
    console.log('✅ AI move emitted exactly like frontend');
    
    // Wait for processing
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    // Test ESP32 polling
    console.log('\n3️⃣ Testing ESP32 SinglePlayer room polling...');
    
    const http = require('http');
    function makeRequest(path) {
      return new Promise((resolve, reject) => {
        const req = http.request({
          hostname: 'localhost',
          port: 3001,
          path: path,
          method: 'GET'
        }, (res) => {
          let body = '';
          res.on('data', (chunk) => body += chunk);
          res.on('end', () => {
            try {
              resolve({ status: res.statusCode, data: JSON.parse(body) });
            } catch (e) {
              resolve({ status: res.statusCode, data: body });
            }
          });
        });
        req.on('error', reject);
        req.end();
      });
    }
    
    const result = await makeRequest('/getLastMove?roomId=singleplayer-default');
    
    console.log('\n📊 RESULT:');
    console.log(`Status: ${result.status}`);
    console.log(`Success: ${result.data.success}`);
    console.log(`Move: ${result.data.move}`);
    console.log(`Room: ${result.data.room}`);
    
    if (result.data.success && result.data.move) {
      console.log('\n🎉 SUCCESS! Backend is handling AI moves correctly!');
      console.log('✅ ESP32 will find this move after uploading the fixed code');
    } else {
      console.log('\n❌ ISSUE: AI move not stored correctly');
    }
    
    socket.disconnect();
    
  } catch (error) {
    console.error('❌ Test failed:', error.message);
  }
}

testFrontendStyleAIMove();
