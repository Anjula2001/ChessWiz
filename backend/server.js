const express = require('express');
const cors = require('cors');
const { spawn } = require('child_process');
const http = require('http');
const socketIo = require('socket.io');

const app = express();
const PORT = process.env.PORT || 3001;
const server = http.createServer(app);
const io = socketIo(server, {
  cors: {
    origin: '*',
    methods: ['GET', 'POST'],
    credentials: true
  }
});

// Middleware
app.use(cors());
app.use(express.json());

// Stockfish process management
class StockfishManager {
  constructor() {
    this.process = null;
    this.isReady = false;
    this.outputBuffer = '';
    this.pendingRequests = new Map();
    this.requestId = 0;
    this.initializeStockfish();
  }

  initializeStockfish() {
    console.log('🚀 Starting Stockfish process...');
    
    try {
      // Start Stockfish process using child_process
      this.process = spawn('stockfish');
      
      this.process.stdout.on('data', (data) => {
        this.handleStockfishOutput(data.toString());
      });

      this.process.stderr.on('data', (data) => {
        console.error('Stockfish stderr:', data.toString());
      });

      this.process.on('error', (error) => {
        console.error('❌ Failed to start Stockfish:', error);
        this.isReady = false;
      });

      this.process.on('close', (code) => {
        console.log(`Stockfish process closed with code ${code}`);
        this.isReady = false;
        // Auto-restart on unexpected close
        if (code !== 0) {
          setTimeout(() => this.initializeStockfish(), 1000);
        }
      });

      // Initialize Stockfish with UCI protocol
      this.sendCommand('uci');
      
    } catch (error) {
      console.error('❌ Error initializing Stockfish:', error);
      this.isReady = false;
    }
  }

  optimizeStockfishSettings() {
    console.log('⚙️ Optimizing Stockfish settings for maximum performance...');
    
    // Set optimal hash size (512 MB for better performance)
    this.sendCommand('setoption name Hash value 512');
    
    // Use maximum threads available (adjust based on system)
    const threads = Math.min(8, require('os').cpus().length);
    this.sendCommand(`setoption name Threads value ${threads}`);
    
    // Enable pondering for better analysis
    this.sendCommand('setoption name Ponder value true');
    
    // Set skill level to maximum (20 = strongest)
    this.sendCommand('setoption name Skill Level value 20');
    
    // Disable UCI strength limiting for maximum power
    this.sendCommand('setoption name UCI_LimitStrength value false');
    
    // Enable WDL (Win/Draw/Loss) statistics
    this.sendCommand('setoption name UCI_ShowWDL value true');
    
    // Set MultiPV to 1 for single best move (faster)
    this.sendCommand('setoption name MultiPV value 1');
    
    // Reduce move overhead for faster analysis
    this.sendCommand('setoption name Move Overhead value 5');
    
    console.log(`✅ Stockfish optimized: ${threads} threads, 512MB hash, maximum strength`);
  }

  handleStockfishOutput(data) {
    this.outputBuffer += data;
    const lines = this.outputBuffer.split('\n');
    
    // Keep the last incomplete line in buffer
    this.outputBuffer = lines.pop() || '';
    
    for (const line of lines) {
      const trimmedLine = line.trim();
      if (trimmedLine) {
        console.log('📟 Stockfish:', trimmedLine);
        this.processStockfishLine(trimmedLine);
      }
    }
  }

  processStockfishLine(line) {
    // Handle UCI initialization
    if (line.includes('uciok')) {
      console.log('✅ UCI protocol initialized');
      this.sendCommand('isready');
      return;
    }
    
    // Handle ready confirmation
    if (line.includes('readyok')) {
      this.isReady = true;
      console.log('🎯 Stockfish is ready for analysis!');
      
      // Apply optimal settings after engine is ready
      this.optimizeStockfishSettings();
      return;
    }
    
    // Handle best move response
    if (line.startsWith('bestmove')) {
      this.handleBestMoveResponse(line);
      return;
    }
  }

  handleBestMoveResponse(line) {
    // Parse: "bestmove e2e4" or "bestmove e2e4 ponder e7e5"
    const parts = line.split(' ');
    const bestMove = parts[1];
    
    console.log(`🎯 Best move found: ${bestMove}`);
    
    // Resolve all pending requests with the best move
    for (const [id, request] of this.pendingRequests) {
      if (request.resolve) {
        clearTimeout(request.timeout);
        request.resolve(bestMove);
        this.pendingRequests.delete(id);
        console.log(`✅ Request ${id} resolved with move: ${bestMove}`);
        break; // Only resolve the first pending request
      }
    }
  }

