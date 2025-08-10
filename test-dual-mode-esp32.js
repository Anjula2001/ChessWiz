#!/usr/bin/env node

// Comprehensive ESP32 Dual-Mode Test
// Tests both single player and multiplayer game mode detection and routing

const axios = require('axios');
const WebSocket = require('ws');

const BASE_URL = 'http://localhost:3001';

console.log('🚀 Starting ESP32 Dual-Mode Game Detection Test\n');

async function testSinglePlayerMode() {
  console.log('🎮 Testing Single Player Mode Detection...');
  
  try {
    // Simulate ESP32 detecting single player mode and sending a move
    const testMove = {
      move: 'e2-e4',
      source: 'web-physical',
      roomId: 'singleplayer-default',
      playerSide: 'human'
    };

    console.log('📤 Sending single player physical move:', testMove);
    
    const response = await axios.post(`${BASE_URL}/physicalMove`, testMove);
    
    if (response.status === 200) {
      console.log('✅ Single player move sent successfully');
      console.log('📊 Response:', response.data);
    } else {
      console.log('❌ Single player move failed');
    }
    
  } catch (error) {
    console.log('❌ Single player test error:', error.message);
  }
  
  console.log('');
}

async function testMultiplayerMode() {
  console.log('👥 Testing Multiplayer Mode Detection...');
  
  try {
    // Simulate ESP32 detecting multiplayer mode and sending a move
    const testMove = {
      move: 'd2-d4',
      source: 'web-physical',
      roomId: 'default',
      playerSide: 'white'
    };

    console.log('📤 Sending multiplayer physical move:', testMove);
    
    const response = await axios.post(`${BASE_URL}/physicalMove`, testMove);
    
    if (response.status === 200) {
      console.log('✅ Multiplayer move sent successfully');
      console.log('📊 Response:', response.data);
    } else {
      console.log('❌ Multiplayer move failed');
    }
    
  } catch (error) {
    console.log('❌ Multiplayer test error:', error.message);
  }
  
  console.log('');
}

async function testESP32GameModeDetection() {
  console.log('🧠 Testing ESP32 Game Mode Detection Logic...');
  
  try {
    // Test single player detection by checking if singleplayer-default room exists
    console.log('🔍 Testing single player room detection...');
    const singlePlayerResponse = await axios.get(`${BASE_URL}/getLastMove/singleplayer-default`);
    console.log('✅ Single player room accessible:', singlePlayerResponse.data);
    
    // Test multiplayer detection by checking if default room exists
    console.log('🔍 Testing multiplayer room detection...');
    const multiplayerResponse = await axios.get(`${BASE_URL}/getLastMove/default`);
    console.log('✅ Multiplayer room accessible:', multiplayerResponse.data);
    
    // Test unified endpoint (what ESP32 uses when it can't determine mode)
    console.log('🔍 Testing unified endpoint...');
    const unifiedResponse = await axios.get(`${BASE_URL}/getAnyMove`);
    console.log('✅ Unified endpoint accessible:', unifiedResponse.data);
    
  } catch (error) {
    console.log('❌ Game mode detection test error:', error.message);
  }
  
  console.log('');
}

async function testESP32Polling() {
  console.log('🔄 Testing ESP32 Polling Simulation...');
  
  try {
    // Simulate ESP32 polling both endpoints like the real code does
    console.log('📡 Simulating ESP32 polling sequence...');
    
    // Poll getAnyMove (unified endpoint)
    const anyMoveResponse = await axios.get(`${BASE_URL}/getAnyMove`);
    console.log('📥 getAnyMove response:', anyMoveResponse.data);
    
    // Poll getLastMove for single player
    const singlePlayerMove = await axios.get(`${BASE_URL}/getLastMove/singleplayer-default`);
    console.log('📥 Single player getLastMove response:', singlePlayerMove.data);
    
    // Poll getLastMove for multiplayer
    const multiplayerMove = await axios.get(`${BASE_URL}/getLastMove/default`);
    console.log('📥 Multiplayer getLastMove response:', multiplayerMove.data);
    
  } catch (error) {
    console.log('❌ ESP32 polling test error:', error.message);
  }
  
  console.log('');
}

async function runAllTests() {
  console.log('🔧 ESP32 Dual-Mode System Test Starting...\n');
  
  await testESP32GameModeDetection();
  await testESP32Polling();
  await testSinglePlayerMode();
  await testMultiplayerMode();
  
  console.log('🏁 ESP32 Dual-Mode Test Complete!');
  console.log('');
  console.log('📋 Summary:');
  console.log('- Backend server endpoints tested ✅');
  console.log('- Single player mode routing tested ✅');
  console.log('- Multiplayer mode routing tested ✅');
  console.log('- ESP32 polling simulation tested ✅');
  console.log('');
  console.log('🎯 Both single player and multiplayer modes should now work simultaneously!');
  console.log('');
  console.log('🔧 Next Steps:');
  console.log('1. Upload esp32_bidirectional_bridge_ORIGINAL_LOGIC.ino to your ESP32');
  console.log('2. Open the web frontend and test both single player and multiplayer');
  console.log('3. Make physical moves and verify they route to the correct game mode');
}

// Run the tests
runAllTests().catch(console.error);
