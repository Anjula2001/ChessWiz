#!/usr/bin/env node

console.log('🔍 Frontend Socket.IO Connection and Physical Move Test');
console.log('====================================================');

const io = require('socket.io-client');

const socket = io('http://localhost:3001');

socket.on('connect', () => {
  console.log('✅ Connected to Socket.IO server');
  console.log('🆔 Socket ID:', socket.id);
  
  // Join the singleplayer room
  socket.emit('joinGame', 'singleplayer-default');
  console.log('🏠 Joined room: singleplayer-default');
  
  // Listen for physical moves
  socket.on('physicalMove', (data) => {
    console.log('📥 RECEIVED PHYSICAL MOVE:', data);
    console.log('  Move:', data.move);
    console.log('  Source:', data.source);
    console.log('  Player Side:', data.playerSide);
    console.log('  Timestamp:', data.timestamp);
  });
  
  // Listen for other move events
  socket.on('moveMade', (data) => {
    console.log('📥 RECEIVED MOVE MADE:', data);
  });
  
  console.log('👂 Listening for physicalMove and moveMade events...');
  console.log('📡 Send a physical move using:');
  console.log('   curl -X POST http://localhost:3001/physicalMove \\');
  console.log('     -H "Content-Type: application/json" \\');
  console.log('     -d \'{"move": "e2-e4", "roomId": "singleplayer-default", "playerSide": "white"}\'');
  
  // Keep the connection alive for testing
  setTimeout(() => {
    console.log('🏁 Test completed after 30 seconds. Disconnecting...');
    socket.disconnect();
  }, 30000);
});

socket.on('disconnect', () => {
  console.log('❌ Disconnected from Socket.IO server');
  process.exit(0);
});

socket.on('error', (error) => {
  console.error('❌ Socket.IO error:', error);
});