  sendCommand(command) {
    if (this.process && !this.process.killed) {
      this.process.stdin.write(command + '\n');
      console.log('📤 Sent to Stockfish:', command);
      return true;
    }
    console.warn('⚠️ Cannot send command, Stockfish not available');
    return false;
  }

  async getBestMove(fen, depth = 18, timeLimit = null) {
    return new Promise((resolve, reject) => {
      if (!this.isReady) {
        reject(new Error('Stockfish engine is not ready'));
        return;
      }

      const requestId = ++this.requestId;
      const maxTimeout = timeLimit ? timeLimit + 2000 : 15000; // Add buffer to timeLimit
      
      console.log(`🔍 Starting analysis request ${requestId} for position: ${fen.substring(0, 50)}...`);
      console.log(`⚙️ Analysis settings: Depth=${depth}${timeLimit ? `, Time=${timeLimit}ms` : ''}`);
      
      // Set up request timeout with buffer
      const timeout = setTimeout(() => {
        if (this.pendingRequests.has(requestId)) {
          this.pendingRequests.delete(requestId);
          console.warn(`⏰ Analysis timeout for request ${requestId} after ${maxTimeout}ms`);
          reject(new Error(`Analysis timeout after ${maxTimeout}ms for request ${requestId}`));
        }
      }, maxTimeout);

      // Store the request
      this.pendingRequests.set(requestId, { resolve, reject, timeout });

      try {
        // Send commands to Stockfish for optimal analysis
        this.sendCommand('stop'); // Stop any previous analysis
        this.sendCommand('ucinewgame'); // Clear hash tables for fresh analysis
        this.sendCommand(`position fen ${fen}`);
        
        // Use either depth-based or time-based search
        if (timeLimit && timeLimit > 0) {
          this.sendCommand(`go movetime ${timeLimit}`);
        } else {
          this.sendCommand(`go depth ${depth}`);
        }
      } catch (error) {
        // Clean up on error
        if (this.pendingRequests.has(requestId)) {
          clearTimeout(timeout);
          this.pendingRequests.delete(requestId);
        }
        reject(error);
      }
    });
  }

  close() {
    if (this.process && !this.process.killed) {
      console.log('🛑 Shutting down Stockfish...');
      this.sendCommand('quit');
      
      setTimeout(() => {
        if (!this.process.killed) {
          this.process.kill('SIGTERM');
        }
      }, 1000);
    }
  }
}

// Initialize Stockfish manager
const stockfish = new StockfishManager();

// Store the most recent physical move received from ESP microcontroller
let lastPhysicalMove = null;
let lastPhysicalMoveTime = null;

// Periodic transfer status logging
setInterval(() => {
  const currentTime = new Date();
  if (!lastPhysicalMove || !lastPhysicalMoveTime || (currentTime - lastPhysicalMoveTime) > 30000) {
    console.log('\n' + '='.repeat(60));
    console.log('📡 TRANSFER STATUS (SERVER) 📡');
    console.log('transfering move is - not received');
    console.log('='.repeat(60));
  }
}, 30000); // Check every 30 seconds

// Socket.io game rooms and state management
const gameRooms = new Map(); // Store active game rooms

// Health check endpoint
app.get('/', (req, res) => {
  res.json({ 
    message: 'Chesswizzz Backend Server is running!',
    stockfishReady: stockfish.isReady,
    timestamp: new Date().toISOString()
  });
});

// Enhanced difficulty configuration for different playing levels
const DIFFICULTY_SETTINGS = {
  beginner: { depth: 6, timeLimit: 500, eloRating: 1000, skillLevel: 5 },
  easy: { depth: 8, timeLimit: 800, eloRating: 1300, skillLevel: 10 },
  intermediate: { depth: 10, timeLimit: 1200, eloRating: 1600, skillLevel: 15 },
  advanced: { depth: 12, timeLimit: 2000, eloRating: 2000, skillLevel: 18 },
  expert: { depth: 15, timeLimit: 3000, eloRating: 2400, skillLevel: 20 },
  grandmaster: { depth: 18, timeLimit: 5000, eloRating: 2800, skillLevel: 20 },
  superhuman: { depth: 22, timeLimit: 8000, eloRating: 3200, skillLevel: 20 },
  maximum: { depth: 25, timeLimit: 12000, eloRating: 3500, skillLevel: 20 }
};

