#!/usr/bin/env node

// Test frontend Socket.IO connection and physical move reception
const io = require('socket.io-client');
const http = require('http');

console.log('🔍 Frontend Socket.IO Connection Test');
console.log('====================================\n');

// Connect to the Socket.IO server like the frontend does
const socket = io('http://localhost:3001');

console.log('📡 Connecting to Socket.IO server...');

socket.on('connect', () => {
    console.log('✅ Connected to Socket.IO server');
    console.log('🆔 Socket ID:', socket.id);
    
    // Listen for physical moves like the frontend does
    socket.on('physicalMove', (data) => {
        console.log('\n🎯 RECEIVED PHYSICAL MOVE EVENT:');
        console.log('📦 Data:', JSON.stringify(data, null, 2));
        console.log('🎮 Move:', data.move);
        console.log('🏠 Room:', data.roomId || 'not specified');
        console.log('👤 Player Side:', data.playerSide);
        console.log('📍 Source:', data.source);
        console.log('⏰ Timestamp:', data.timestamp);
    });
    
    socket.on('moveMade', (data) => {
        console.log('\n♟️ RECEIVED MOVE MADE EVENT:');
        console.log('📦 Data:', JSON.stringify(data, null, 2));
    });
    
    // Join the singleplayer room like the frontend does
    socket.emit('joinRoom', 'singleplayer-default');
    console.log('🏠 Joined room: singleplayer-default');
    
    // Wait a moment, then send a test physical move
    setTimeout(() => {
        console.log('\n🧪 Sending test physical move...');
        sendTestPhysicalMove();
    }, 1000);
});

socket.on('disconnect', () => {
    console.log('❌ Disconnected from Socket.IO server');
});

socket.on('error', (error) => {
    console.error('❌ Socket.IO error:', error);
});

function sendTestPhysicalMove() {
    const payload = JSON.stringify({
        move: "c2-c4",
        source: "physical",
        roomId: "singleplayer-default", 
        playerSide: "white"
    });

    const options = {
        hostname: 'localhost',
        port: 3001,
        path: '/physicalMove',
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': Buffer.byteLength(payload)
        }
    };

    console.log('📤 Sending POST to /physicalMove...');
    
    const req = http.request(options, (res) => {
        let data = '';
        res.on('data', (chunk) => data += chunk);
        res.on('end', () => {
            console.log('📥 Server response:', res.statusCode);
            try {
                const response = JSON.parse(data);
                console.log('📦 Response data:', response);
            } catch (err) {
                console.log('📄 Raw response:', data);
            }
        });
    });

    req.on('error', (err) => {
        console.error('❌ Request error:', err.message);
    });

    req.write(payload);
    req.end();
}

// Auto-disconnect after 5 seconds
setTimeout(() => {
    console.log('\n🏁 Test completed. Disconnecting...');
    socket.disconnect();
    process.exit(0);
}, 5000);
