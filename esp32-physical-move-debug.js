#!/usr/bin/env node

// ESP32 Physical Move Diagnostic Tool
// This helps debug why physical moves aren't appearing on web interface

const http = require('http');

console.log('🔍 ESP32 Physical Move Diagnostic Tool');
console.log('=====================================\n');

// Test 1: Check current game state
function checkGameState() {
    return new Promise((resolve, reject) => {
        const options = {
            hostname: 'localhost',
            port: 3001,
            path: '/getAnyMove',
            method: 'GET',
        };

        console.log('📊 Test 1: Checking current game state...');
        
        const req = http.request(options, (res) => {
            let data = '';
            res.on('data', (chunk) => data += chunk);
            res.on('end', () => {
                try {
                    const response = JSON.parse(data);
                    console.log('📈 Current game state:', response);
                    
                    if (response.move && response.move !== "null") {
                        console.log('⚠️  There is a pending move for ESP32:', response.move);
                        console.log('📍 This means ESP32 should be receiving web moves, not sending them');
                    } else {
                        console.log('✅ No pending moves - ESP32 should be ready to send physical moves');
                    }
                    resolve(response);
                } catch (err) {
                    reject(err);
                }
            });
        });

        req.on('error', reject);
        req.end();
    });
}

// Test 2: Simulate physical move from ESP32
function testPhysicalMove(roomId, playerSide) {
    return new Promise((resolve, reject) => {
        const payload = JSON.stringify({
            move: "c2-c4",
            source: "physical", 
            roomId: roomId,
            playerSide: playerSide
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

        console.log(`\n🧪 Test 2: Simulating ESP32 physical move c2-c4`);
        console.log(`📤 Room: ${roomId}, Player: ${playerSide}`);

        const req = http.request(options, (res) => {
            let data = '';
            res.on('data', (chunk) => data += chunk);
            res.on('end', () => {
                try {
                    const response = JSON.parse(data);
                    console.log(`📥 Server response (${res.statusCode}):`, response);
                    
                    if (res.statusCode === 200) {
                        console.log('✅ Physical move accepted by server');
                    } else {
                        console.log('❌ Physical move rejected by server');
                    }
                    resolve(response);
                } catch (err) {
                    console.log('📄 Raw response:', data);
                    resolve({ statusCode: res.statusCode, data });
                }
            });
        });

        req.on('error', reject);
        req.write(payload);
        req.end();
    });
}

// Test 3: Check what the frontend should receive
function checkFrontendEvents() {
    console.log('\n📱 Test 3: Frontend connection check');
    console.log('🌐 Frontend should be running at: http://localhost:5174');
    console.log('🔗 Frontend connects to: http://localhost:3001 (Socket.IO)');
    console.log('💡 Frontend should automatically receive physicalMove events');
    console.log('\n📋 Troubleshooting checklist:');
    console.log('  ✓ Open http://localhost:5174 in browser');
    console.log('  ✓ Start single player mode');
    console.log('  ✓ Choose white or black pieces');
    console.log('  ✓ Open browser console (F12) to see debug messages');
    console.log('  ✓ Look for "ESP MOVE RECEIVED" messages');
}

// Main diagnostic function
async function runDiagnostics() {
    try {
        console.log('🎯 Diagnosing why c2-c4 move is not appearing...\n');
        
        // Test 1: Check game state
        await checkGameState();
        
        // Test 2a: Test single player mode (white player)
        console.log('\n' + '='.repeat(60));
        console.log('🎮 SINGLE PLAYER MODE TESTS');
        console.log('='.repeat(60));
        
        await testPhysicalMove('singleplayer-default', 'white');
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        // Test 2b: Test single player mode (black player)  
        await testPhysicalMove('singleplayer-default', 'black');
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        // Test 2c: Test multiplayer mode
        console.log('\n' + '='.repeat(60));
        console.log('🎮 MULTIPLAYER MODE TEST');
        console.log('='.repeat(60));
        
        await testPhysicalMove('default', 'white');
        
        // Test 3: Frontend guidance
        checkFrontendEvents();
        
        console.log('\n🏁 Diagnostic completed!');
        console.log('\n🔧 LIKELY SOLUTIONS:');
        console.log('1. Make sure ESP32 is connected to WiFi');
        console.log('2. Check ESP32 serial monitor for "PHYSICAL MOVE" messages');
        console.log('3. Verify ESP32 IP matches server IP in code');
        console.log('4. Ensure button is pressed after web moves to re-enable sensors');
        console.log('5. Check browser console for physicalMove events');
        
    } catch (error) {
        console.error('❌ Diagnostic failed:', error.message);
    }
}

// Run diagnostics
runDiagnostics();
