// Simple ESP32 AI Move Transfer Test
// This test simulates the exact flow: Physical move → AI response → ESP32 polling

const http = require('http');

console.log('🧪 ESP32 AI Move Transfer Test');
console.log('==============================');

async function makeRequest(options, postData = null) {
    return new Promise((resolve, reject) => {
        const req = http.request(options, (res) => {
            let responseData = '';
            res.on('data', (chunk) => {
                responseData += chunk;
            });
            res.on('end', () => {
                resolve({
                    statusCode: res.statusCode,
                    data: responseData
                });
            });
        });
        
        req.on('error', (e) => {
            reject(e);
        });
        
        if (postData) {
            req.write(postData);
        }
        req.end();
    });
}

async function testAiMoveTransfer() {
    try {
        console.log('\n1️⃣ Step 1: Simulating physical move (a2-a4)...');
        
        // Send physical move
        const physicalMoveData = JSON.stringify({
            move: 'a2-a4',
            source: 'physical',
            roomId: 'singleplayer-default',
            playerSide: 'white'
        });
        
        const physicalResponse = await makeRequest({
            hostname: 'localhost',
            port: 3001,
            path: '/physicalMove',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Content-Length': Buffer.byteLength(physicalMoveData)
            }
        }, physicalMoveData);
        
        console.log(`📤 Physical move sent - Status: ${physicalResponse.statusCode}`);
        console.log(`📥 Response: ${physicalResponse.data}`);
        
        // Wait for AI to process (AI should respond automatically)
        console.log('\n2️⃣ Step 2: Waiting 3 seconds for AI to process...');
        await new Promise(resolve => setTimeout(resolve, 3000));
        
        console.log('\n3️⃣ Step 3: Checking ESP32 SinglePlayer endpoint...');
        
        // Check ESP32 singleplayer endpoint (what ESP32 polls)
        const esp32Response = await makeRequest({
            hostname: 'localhost',
            port: 3001,
            path: '/getLastMove?roomId=singleplayer-default',
            method: 'GET'
        });
        
        console.log(`📡 ESP32 SinglePlayer endpoint - Status: ${esp32Response.statusCode}`);
        console.log(`📥 ESP32 Response: ${esp32Response.data}`);
        
        // Parse and analyze the response
        try {
            const data = JSON.parse(esp32Response.data);
            if (data.move && data.move !== 'null' && data.source === 'ai') {
                console.log('\n✅ SUCCESS: AI move found for ESP32!');
                console.log(`🎯 AI Move: ${data.move}`);
                console.log(`📍 Source: ${data.source}`);
                console.log(`👤 Player Side: ${data.playerSide}`);
                console.log('\n🎉 AI moves are correctly transferred to ESP32!');
            } else {
                console.log('\n❌ ISSUE: No AI move found for ESP32');
                console.log('This means AI moves are NOT reaching the ESP32 polling endpoint');
                
                // Check if there's any move at all
                if (data.move && data.move !== 'null') {
                    console.log(`Found move: ${data.move} from source: ${data.source}`);
                    console.log('The move exists but source is not "ai"');
                } else {
                    console.log('No moves found at all in the ESP32 endpoint');
                }
            }
        } catch (e) {
            console.log('❌ Parse error:', e.message);
            console.log('Raw response:', esp32Response.data);
        }
        
        console.log('\n4️⃣ Step 4: Also checking Unified endpoint...');
        
        // Check unified endpoint as backup
        const unifiedResponse = await makeRequest({
            hostname: 'localhost',
            port: 3001,
            path: '/getAnyMove',
            method: 'GET'
        });
        
        console.log(`📡 Unified endpoint - Status: ${unifiedResponse.statusCode}`);
        console.log(`📥 Unified Response: ${unifiedResponse.data}`);
        
        try {
            const data = JSON.parse(unifiedResponse.data);
            if (data.move && data.move !== 'null' && data.source === 'ai') {
                console.log('✅ AI move also found on unified endpoint!');
            } else {
                console.log('❌ No AI move on unified endpoint either');
            }
        } catch (e) {
            console.log('❌ Unified endpoint parse error:', e.message);
        }
        
    } catch (error) {
        console.error('❌ Test failed:', error.message);
    }
    
    console.log('\n==============================');
    console.log('🧪 Test Complete');
    console.log('==============================');
}

// Run the test
testAiMoveTransfer();
