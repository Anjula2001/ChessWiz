const express = require('express');
const cors = require('cors');
const { spawn } = require('child_process');

const app = express();
const PORT = process.env.PORT || 3001;

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
      message: `Best move calculated at ${difficulty} level`,
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
    availableEndpoints: ['GET /', 'GET /health', 'GET /engine-info', 'POST /getBestMove']
  });
});

// Start server
const server = app.listen(PORT, () => {
  console.log('🎮 ==========================================');
  console.log(`🎯 Chesswizzz Backend Server started!`);
  console.log(`🌐 Server URL: http://localhost:${PORT}`);
  console.log(`🩺 Health check: http://localhost:${PORT}/health`);
  console.log(`♟️  Chess API: http://localhost:${PORT}/getBestMove`);
  console.log(`⚙️  Engine Info: http://localhost:${PORT}/engine-info`);
  console.log('🎮 ==========================================');
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

module.exports = { app, stockfish };