// Main endpoint: Get best move from Stockfish
app.post('/getBestMove', async (req, res) => {
  const startTime = Date.now();
  
  try {
    const { fen, depth, difficulty = 'intermediate', timeLimit } = req.body;
    
    // Validate FEN string
    if (!fen || typeof fen !== 'string') {
      return res.status(400).json({ 
        error: 'Valid FEN string is required',
        received: typeof fen
      });
    }
    
    // Get difficulty settings
    const difficultyConfig = DIFFICULTY_SETTINGS[difficulty] || DIFFICULTY_SETTINGS.intermediate;
    
    // Use provided depth/timeLimit or defaults from difficulty
    const analysisDepth = depth ? Math.max(1, Math.min(30, parseInt(depth))) : difficultyConfig.depth;
    const analysisTime = timeLimit ? parseInt(timeLimit) : difficultyConfig.timeLimit;
    
    console.log(`🔥 Analysis request: Difficulty="${difficulty}", Depth=${analysisDepth}, Time=${analysisTime}ms`);
    console.log(`🎯 Target ELO: ${difficultyConfig.eloRating}, Skill Level: ${difficultyConfig.skillLevel}`);
    
    // Apply difficulty-specific settings to Stockfish
    if (difficulty === 'superhuman' || difficulty === 'maximum') {
      // Disable strength limiting for superhuman levels
      stockfish.sendCommand('setoption name UCI_LimitStrength value false');
      stockfish.sendCommand('setoption name Skill Level value 20');
      
      // Maximum performance settings for 3000+ ELO
      stockfish.sendCommand('setoption name Hash value 1024'); // Increase to 1GB
      stockfish.sendCommand('setoption name Threads value 8');
      stockfish.sendCommand('setoption name Move Overhead value 1'); // Minimize overhead
      
      console.log(`🚀 MAXIMUM STRENGTH: Unlimited ELO, 1GB hash, 8 threads`);
    } else if (difficulty === 'grandmaster' || difficulty === 'expert') {
      // High strength but with some ELO limiting
      stockfish.sendCommand('setoption name UCI_LimitStrength value false');
      stockfish.sendCommand('setoption name Skill Level value 20');
      
      console.log(`🏆 HIGH STRENGTH: ${difficultyConfig.eloRating} ELO target`);
    } else {
      // Limit strength for lower difficulties
      stockfish.sendCommand('setoption name UCI_LimitStrength value true');
      stockfish.sendCommand(`setoption name UCI_Elo value ${difficultyConfig.eloRating}`);
      stockfish.sendCommand(`setoption name Skill Level value ${difficultyConfig.skillLevel}`);
      
      console.log(`🎯 LIMITED STRENGTH: ${difficultyConfig.eloRating} ELO, Skill ${difficultyConfig.skillLevel}`);
    }
     // Get best move from Stockfish with enhanced error handling
    let bestMove;
    try {
      bestMove = await stockfish.getBestMove(fen, analysisDepth, analysisTime);
    } catch (error) {
      console.error(`❌ Stockfish analysis failed:`, error.message);
      
      // Return a fallback response instead of 500 error
      return res.status(200).json({
        bestMove: null,
        difficulty,
        eloRating: difficultyConfig.eloRating,
        analysisDepth,
        analysisTime: Date.now() - startTime,
        skillLevel: difficultyConfig.skillLevel,
        engine: 'Stockfish dev-20250702',
        error: 'Analysis timeout - engine is thinking deeply',
        message: `Analysis exceeded ${analysisTime}ms time limit`,
        fallback: true
      });
    }
    
    const totalTime = Date.now() - startTime;

    // Validate move result
    if (!bestMove || bestMove === '(none)' || bestMove === 'null' || bestMove === 'undefined') {
      console.warn('⚠️ Stockfish returned invalid move:', bestMove);
      
      // Return fallback instead of error
      return res.status(200).json({
        bestMove: null,
        difficulty,
        eloRating: difficultyConfig.eloRating,
        analysisDepth,
        analysisTime: totalTime,
        skillLevel: difficultyConfig.skillLevel,
        engine: 'Stockfish dev-20250702',
        error: 'No valid move found',
        message: 'Engine could not find a valid move for this position',
        fallback: true
      });
    }

    console.log(`✅ Analysis complete: ${bestMove} (${totalTime}ms, ${difficulty} level)`);
    
    // Enhanced response with analysis data
    res.json({
      bestMove,
      difficulty,
      eloRating: difficultyConfig.eloRating,
      analysisDepth,
      analysisTime: totalTime,
      skillLevel: difficultyConfig.skillLevel,
      engine: 'Stockfish dev-20250702-ce73441f',
      version: 'development',
      strength: difficulty === 'superhuman' || difficulty === 'maximum' ? 'UNLIMITED' : `${difficultyConfig.eloRating} ELO`,
      message: `Best move calculated at ${difficulty} level (${difficultyConfig.eloRating} ELO)`,
      success: true
    });

  } catch (error) {
    const totalTime = Date.now() - startTime;
    console.error('❌ Server error:', error.message);
    
    // Always return 200 with error details instead of 500
    res.status(200).json({
      bestMove: null,
      error: 'Server analysis error',
      details: error.message,
      analysisTime: totalTime,
      timestamp: new Date().toISOString(),
      success: false
    });
  }
});

