#!/usr/bin/env node

// Simulate the exact ESP32 polling behavior to debug the issue
const io = require('socket.io-client');
const http = require('http');

console.log('🔍 ESP32 Polling Simulation Test');
console.log('================================');

const socket = io('http://localhost:3001');

let pollCount = 0;
let moveFound = false;

// Function to poll like ESP32
function pollForMoves() {
  pollCount++;
  
  const options = {
    hostname: 'localhost',
    port: 3001,
    path: '/getAnyMove',
    method: 'GET'
  };
  
  const req = http.request(options, (res) => {
    let data = '';
    res.on('data', (chunk) => data += chunk);
    res.on('end', () => {
      const result = JSON.parse(data);
      console.log(`📡 Poll #${pollCount}:`, result.success ? `FOUND: ${result.move}` : 'No moves');
      
      if (result.success && result.move) {
        moveFound = true;
        console.log('✅ ESP32 would receive move:', result.move);
      }
    });
  });
  
  req.on('error', (err) => {
    console.error('❌ Polling error:', err.message);
  });
  
  req.end();
}

socket.on('connect', () => {
  console.log('✅ Connected to Socket.IO server');
  
  // Join the singleplayer room
  socket.emit('joinGame', 'singleplayer-default');
  console.log('🏠 Joined room: singleplayer-default');
  
  // Start polling like ESP32 (every 2 seconds)
  console.log('🔄 Starting ESP32-style polling (every 2 seconds)...');
  const pollInterval = setInterval(pollForMoves, 2000);
  
  // After 3 seconds, send an AI move
  setTimeout(() => {
    console.log('\n📤 Sending AI move (simulating AI response)...');
    
    socket.emit('move', {
      roomId: 'singleplayer-default',
      move: 'e7e5',
      fen: 'rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2',
      playerType: 'ai',
      playerColor: 'black'
    });
    
    console.log('✅ AI move sent! ESP32 should detect it in next poll...');
    
    // Stop test after 10 more seconds
    setTimeout(() => {
      clearInterval(pollInterval);
      console.log('\n🏁 Test completed');
      console.log(`📊 Total polls: ${pollCount}`);
      console.log(`📋 Move found: ${moveFound ? 'YES ✅' : 'NO ❌'}`);
      
      if (!moveFound) {
        console.log('💡 The ESP32 polling timing might be the issue!');
      }
      
      socket.disconnect();
      process.exit(0);
    }, 10000);
    
  }, 3000);
});

socket.on('disconnect', () => {
  console.log('❌ Disconnected from Socket.IO server');
});
