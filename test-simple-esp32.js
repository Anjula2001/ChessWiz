const http = require('http');

console.log('🧪 Simple ESP32 AI Transfer Test');
console.log('=================================');

// Step 1: Send a physical move to trigger AI response
console.log('\n📤 Step 1: Sending physical move (a2-a4)...');

const physicalMoveData = JSON.stringify({
    move: 'a2-a4',
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
    console.log(`✅ Physical move response: ${res.statusCode}`);
    
    let responseData = '';
    res.on('data', (chunk) => {
        responseData += chunk;
    });
    
    res.on('end', () => {
        console.log('📥 Response:', responseData);
        
        // Step 2: Wait for AI to process, then check ESP32 endpoints
        console.log('\n⏳ Waiting 3 seconds for AI to process...');
        setTimeout(checkESP32Endpoints, 3000);
    });
});

physicalReq.on('error', (e) => {
    console.error('❌ Physical move error:', e.message);
    process.exit(1);
});

physicalReq.write(physicalMoveData);
physicalReq.end();

function checkESP32Endpoints() {
    console.log('\n🔍 Step 2: Checking ESP32 endpoints for AI moves...');
    
    // Check SinglePlayer endpoint (what ESP32 should use)
    console.log('\n📡 Checking SinglePlayer endpoint...');
    
    const singlePlayerOptions = {
        hostname: 'localhost',
        port: 3001,
        path: '/getLastMove?roomId=singleplayer-default',
        method: 'GET'
    };
    
    const singlePlayerReq = http.request(singlePlayerOptions, (res) => {
        console.log(`Status: ${res.statusCode}`);
        
        let responseData = '';
        res.on('data', (chunk) => {
            responseData += chunk;
        });
        
        res.on('end', () => {
            try {
                const data = JSON.parse(responseData);
                console.log('📥 SinglePlayer response:', JSON.stringify(data, null, 2));
                
                if (data.move && data.move !== 'null') {
                    console.log('🎯 ✅ SUCCESS: AI move found for ESP32!');
                    console.log(`   Move: ${data.move}`);
                    console.log(`   Source: ${data.source}`);
                    console.log(`   Player: ${data.playerSide}`);
                } else {
                    console.log('❌ ISSUE: No AI move found in SinglePlayer endpoint');
                }
                
                // Also check unified endpoint
                checkUnifiedEndpoint();
                
            } catch (e) {
                console.log('❌ Parse error:', e.message);
                console.log('Raw response:', responseData);
                checkUnifiedEndpoint();
            }
        });
    });
    
    singlePlayerReq.on('error', (e) => {
        console.error('❌ SinglePlayer endpoint error:', e.message);
        checkUnifiedEndpoint();
    });
    
    singlePlayerReq.end();
}

function checkUnifiedEndpoint() {
    console.log('\n📡 Checking Unified endpoint...');
    
    const unifiedOptions = {
        hostname: 'localhost',
        port: 3001,
        path: '/getAnyMove',
        method: 'GET'
    };
    
    const unifiedReq = http.request(unifiedOptions, (res) => {
        console.log(`Status: ${res.statusCode}`);
        
        let responseData = '';
        res.on('data', (chunk) => {
            responseData += chunk;
        });
        
        res.on('end', () => {
            try {
                const data = JSON.parse(responseData);
                console.log('📥 Unified response:', JSON.stringify(data, null, 2));
                
                if (data.move && data.move !== 'null') {
                    console.log('🎯 ✅ SUCCESS: AI move found on unified endpoint!');
                    console.log(`   Move: ${data.move}`);
                    console.log(`   Source: ${data.source}`);
                } else {
                    console.log('❌ ISSUE: No AI move found on unified endpoint');
                }
                
            } catch (e) {
                console.log('❌ Parse error:', e.message);
                console.log('Raw response:', responseData);
            }
            
            console.log('\n=================================');
            console.log('🧪 TEST COMPLETE');
            console.log('=================================');
            
            // Exit the script
            process.exit(0);
        });
    });
    
    unifiedReq.on('error', (e) => {
        console.error('❌ Unified endpoint error:', e.message);
        console.log('\n=================================');
        console.log('🧪 TEST COMPLETE (with errors)');
        console.log('=================================');
        process.exit(1);
    });
    
    unifiedReq.end();
}

// Timeout safety
setTimeout(() => {
    console.log('\n⏰ Test timeout - exiting');
    process.exit(1);
}, 10000);
