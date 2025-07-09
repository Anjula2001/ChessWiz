# Chesswizzz Backend

A powerful Express.js backend server for the Chesswizzz chess game application, integrated with the Stockfish chess engine for professional-grade move analysis.

## Features

- Express server with CORS enabled
- JSON request/response handling
- **Stockfish chess engine integration** for real chess analysis
- `/getBestMove` endpoint that accepts FEN strings and returns optimal moves
- Configurable analysis depth
- Error handling and validation
- Graceful shutdown with proper resource cleanup

## Prerequisites

- Node.js (v14 or higher)
- Stockfish chess engine (automatically installed via Homebrew on macOS)

## Installation

1. Install dependencies:
```bash
npm install
```

2. Install Stockfish (if not already installed):
```bash
# macOS
brew install stockfish

# Ubuntu/Debian
sudo apt-get install stockfish

# Other systems: download from https://stockfishchess.org/
```

## Development

```bash
npm run dev
```

## Production

```bash
npm start
```

## API Endpoints

### GET /
Health check endpoint that returns server status.

### POST /getBestMove
Accepts a JSON payload with a FEN string and optional depth parameter. Returns the best move calculated by Stockfish.

**Request:**
```json
{
  "fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
  "depth": 15
}
```

**Response:**
```json
{
  "fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
  "bestMove": "e2e4",
  "depth": 15,
  "message": "Best move calculated by Stockfish"
}
```

**Parameters:**
- `fen` (required): Chess position in FEN notation
- `depth` (optional): Analysis depth (default: 15, range: 1-30)

## Stockfish Integration

The backend uses Node.js `child_process` to communicate with Stockfish:

1. Sends `position fen <FEN>` command to set the board position
2. Sends `go depth <DEPTH>` command to analyze the position
3. Parses the `bestmove` response from Stockfish output
4. Returns the move in standard algebraic notation (e.g., "e2e4")

## Error Handling

- **400**: Missing or invalid FEN string
- **408**: Request timeout (analysis took too long)
- **503**: Chess engine not ready
- **500**: Internal server error

## Server Configuration

- Default port: 3001
- CORS enabled for all origins
- JSON body parsing enabled
- Request timeout: 30 seconds
- Stockfish engine: Latest version via system installation