// Get engine information endpoint
app.get('/engine-info', (req, res) => {
  res.json({
    engine: 'Stockfish',
    version: 'dev-20250702-ce73441f',
    ready: stockfish.isReady,
    supportedDifficulties: Object.keys(DIFFICULTY_SETTINGS),
    difficultySettings: DIFFICULTY_SETTINGS,
    features: [
      'UCI Protocol',
      'Multi-threading support',
      'Variable skill levels',
      'ELO-based strength limiting',
      'Deep position analysis',
      'Win/Draw/Loss evaluation'
    ]
  });
});

// Health check endpoint  
app.get('/health', (req, res) => {
  res.json({ 
    status: 'healthy',
    stockfishReady: stockfish.isReady,
    timestamp: new Date().toISOString(),
    engine: 'Stockfish dev-20250702-ce73441f'
  });
});

// Physical Move endpoint - Receives moves from ESP microcontroller
app.post('/physicalMove', (req, res) => {
  try {
    const { move, roomId = 'default', playerSide = 'white' } = req.body;
    
    // Enhanced transfer status logging
    console.log('\n' + '='.repeat(60));
    console.log('📡 TRANSFER STATUS (SERVER) 📡');
    console.log(`transfering move is - ${move || 'not received'}`);
    console.log('='.repeat(60));
    
    // Validate the move format (e.g., "e2-e4")
    if (!move || typeof move !== 'string' || !move.match(/^[a-h][1-8]-[a-h][1-8]$/)) {
      console.log('❌ TRANSFER FAILED - Invalid move format');
      console.log(`📡 TRANSFER STATUS: transfering move is - not received (invalid format: ${move})`);
      return res.status(400).json({ 
        error: 'Invalid move format',
        message: 'Move should be in format like "e2-e4"',
        received: move
      });
    }
    
    // Store the received move
    lastPhysicalMove = move;
    lastPhysicalMoveTime = new Date();
    console.log(`🎮 Physical move received from ESP: ${move} (${playerSide} player)`);
    console.log(`📡 TRANSFER STATUS: transfering move is - ${move} (received from ESP)`);
    
    // Convert from "e2-e4" format to "e2e4" format if needed
    const normalizedMove = move.replace('-', '');
    
    // Broadcast the move to all clients in the multiplayer room
    if (gameRooms.has(roomId)) {
      const gameRoom = gameRooms.get(roomId);
      gameRoom.lastMove = normalizedMove;
      gameRoom.moves.push(normalizedMove);
      
      console.log(`♟️ Physical move in room ${roomId}: ${normalizedMove} for ${playerSide} player`);
      console.log(`📡 TRANSFER STATUS: transfering move is - ${move} (broadcasting to room ${roomId})`);
      
      io.to(roomId).emit('physicalMove', { 
        move: normalizedMove, 
        source: 'esp',
        playerSide: playerSide,
        timestamp: new Date().toISOString()
      });
      
      // Also broadcast as a regular move for compatibility
      io.to(roomId).emit('moveMade', { 
        move: normalizedMove, 
        fromESP: true,
        playerSide: playerSide
      });
      
      console.log(`✅ TRANSFER SUCCESS: transfering move is - ${move} (successfully broadcast)`);
    } else {
      console.log(`⚠️ Room ${roomId} not found, broadcasting to all rooms`);
      console.log(`📡 TRANSFER STATUS: transfering move is - ${move} (broadcasting to all clients)`);
      
      // If no specific room, broadcast to all clients
      io.emit('physicalMove', { 
        move: normalizedMove, 
        source: 'esp',
        playerSide: playerSide,
        timestamp: new Date().toISOString()
      });
      
      console.log(`✅ TRANSFER SUCCESS: transfering move is - ${move} (broadcast to all clients)`);
    }
    
    // Return success response
    res.status(200).json({
      success: true,
      move: move,
      normalizedMove: normalizedMove,
      timestamp: new Date().toISOString(),
      roomId: roomId,
      message: `Move ${move} successfully received and broadcast to multiplayer room`
    });
    
  } catch (error) {
    console.error('❌ Error processing physical move:', error.message);
    res.status(500).json({
      error: 'Failed to process physical move',
      details: error.message,
      timestamp: new Date().toISOString()
    });
  }
});

