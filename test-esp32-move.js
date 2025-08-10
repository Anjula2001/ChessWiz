#!/usr/bin/env node

// Test script to simulate ESP32 sending physical moves to server
// This tests the game mode logic and player positioning

const http = require('http');

// Test scenarios
const testScenarios = [
  {
    name: "Single Player - Physical Player WHITE (bottom)",
    roomId: "singleplayer-default", 
    playerSide: "white",
    move: "e2-e4",
    description: "Physical player chooses WHITE, should appear on bottom"
  },
  {
    name: "Single Player - Physical Player BLACK (bottom)", 
    roomId: "singleplayer-default",
    playerSide: "black", 
    move: "e7-e5",
    description: "Physical player chooses BLACK, should appear on bottom"
  },
  {
    name: "Multiplayer - Physical Player TOP",
    roomId: "default",
    playerSide: "black",
    move: "d7-d5", 
    description: "Multiplayer mode: Physical player on top, web player on bottom"
  }
];

function sendPhysicalMove(scenario) {
  return new Promise((resolve, reject) => {
    const payload = JSON.stringify({
      move: scenario.move,
      source: "physical",
      roomId: scenario.roomId,
      playerSide: scenario.playerSide
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

    console.log(`\n🧪 Testing: ${scenario.name}`);
    console.log(`📝 Description: ${scenario.description}`);
    console.log(`📤 Sending: ${scenario.move} as ${scenario.playerSide} to room ${scenario.roomId}`);

    const req = http.request(options, (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        console.log(`📥 Response Status: ${res.statusCode}`);
        try {
          const response = JSON.parse(data);
          console.log(`✅ Server Response:`, response);
          resolve(response);
        } catch (err) {
          console.log(`📄 Raw Response:`, data);
          resolve({ statusCode: res.statusCode, data });
        }
      });
    });

    req.on('error', (err) => {
      console.error(`❌ Request Error:`, err.message);
      reject(err);
    });

    req.write(payload);
    req.end();
  });
}

async function runTests() {
  console.log('🎮 ESP32 Physical Move Test Suite');
  console.log('================================');
  
  for (const scenario of testScenarios) {
    try {
      await sendPhysicalMove(scenario);
      await new Promise(resolve => setTimeout(resolve, 1000)); // Wait 1 second between tests
    } catch (error) {
      console.error(`❌ Test failed:`, error.message);
    }
  }
  
  console.log('\n🏁 Test suite completed!');
  console.log('\n📋 Next Steps:');
  console.log('1. Open frontend at http://localhost:5174');
  console.log('2. Start single player mode');
  console.log('3. Check if physical moves appear on correct side (bottom)');
  console.log('4. Test both WHITE and BLACK player selections');
}

// Run the tests
runTests().catch(console.error);
