const io = require('socket.io-client');
const http = require('http');

console.log('🎮 Complete Single Player Chess Flow Test');
console.log('=========================================');

// Connect to server like the frontend does
const socket = io('http://localhost:3001');

socket.on('connect', () => {
    console.log('✅ Connected to server as:', socket.id);
    
    // Step 1: Join single player room (like frontend does)
    console.log('\n🎮 Step 1: Joining single player room...');
    socket.emit('joinRoom', {
        roomId: 'singleplayer-default',
        playerSide: 'white',
        gameMode: 'singleplayer'
    });
});

socket.on('roomJoined', (data) => {
    console.log('✅ Room joined successfully:', data);
    
    // Step 2: Simulate physical move (like ESP32 would do)
    console.log('\n📤 Step 2: Simulating physical move from ESP32...');
    
    // Send physical move via HTTP (like ESP32)
    const physicalMoveData = JSON.stringify({
        move: 'e2-e4',
        source: 'physical',
        roomId: 'singleplayer-default',
        playerSide: 'white'
    });
    
    const physicalMoveOptions = {
        hostname: 'localhost',
        port: 3001,
        path: '/physicalMove',
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': Buffer.byteLength(physicalMoveData)
        }
    };
    
    const physicalReq = http.request(physicalMoveOptions, (res) => {
        console.log(`📡 Physical move sent - Status: ${res.statusCode}`);
        
        let responseData = '';
        res.on('data', (chunk) => {
            responseData += chunk;
        });
        
        res.on('end', () => {
            console.log('📥 Physical move response:', responseData);
        });
    });
    
    physicalReq.on('error', (e) => {
        console.error('❌ Physical move error:', e.message);
    });
    
    physicalReq.write(physicalMoveData);
    physicalReq.end();
});

// Listen for moves (including AI responses)
socket.on('move', (moveData) => {
    console.log('\n🤖 MOVE RECEIVED VIA SOCKET.IO:');
    console.log('Move data:', JSON.stringify(moveData, null, 2));
    
    if (moveData.playerType === 'ai') {
        console.log('🎯 ✅ AI MOVE DETECTED:', moveData.move);
        
        // Step 3: Check if this AI move is available to ESP32
        console.log('\n🔍 Step 3: Checking if ESP32 can access this AI move...');
        
        setTimeout(() => {
            // Test ESP32 SinglePlayer endpoint
            const espOptions = {
                hostname: 'localhost',
                port: 3001,
                path: '/getLastMove?roomId=singleplayer-default',
                method: 'GET'
            };
            
            const espReq = http.request(espOptions, (res) => {
                console.log(`📡 ESP32 endpoint status: ${res.statusCode}`);
                
                let responseData = '';
                res.on('data', (chunk) => {
                    responseData += chunk;
                });
                
                res.on('end', () => {
                    try {
                        const data = JSON.parse(responseData);
                        console.log('📥 ESP32 endpoint response:', JSON.stringify(data, null, 2));
                        
                        if (data.move && data.move !== 'null') {
                            console.log('\n🎯 ✅ SUCCESS! ESP32 can access AI move:', data.move);
                            console.log('🔧 COMMUNICATION CHAIN VERIFIED:');
                            console.log('   Frontend → Socket.IO → AI → ESP32 ✅');
                        } else {
                            console.log('\n❌ ISSUE: AI move not available to ESP32');
                            console.log('🔧 POSSIBLE CAUSES:');
                            console.log('   - Move expired too quickly');
                            console.log('   - AI move not stored properly');
                            console.log('   - Wrong room or player type filtering');
                        }
                        
                    } catch (e) {
                        console.log('❌ Parse error:', e.message);
                    }
                    
                    console.log('\n=========================================');
                    console.log('🎮 SINGLE PLAYER FLOW TEST COMPLETE');
                    console.log('=========================================');
                    
                    // Close connection and exit
                    socket.disconnect();
                    process.exit(0);
                });
            });
            
            espReq.on('error', (e) => {
                console.error('❌ ESP32 endpoint error:', e.message);
                socket.disconnect();
                process.exit(1);
            });
            
            espReq.end();
            
        }, 1000); // Give AI time to process and store move
    }
});

socket.on('gameState', (gameData) => {
    console.log('🎲 Game state updated:', gameData.fen);
});

socket.on('disconnect', () => {
    console.log('🔌 Disconnected from server');
});

socket.on('error', (error) => {
    console.error('❌ Socket error:', error);
});

// Safety timeout
setTimeout(() => {
    console.log('\n⏰ Test timeout - no AI response received');
    console.log('This suggests AI is not being triggered properly');
    socket.disconnect();
    process.exit(1);
}, 10000);
