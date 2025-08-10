const axios = require('axios');

async function testCommunicationLoopFix() {
  console.log('🧪 Testing Communication Loop Fix');
  console.log('=====================================');
  
  try {
    // Test 1: Make an AI move to verify ESP32 receives it
    console.log('\n📤 Step 1: Making AI move c7-c5...');
    
    const aiMoveResponse = await axios.post('http://localhost:3000/api/move', {
      move: 'c7-c5',
      gameId: 'test-game',
      playerId: 'ai'
    });
    
    console.log('✅ AI move sent to server:', aiMoveResponse.data);
    
    // Wait for ESP32 to process
    await new Promise(resolve => setTimeout(resolve, 2000));
    
    // Test 2: Check current game state
    console.log('\n📊 Step 2: Checking game state...');
    
    const gameStateResponse = await axios.get('http://localhost:3000/api/game/test-game/state');
    console.log('🎯 Current game state:', gameStateResponse.data);
    
    // Test 3: Verify no communication loop issues
    console.log('\n🔍 Step 3: Communication Loop Verification');
    console.log('Expected behavior:');
    console.log('- ESP32 should receive c7-c5 move');
    console.log('- ESP32 should send clean move to Arduino: "c7-c5"');
    console.log('- Arduino should execute c7-c5 (not repeat h2-h4)');
    console.log('- No acknowledgment parsing loops');
    
    console.log('\n✅ Test complete - Check Arduino serial monitor');
    console.log('   Arduino should show: "Received move: c7-c5"');
    console.log('   Arduino should NOT show: "Received move: h2-h4"');
    
  } catch (error) {
    console.error('❌ Test failed:', error.message);
    if (error.response) {
      console.error('Response:', error.response.data);
    }
  }
}

// Check if server is running first
async function checkServer() {
  try {
    const response = await axios.get('http://localhost:3000/health');
    console.log('✅ Server is running');
    return true;
  } catch (error) {
    console.log('❌ Server not running. Start with: npm run dev');
    return false;
  }
}

async function main() {
  const serverRunning = await checkServer();
  if (serverRunning) {
    await testCommunicationLoopFix();
  }
}

main();