// Test endpoint to manually store moves for ESP32 (for debugging)
app.post('/testMove', (req, res) => {
  const { roomId = 'singleplayer-default', move, fen, playerType, playerColor } = req.body;
  
  if (gameRooms.has(roomId)) {
    const gameRoom = gameRooms.get(roomId);
    gameRoom.lastMove = move;
    gameRoom.currentFen = fen || gameRoom.currentFen;
    
    // Apply the same filtering logic as Socket.IO handler
    let storedForESP = false;
    if (roomId === 'singleplayer-default') {
      // Single player mode: Only store AI moves
      if (!playerType || playerType === 'ai' || playerType === 'top') {
        gameRoom.lastMoveForESP = move;
        storedForESP = true;
        console.log(`📡 TEST MOVE STORED (Single Player - AI): ${move} for ESP32`);
      } else {
        storedForESP = false;
        console.log(`🚫 TEST MOVE FILTERED (Single Player - Human): ${move} NOT stored for ESP32`);
      }
    } else if (roomId === 'default') {
      // Multiplayer mode: Apply player type filtering
      if (playerType === 'bottom') {
        gameRoom.lastMoveForESP = move;
        storedForESP = true;
        console.log(`📡 TEST MOVE STORED (Multiplayer - Bottom Player): ${move} for ESP32 | Player: ${playerColor}`);
      } else if (playerType === 'top') {
        storedForESP = false;
        console.log(`🚫 TEST MOVE FILTERED (Multiplayer - Top Player): ${move} NOT stored for ESP32 | Player: ${playerColor}`);
      } else {
        // Legacy behavior - store for ESP32
        gameRoom.lastMoveForESP = move;
        storedForESP = true;
        console.log(`📡 TEST MOVE STORED (Multiplayer - Legacy): ${move} for ESP32 (no playerType specified)`);
      }
    }
    
    res.json({
      success: true,
      message: `Move ${move} ${storedForESP ? 'stored for ESP32' : 'filtered (not sent to ESP32)'} in room ${roomId}`,
      room: roomId,
      playerType: playerType || 'not specified',
      storedForESP: storedForESP,
      timestamp: new Date().toISOString()
    });
  } else {
    res.status(404).json({
      success: false,
      error: `Room ${roomId} not found`,
      timestamp: new Date().toISOString()
    });
  }
});

// Get last physical move endpoint
app.get('/getPhysicalMove', (req, res) => {
  res.json({
    move: lastPhysicalMove,
    timestamp: new Date().toISOString(),
    available: lastPhysicalMove !== null
  });
});

// Get last move for ESP32 polling endpoint
app.get('/getLastMove', (req, res) => {
  const { roomId = 'default' } = req.query;
  console.log(`🔍 ESP32 polling for moves in room: ${roomId}`);
  
  if (gameRooms.has(roomId)) {
    const gameRoom = gameRooms.get(roomId);
    const move = gameRoom.lastMoveForESP;
    
    if (move) {
      console.log(`📤 ESP32 MOVE FOUND: ${move} in room ${roomId}`);
      // Clear the move after serving it to ESP32
      gameRoom.lastMoveForESP = null;
      
      res.json({
        move: move,
        room: roomId,
        gameMode: gameRoom.gameMode || 'unknown',
        timestamp: new Date().toISOString(),
        success: true
      });
    } else {
      res.json({
        move: null,
        room: roomId,
        gameMode: gameRoom.gameMode || 'unknown',
        timestamp: new Date().toISOString(),
        success: false,
        message: 'No moves available'
      });
    }
  } else {
    console.log(`❌ ESP32 ERROR: Room ${roomId} does not exist`);
    res.status(404).json({
      error: `Room ${roomId} not found`,
      room: roomId,
      timestamp: new Date().toISOString(),
      success: false
    });
  }
});

