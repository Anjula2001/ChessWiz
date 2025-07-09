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
    
    // Resolve the first pending request
    for (const [id, request] of this.pendingRequests) {
      if (request.resolve) {
        clearTimeout(request.timeout);
        request.resolve(bestMove);
        this.pendingRequests.delete(id);
        console.log(`✅ Request ${id} resolved with move: ${bestMove}`);
        break;
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

  async getBestMove(fen, depth = 15) {
    return new Promise((resolve, reject) => {
      if (!this.isReady) {
        reject(new Error('Stockfish engine is not ready'));
        return;
      }

      const requestId = ++this.requestId;
      console.log(`🔍 Starting analysis request ${requestId} for position: ${fen}`);
      
      // Set up request timeout
      const timeout = setTimeout(() => {
        if (this.pendingRequests.has(requestId)) {
          this.pendingRequests.delete(requestId);
          reject(new Error(`Analysis timeout after 30 seconds for request ${requestId}`));
        }
      }, 30000);

      // Store the request
      this.pendingRequests.set(requestId, { resolve, reject, timeout });

      // Send commands to Stockfish
      this.sendCommand('stop'); // Stop any previous analysis
      this.sendCommand(`position fen ${fen}`);
      this.sendCommand(`go depth ${depth}`);
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

// Main endpoint: Get best move from Stockfish
app.post('/getBestMove', async (req, res) => {
  const startTime = Date.now();
  
  try {
    const { fen, depth = 15 } = req.body;
    
    // Validate FEN string
    if (!fen || typeof fen !== 'string') {
      return res.status(400).json({ 
        error: 'Valid FEN string is required',
        received: typeof fen
      });
    }
    
    // Validate depth
    const analysisDepth = Math.max(1, Math.min(30, parseInt(depth) || 15));
    
    console.log(`🔥 Analysis request: FEN="${fen}", Depth=${analysisDepth}`);
    
    // Get best move from Stockfish
    const bestMove = await stockfish.getBestMove(fen, analysisDepth);
    
    const analysisTime = Date.now() - startTime;
    
    // Validate move result
    if (!bestMove || bestMove === '(none)' || bestMove === 'null') {
      return res.status(400).json({
        error: 'No valid move found for the given position',
        fen: fen,
        analysisTime: `${analysisTime}ms`
      });
    }
    
    // Return successful response
    res.json({
      success: true,
      fen: fen,
      bestMove: bestMove,
      depth: analysisDepth,
      analysisTime: `${analysisTime}ms`,
      engine: 'Stockfish 17.1',
      message: 'Best move calculated successfully'
    });
    
    console.log(`✅ Analysis complete in ${analysisTime}ms: ${bestMove}`);
    
  } catch (error) {
    const analysisTime = Date.now() - startTime;
    console.error('❌ Analysis error:', error.message);
    
    // Handle specific error types
    if (error.message.includes('timeout')) {
      res.status(408).json({ 
        error: 'Analysis timeout - Stockfish took too long to respond',
        analysisTime: `${analysisTime}ms`,
        suggestion: 'Try reducing the analysis depth'
      });
    } else if (error.message.includes('not ready')) {
      res.status(503).json({ 
        error: 'Chess engine is not ready, please try again in a moment',
        engineStatus: 'initializing'
      });
    } else {
      res.status(500).json({ 
        error: 'Internal server error during chess analysis',
        analysisTime: `${analysisTime}ms`,
        details: error.message
      });
    }
  }
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
    availableEndpoints: ['GET /', 'POST /getBestMove']
  });
});

// Start server
const server = app.listen(PORT, () => {
  console.log('🎮 ==========================================');
  console.log(`🎯 Chesswizzz Backend Server started!`);
  console.log(`🌐 Server URL: http://localhost:${PORT}`);
  console.log(`🩺 Health check: http://localhost:${PORT}/`);
  console.log(`♟️  Chess API: http://localhost:${PORT}/getBestMove`);
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
