// Direct Arduino Communication Test
// This will simulate ESP32 sending moves to Arduino

const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

console.log('🔍 Arduino Direct Communication Test');
console.log('This will send moves directly to Arduino to test motor execution');
console.log('');

// List available serial ports first
SerialPort.list().then(ports => {
  console.log('📡 Available Serial Ports:');
  ports.forEach(port => {
    console.log(`   ${port.path} - ${port.manufacturer || 'Unknown'}`);
  });
  console.log('');
  
  // You'll need to update this with your Arduino's port
  const arduinoPort = '/dev/cu.usbmodem'; // Update this path for your Arduino
  
  console.log('⚠️  MANUAL SETUP REQUIRED:');
  console.log('1. Connect Arduino to computer via USB');
  console.log('2. Open Arduino IDE Serial Monitor at 115200 baud');
  console.log('3. Find Arduino port in the list above');
  console.log('4. Update the arduinoPort variable in this script');
  console.log('5. Run this test again');
  console.log('');
  console.log('🎯 Expected Arduino Serial Output when working:');
  console.log('   📨 RAW RECEIVED: \'e2-e4\' (Length: 5)');
  console.log('   ARDUINO_RECEIVED');
  console.log('   🔍 PARSING MOVE: e2-e4');
  console.log('   ✅ VALID MOVE FORMAT - From: e2, To: e4');
  console.log('   📍 COORDINATE CONVERSION: From e2(3,1) → To e4(3,3)');
  console.log('   REQUEST_BOARD_STATE');
  console.log('   🚀 STARTING MOVE EXECUTION: e2-e4');
  console.log('   📍 Moving to source: e2');
  console.log('   🔧 X Movement: 0 squares (no X movement needed)');
  console.log('   🔧 Y Movement: 1 squares UP (+Y)');
  console.log('   ✅ REACHED SOURCE - Current Position: (3,1)');
  console.log('   MAGNET_ON');
  console.log('   ➡️ EXECUTING STRAIGHT MOVE to: e4');
  console.log('   🔧 Y Movement: 2 squares UP (+Y)');
  console.log('   🏁 FINAL POSITION: (3,3) = e4');
  console.log('   Move complete');
  console.log('   MAGNET_OFF');
  console.log('   ✅ MOVE COMPLETED SUCCESSFULLY. Ready for next move.');
  console.log('   MOVE_COMPLETED');
  console.log('');
  console.log('🔧 TROUBLESHOOTING:');
  console.log('   - If no serial output: Check Arduino power and USB connection');
  console.log('   - If "RAW RECEIVED" but no movement: Check motor power supply');
  console.log('   - If motors make noise but don\'t move: Check step_delay timing');
  console.log('   - If movement is wrong direction: Check motor wiring');
  
}).catch(err => {
  console.error('❌ Error listing serial ports:', err);
});

// Uncomment and modify this section once you have the correct port
/*
function testArduinoDirectly() {
  const port = new SerialPort({
    path: '/dev/cu.usbmodem14101', // Update with your Arduino port
    baudRate: 115200
  });
  
  const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));
  
  port.on('open', () => {
    console.log('✅ Connected to Arduino');
    console.log('📤 Sending test move: e2-e4');
    
    // Send a simple test move
    port.write('e2-e4\n');
    
    // Send another move after 5 seconds
    setTimeout(() => {
      console.log('📤 Sending second test move: d2-d4');
      port.write('d2-d4\n');
    }, 5000);
    
    // Close after 10 seconds
    setTimeout(() => {
      console.log('🔚 Test complete - closing connection');
      port.close();
    }, 10000);
  });
  
  parser.on('data', (data) => {
    console.log('📥 Arduino:', data.trim());
  });
  
  port.on('error', (err) => {
    console.error('❌ Serial port error:', err);
  });
}

// Call testArduinoDirectly() after updating the port path
*/