// Unified ESP32 endpoint - gets moves from ANY game mode
app.get('/getAnyMove', (req, res) => {
  console.log(`🔍 ESP32 polling for ANY available moves`);
  
  // Check both single player and multiplayer rooms for moves
  const rooms = ['singleplayer-default', 'default'];
  
  for (const roomId of rooms) {
    if (gameRooms.has(roomId)) {
      const gameRoom = gameRooms.get(roomId);
      const move = gameRoom.lastMoveForESP;
      
      if (move) {
        console.log(`📤 ESP32 MOVE FOUND: ${move} from ${roomId}`);
        // Clear the move after serving it to ESP32
        gameRoom.lastMoveForESP = null;
        
        res.json({
          move: move,
          source: roomId,
          timestamp: new Date().toISOString(),
          success: true
        });
        return; // Found a move, return immediately
      }
    }
  }
  
  // No moves found in any room
  res.json({
    move: null,
    source: 'none',
    timestamp: new Date().toISOString(),
    success: false,
    message: 'No moves available in any room'
  });
});

// Error handling middleware
app.use((err, req, res, next) => {
  console.error('🚨 Server error:', err.stack);
  res.status(500).json({ 
    error: 'Internal server error',
    timestamp: new Date().toISOString()
  });
});

// 404 handler
app.use('*', (req, res) => {
  res.status(404).json({ 
    error: 'Endpoint not found',
    availableEndpoints: [
      'GET /', 
      'GET /health', 
      'GET /engine-info', 
      'POST /getBestMove',
      'POST /physicalMove',
      'GET /getPhysicalMove'
    ]
  });
});

// Start server
server.listen(PORT, () => {
  console.log('🎮 ==========================================');
  console.log(`🎯 Chesswizzz Backend Server started!`);
  console.log(`🌐 Server URL: http://localhost:${PORT}`);
  console.log(`🩺 Health check: http://localhost:${PORT}/health`);
  console.log(`♟️  Chess API: http://localhost:${PORT}/getBestMove`);
  console.log(`⚙️  Engine Info: http://localhost:${PORT}/engine-info`);
  console.log(`🎛️  Physical moves: http://localhost:${PORT}/physicalMove`);
  console.log(`📥 ESP32 move polling: http://localhost:${PORT}/getLastMove?roomId=default`);
  console.log(`🎯 ESP32 unified polling: http://localhost:${PORT}/getAnyMove`);
  console.log(`📤 Physical move status: http://localhost:${PORT}/getPhysicalMove`);
  console.log('🎮 ==========================================');
  
  // Initialize default rooms for ESP32 compatibility
  console.log('🏠 Initializing default rooms for ESP32...');
  gameRooms.set('singleplayer-default', {
    players: [],
    currentFen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
    moves: [],
    lastMove: null,
    gameMode: 'singleplayer',
    lastMoveForESP: null
  });
  
  gameRooms.set('default', {
    players: [],
    currentFen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
    moves: [],
    lastMove: null,
    gameMode: 'multiplayer',
    lastMoveForESP: null
  });
  
  console.log('✅ Default rooms created: singleplayer-default, default');
  
  // Show initial transfer status
  console.log('\n' + '='.repeat(60));
  console.log('📡 TRANSFER STATUS (SERVER) 📡');
  console.log('transfering move is - not received');
  console.log('='.repeat(60));
  console.log('='.repeat(60));
});

// Graceful shutdown
const gracefulShutdown = (signal) => {
  console.log(`\n🛑 Received ${signal}, shutting down gracefully...`);
  
  server.close(() => {
    console.log('📴 HTTP server closed');
    stockfish.close();
    
    setTimeout(() => {
      console.log('👋 Backend shutdown complete');
      process.exit(0);
    }, 1000);
  });
};

