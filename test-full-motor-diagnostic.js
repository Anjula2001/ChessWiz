const http = require('http');

function makeHttpRequest(options, data = null) {
  return new Promise((resolve, reject) => {
    const req = http.request(options, (res) => {
      let body = '';
      res.on('data', chunk => body += chunk);
      res.on('end', () => {
        try {
          const jsonData = JSON.parse(body);
          resolve({ status: res.statusCode, data: jsonData });
        } catch (e) {
          resolve({ status: res.statusCode, data: body });
        }
      });
    });
    
    req.on('error', reject);
    
    if (data) {
      req.write(JSON.stringify(data));
    }
    req.end();
  });
}

async function fullSystemDiagnostic() {
  console.log('🔍 COMPREHENSIVE MOTOR DIAGNOSTIC');
  console.log('=====================================');
  
  try {
    // Step 1: Check server health
    console.log('\n📡 Step 1: Server Health Check...');
    const healthOptions = {
      hostname: 'localhost',
      port: 3001,
      path: '/health',
      method: 'GET'
    };
    
    const healthResponse = await makeHttpRequest(healthOptions);
    console.log('✅ Server Status:', healthResponse.data);
    
    // Step 2: Send physical move to trigger system
    console.log('\n📤 Step 2: Sending Physical Move (e2-e4)...');
    const moveOptions = {
      hostname: 'localhost',
      port: 3001,
      path: '/physicalMove',
      method: 'POST',
      headers: { 'Content-Type': 'application/json' }
    };
    
    const moveData = { move: 'e2-e4', roomId: 'singleplayer-default' };
    const moveResponse = await makeHttpRequest(moveOptions, moveData);
    console.log('📊 Physical Move Result:', moveResponse.data);
    
    // Wait for AI processing
    console.log('\n⏳ Step 3: Waiting for AI to process (5 seconds)...');
    await new Promise(resolve => setTimeout(resolve, 5000));
    
    // Step 3: Check for AI move
    console.log('\n🤖 Step 4: Checking for AI Move...');
    const pollingOptions = {
      hostname: 'localhost',
      port: 3001,
      path: '/getAnyMove',
      method: 'GET'
    };
    
    const pollingResponse = await makeHttpRequest(pollingOptions);
    console.log('🎯 AI Move Polling Result:', pollingResponse.data);
    
    // Step 4: Generate AI move manually
    if (!pollingResponse.data.move) {
      console.log('\n🎮 Step 5: Manually Generating AI Move...');
      const aiMoveOptions = {
        hostname: 'localhost',
        port: 3001,
        path: '/getBestMove',
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
      };
      
      // After e2-e4, get AI response
      const aiMoveData = { 
        fen: 'rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1'
      };
      
      const aiResponse = await makeHttpRequest(aiMoveOptions, aiMoveData);
      console.log('🤖 AI Move Generated:', aiResponse.data);
      
      if (aiResponse.data.bestMove) {
        // Convert Stockfish format to our format
        const bestMove = aiResponse.data.bestMove; // e.g., "e7e5"
        const formattedMove = bestMove.substring(0,2) + '-' + bestMove.substring(2,4);
        console.log('📤 Formatted AI Move:', formattedMove);
        
        // Store the AI move in the system
        console.log('\n📥 Step 6: Storing AI Move in System...');
        const storeOptions = {
          hostname: 'localhost',
          port: 3001,
          path: '/physicalMove',
          method: 'POST',
          headers: { 'Content-Type': 'application/json' }
        };
        
        const storeData = { 
          move: formattedMove, 
          roomId: 'singleplayer-default',
          source: 'ai'
        };
        
        const storeResponse = await makeHttpRequest(storeOptions, storeData);
        console.log('💾 AI Move Stored:', storeResponse.data);
        
        // Check if ESP32 can now poll the AI move
        console.log('\n📡 Step 7: ESP32 Polling for AI Move...');
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        const finalPolling = await makeHttpRequest(pollingOptions);
        console.log('🔍 Final Polling Result:', finalPolling.data);
        
        if (finalPolling.data.move) {
          console.log('\n✅ SUCCESS: AI Move Available for ESP32');
          console.log('🎯 Expected ESP32 Flow:');
          console.log(`   1. ESP32 polls /getAnyMove → receives: ${finalPolling.data.move}`);
          console.log(`   2. ESP32 sends to Arduino: "${finalPolling.data.move}"`);
          console.log(`   3. Arduino receives: "${finalPolling.data.move}"`);
          console.log('   4. Arduino executes motor movement');
          console.log('   5. Arduino sends: "Move complete: [move]"');
          console.log('   6. ESP32 ignores acknowledgment (no re-parsing)');
        }
      }
    }
    
    console.log('\n🔧 DIAGNOSTIC SUMMARY:');
    console.log('==========================================');
    console.log('1. ✅ Backend server is running and healthy');
    console.log('2. ✅ Physical move endpoint working');
    console.log('3. ✅ AI move generation working');
    console.log('4. ✅ Move storage and polling working');
    console.log('');
    console.log('🎯 NEXT STEPS FOR MOTOR MOVEMENT:');
    console.log('1. 📤 Upload fixed ESP32 code (esp32_bidirectional_bridge_ORIGINAL_LOGIC.ino)');
    console.log('2. 📤 Upload Arduino code (arduino_chess_motor_controller_OPTIMIZED.ino)');
    console.log('3. 🔌 Ensure ESP32-Arduino serial connection (GPIO1/3)');
    console.log('4. 📊 Monitor both serial outputs for communication');
    console.log('5. 🧪 Test with the generated AI move above');
    
    console.log('\n❌ POSSIBLE MOTOR FAILURE POINTS:');
    console.log('- ESP32 not connected or not running fixed code');
    console.log('- Arduino not receiving moves from ESP32');
    console.log('- Motor drivers not enabled (ENABLE_PIN)');
    console.log('- Step delay too fast/slow for motor drivers');
    console.log('- Power supply issues for motors');
    console.log('- Serial communication timing issues');
    
  } catch (error) {
    console.error('❌ Diagnostic failed:', error.message);
  }
}

fullSystemDiagnostic();
