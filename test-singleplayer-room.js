#!/usr/bin/env node

// Test Socket.IO connection and create singleplayer room
const io = require('socket.io-client');

console.log('🎮 Testing Single Player Room Creation...');

const socket = io('http://localhost:3001');

socket.on('connect', () => {
  console.log('✅ Connected to Socket.IO server');
  console.log('🆔 Socket ID:', socket.id);
  
  // Join the singleplayer room to create it
  socket.emit('joinGame', 'singleplayer-default');
  console.log('🏠 Joined room: singleplayer-default');
  
  // Wait a bit for room creation
  setTimeout(() => {
    console.log('📤 Sending test AI move...');
    
    // Send an AI move to the singleplayer room
    socket.emit('move', {
      roomId: 'singleplayer-default',
      move: 'e7e5',
      fen: 'rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2',
      playerType: 'ai',
      playerColor: 'black'
    });
    
    console.log('✅ AI move sent to singleplayer room');
    
    // Test ESP32 polling after move
    setTimeout(() => {
      console.log('🔍 Testing ESP32 polling...');
      
      const http = require('http');
      
      // Test /getAnyMove
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
          console.log('📥 ESP32 polling result:', JSON.parse(data));
          
          // Also test specific singleplayer polling
          const spOptions = {
            hostname: 'localhost',
            port: 3001,
            path: '/getLastMove?roomId=singleplayer-default',
            method: 'GET'
          };
          
          const spReq = http.request(spOptions, (spRes) => {
            let spData = '';
            spRes.on('data', (chunk) => spData += chunk);
            spRes.on('end', () => {
              console.log('📥 Singleplayer polling result:', JSON.parse(spData));
              socket.disconnect();
              process.exit(0);
            });
          });
          
          spReq.end();
        });
      });
      
      req.end();
    }, 1000);
    
  }, 500);
});

socket.on('disconnect', () => {
  console.log('❌ Disconnected from Socket.IO server');
});

// Cleanup after 10 seconds
setTimeout(() => {
  console.log('⏰ Test timeout - disconnecting');
  socket.disconnect();
  process.exit(0);
}, 10000);