process.on('SIGINT', () => gracefulShutdown('SIGINT'));
process.on('SIGTERM', () => gracefulShutdown('SIGTERM'));

// Socket.io connection handler

// Socket.io connection handler
io.on('connection', (socket) => {
  console.log(`🔌 New client connected: ${socket.id}`);
  
  // Join a game room
  socket.on('joinGame', (roomId) => {
    socket.join(roomId);
    
    if (!gameRooms.has(roomId)) {
      gameRooms.set(roomId, {
        players: [],
        currentFen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1', // Starting position
        moves: [],
        lastMove: null
      });
    }
    
    const gameRoom = gameRooms.get(roomId);
    if (gameRoom.players.length < 2) {
      gameRoom.players.push(socket.id);
    }
    
    console.log(`👤 Player ${socket.id} joined room ${roomId}`);
    io.to(roomId).emit('gameState', gameRoom);
  });
  
  // Handle player move
  socket.on('move', ({ roomId, move, fen, playerType, playerColor }) => {
    if (gameRooms.has(roomId)) {
      const gameRoom = gameRooms.get(roomId);
      gameRoom.lastMove = move;
      gameRoom.currentFen = fen;
      gameRoom.moves.push(move);
      
      // Store move for ESP32 based on game mode and player type
      if (roomId === 'singleplayer-default') {
        // Single player mode: Only store AI moves (playerType should be 'ai' or undefined for backwards compatibility)
        if (!playerType || playerType === 'ai' || playerType === 'top') {
          gameRoom.lastMoveForESP = move;
          console.log(`📡 ESP32 STORAGE (Single Player - AI): ${move} stored for ESP32`);
        } else {
          console.log(`🚫 ESP32 FILTERED (Single Player - Human): ${move} NOT stored for ESP32`);
        }
      } else if (roomId === 'default') {
        // Multiplayer mode: Only store BOTTOM player moves
        if (playerType === 'bottom') {
          gameRoom.lastMoveForESP = move;
          console.log(`📡 ESP32 STORAGE (Multiplayer - Bottom Player): ${move} stored for ESP32 | Player: ${playerColor}`);
        } else if (playerType === 'top') {
          console.log(`🚫 ESP32 FILTERED (Multiplayer - Top Player): ${move} NOT stored for ESP32 | Player: ${playerColor}`);
        } else {
          // Fallback for moves without playerType (backward compatibility) - assume bottom player
          gameRoom.lastMoveForESP = move;
          console.log(`📡 ESP32 STORAGE (Multiplayer - Legacy): ${move} stored for ESP32 (no playerType specified)`);
        }
      }
      
      console.log(`♟️ Move in room ${roomId}: ${move}`);
      io.to(roomId).emit('moveMade', { move, fen });
      io.to(roomId).emit('gameState', gameRoom);
    }
  });
  
  // Handle new game start - clear physical move state
  socket.on('newGame', (roomId) => {
    console.log(`🔄 New game started in room ${roomId} - clearing physical move state`);
    lastPhysicalMove = null; // Clear the global physical move
    
    if (gameRooms.has(roomId)) {
      const gameRoom = gameRooms.get(roomId);
      gameRoom.currentFen = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'; // Reset to starting position
      gameRoom.moves = [];
      gameRoom.lastMove = null;
      
      // Broadcast new game state to all players in the room
      io.to(roomId).emit('gameState', gameRoom);
      io.to(roomId).emit('newGameStarted', { roomId });
    }
  });
  
  // Handle disconnection
  socket.on('disconnect', () => {
    console.log(`🔌 Client disconnected: ${socket.id}`);
    
    // Remove player from any game rooms
    for (const [roomId, gameRoom] of gameRooms.entries()) {
      const playerIndex = gameRoom.players.indexOf(socket.id);
      if (playerIndex !== -1) {
        gameRoom.players.splice(playerIndex, 1);
        console.log(`👤 Player ${socket.id} removed from room ${roomId}`);
        
        // Notify remaining players
        io.to(roomId).emit('playerLeft', { playerId: socket.id });
        io.to(roomId).emit('gameState', gameRoom);
        
        // Remove empty rooms
        if (gameRoom.players.length === 0) {
          gameRooms.delete(roomId);
          console.log(`🧹 Empty room ${roomId} removed`);
        }
      }
    }
  });
});

module.exports = { app, stockfish };
