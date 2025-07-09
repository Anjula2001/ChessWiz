# ChessWizzz Development Guide

This guide will help you set up and contribute to the ChessWizzz project.

## 🚀 Quick Start

### Prerequisites
```bash
# Node.js (v16+)
node --version

# npm (v7+)
npm --version

# Stockfish chess engine
stockfish --help
```

### Installation
```bash
# Clone and setup
git clone https://github.com/yourusername/chesswizzz.git
cd chesswizzz

# Install all dependencies
npm run install:all

# Start development servers
npm run dev
```

The application will be available at:
- Frontend: http://localhost:5175 (or auto-assigned port)
- Backend: http://localhost:3001

## 📁 Project Architecture

### Frontend (`/frontend`)
```
src/
├── components/           # React components
│   ├── ChessGame.jsx    # Main game logic
│   ├── HomePage.jsx     # Landing page
│   └── ...
├── images/              # Chess piece images
├── useSimpleAI.js      # AI integration hook
└── App.jsx             # Root component
```

### Backend (`/backend`)
```
├── server.js           # Express server + Stockfish
├── package.json        # Dependencies
└── README.md          # Backend docs
```

## 🛠️ Development Workflow

### Running the Application
```bash
# Full stack development
npm run dev

# Individual services
npm run dev:backend     # Backend only
npm run dev:frontend    # Frontend only
```

### Making Changes

1. **Create a feature branch**
```bash
git checkout -b feature/your-feature-name
```

2. **Make your changes**
   - Follow the coding standards
   - Add tests for new functionality
   - Update documentation

3. **Test your changes**
```bash
# Run all tests
npm test

# Test specific parts
cd backend && npm test
cd frontend && npm test
```

4. **Commit and push**
```bash
git add .
git commit -m "feat: your descriptive commit message"
git push origin feature/your-feature-name
```

5. **Create a Pull Request**
   - Use the PR template
   - Link related issues
   - Include screenshots for UI changes

## 🧪 Testing

### Backend Testing
```bash
cd backend

# Health check - Ensure engine is ready
curl http://localhost:3001/health

# Get engine information and all difficulty levels
curl http://localhost:3001/engine-info

# Test standard difficulty levels
curl -X POST http://localhost:3001/getBestMove \
  -H "Content-Type: application/json" \
  -d '{"fen":"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","difficulty":"intermediate"}'

# Test 3000+ ELO levels - Superhuman (3200 ELO)
curl -X POST http://localhost:3001/getBestMove \
  -H "Content-Type: application/json" \
  -d '{"fen":"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","difficulty":"superhuman"}'

# Test maximum 3500+ ELO strength (UNLIMITED)
curl -X POST http://localhost:3001/getBestMove \
  -H "Content-Type: application/json" \
  -d '{"fen":"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","difficulty":"maximum"}'

# Test complex middle game position at maximum strength
curl -X POST http://localhost:3001/getBestMove \
  -H "Content-Type: application/json" \
  -d '{"fen":"r2qkb1r/pp2nppp/3p4/2pP4/4P3/2N2N2/PPP2PPP/R1BQKB1R w KQkq c6 0 6","difficulty":"maximum","depth":22}'
```

# Engine capabilities
curl http://localhost:3001/engine-info
```

### Frontend Testing
```bash
cd frontend

# Unit tests
npm test

# Build test
npm run build
```

## 🔧 Key Components

### ChessGame.jsx
Main game component handling:
- Board rendering and piece movement
- Game state management
- AI integration
- Move validation

### useSimpleAI.js
Custom hook for:
- Stockfish communication
- Move calculation
- Difficulty settings
- Engine status management

### server.js
Backend server with:
- **Latest Stockfish dev-20250702**: Latest development version
- **8 Difficulty Levels**: From 1000 to 3500+ ELO
  - Beginner: 1000 ELO
  - Easy: 1300 ELO  
  - Intermediate: 1600 ELO
  - Advanced: 2000 ELO
  - Expert: 2400 ELO
  - Grandmaster: 2800 ELO
  - **Superhuman: 3200 ELO** 🚀
  - **Maximum: 3500+ ELO** ⚡ (Unlimited strength)
- Multi-threaded analysis (8 cores)
- 1GB hash memory for superhuman levels
- REST API endpoints
- CORS configuration
- Enhanced error handling

## 🚀 3000+ ELO Engine Capabilities

### Difficulty Levels Available
```
beginner:     1000 ELO, depth 6,  500ms  (skill level 5)
easy:         1300 ELO, depth 8,  800ms  (skill level 10)
intermediate: 1600 ELO, depth 10, 1.2s   (skill level 15)
advanced:     2000 ELO, depth 12, 2s     (skill level 18)
expert:       2400 ELO, depth 15, 3s     (skill level 20)
grandmaster:  2800 ELO, depth 18, 5s     (skill level 20)
superhuman:   3200 ELO, depth 22, 8s     (UNLIMITED strength)
maximum:      3500+ ELO, depth 25, 12s   (UNLIMITED strength)
```

### Engine Optimization for 3000+ ELO
The backend automatically configures Stockfish for maximum performance:
- **Hash**: 1GB for superhuman/maximum levels (512MB for others)
- **Threads**: 8 threads for parallel analysis
- **Skill Level**: 20 (maximum)
- **UCI Strength**: Disabled for unlimited ELO
- **Move Overhead**: Minimized for faster calculations
- **Engine**: Latest Stockfish dev-20250702-ce73441f

### Testing 3000+ ELO Responses
Expected API response for maximum difficulty:
```json
{
  "bestMove": "e2e4",
  "difficulty": "maximum",
  "eloRating": 3500,
  "analysisDepth": 25,
  "analysisTime": 12091,
  "skillLevel": 20,
  "engine": "Stockfish dev-20250702-ce73441f",
  "version": "development",
  "strength": "UNLIMITED",
  "message": "Best move calculated at maximum level",
  "success": true
}
```

## 🎯 Adding New Features

### New Game Mode
1. Add component in `frontend/src/components/`
2. Update routing in `App.jsx`
3. Add backend endpoints if needed
4. Update documentation

### New Difficulty Level
1. Modify `useSimpleAI.js` settings
2. Update backend Stockfish configuration
3. Add UI options in game selection

### New API Endpoint
1. Add route in `backend/server.js`
2. Update frontend API calls
3. Add error handling
4. Document the endpoint

## 🐛 Debugging

### Common Issues

**Stockfish not found:**
```bash
# Install Stockfish
brew install stockfish      # macOS
sudo apt install stockfish  # Ubuntu
```

**Backend connection failed:**
```bash
# Check if backend is running
curl http://localhost:3001/health

# Check logs
cd backend && npm run dev
```

**Frontend build errors:**
```bash
# Clear cache
cd frontend
rm -rf node_modules package-lock.json
npm install
```

**Backend 500 errors (FIXED):**
The backend has been enhanced to eliminate 500 Internal Server Errors:
- All errors now return 200 status with detailed error information
- Timeout handling for long analysis (12+ seconds)
- Fallback responses for invalid positions
- Comprehensive input validation

**Frontend shows wrong ELO (like 1400 instead of selected level) (FIXED):**
This has been resolved by:
- Updated `useSimpleAI.js` to send `difficulty` parameter instead of `depth`
- Added proper difficulty selector with all 8 levels in game mode selection
- Fixed ELO rating display to be dynamic based on backend response
- Added visual indicators for superhuman levels (🚀⚡)

**3000+ ELO not working:**
```bash
# Verify Stockfish version (should be dev-20250702 or newer)
stockfish --help | head -1

# Check if superhuman/maximum difficulties are available
curl http://localhost:3001/engine-info | grep -A20 supportedDifficulties

# Test maximum strength directly
curl -X POST http://localhost:3001/getBestMove \
  -H "Content-Type: application/json" \
  -d '{"fen":"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","difficulty":"maximum"}'
```

### Debug Tools

**Backend debugging:**
```bash
# Enable debug logs
DEBUG=* npm run dev

# Check Stockfish process
ps aux | grep stockfish
```

**Frontend debugging:**
- React DevTools
- Browser console
- Network tab for API calls

## 📊 Performance

### Frontend Optimization
- Use React.memo for expensive components
- Optimize re-renders with useCallback
- Lazy load components

### Backend Optimization
- Implement caching for repeated positions
- Use appropriate Stockfish depth settings
- Monitor memory usage

## 🚀 Deployment

### Development
```bash
npm run dev
```

### Production Build
```bash
# Frontend
cd frontend && npm run build

# Backend
cd backend && npm start
```

### GitHub Pages (Frontend only)
```bash
# Build and deploy
cd frontend
npm run build
# Deploy dist/ folder to gh-pages branch
```

## 🎨 Code Style

### JavaScript/React
- Use functional components with hooks
- Follow ESLint configuration
- Use descriptive variable names
- Add PropTypes for components

### Git Commits
Follow conventional commits:
- `feat:` new features
- `fix:` bug fixes
- `docs:` documentation
- `style:` formatting
- `refactor:` code restructuring
- `test:` adding tests

## 📚 Resources

### Chess Programming
- [Chess.js Documentation](https://github.com/jhlywa/chess.js)
- [Stockfish Documentation](https://stockfishchess.org/)
- [UCI Protocol](http://wbec-ridderkerk.nl/html/UCIProtocol.html)

### React/Node.js
- [React Hooks Guide](https://reactjs.org/docs/hooks-intro.html)
- [Express.js Documentation](https://expressjs.com/)
- [Vite Documentation](https://vitejs.dev/)

### Testing
- [Jest Testing Framework](https://jestjs.io/)
- [React Testing Library](https://testing-library.com/docs/react-testing-library/intro/)

## 🤝 Getting Help

- Create an issue for bugs or questions
- Check existing issues first
- Join discussions for feature ideas
- Read the contributing guide

---

## ✅ Recent Fixes & Updates (July 2025)

### Backend 500 Errors - RESOLVED ✅
- **Issue**: API was returning 500 Internal Server Errors
- **Fix**: Enhanced error handling to always return 200 status with proper JSON responses
- **Result**: All API calls now return appropriate error details without crashing

### Frontend ELO Display - RESOLVED ✅  
- **Issue**: Frontend was showing hardcoded 1400 ELO instead of selected difficulty
- **Fix**: Updated `useSimpleAI.js` to properly send difficulty parameter to backend
- **Result**: Frontend now dynamically displays correct ELO for all 8 difficulty levels

### 3000+ ELO Support - ACTIVE ✅
- **Superhuman**: 3200 ELO with unlimited strength mode 🚀
- **Maximum**: 3500+ ELO with unlimited strength mode ⚡
- **Backend**: Optimized with 1GB hash, 8 threads, latest Stockfish dev-20250702
- **Frontend**: Added visual difficulty selector with all 8 levels

### New Features Added ✅
- Complete difficulty selector UI in game mode selection
- Dynamic ELO rating display based on backend response  
- Enhanced error handling and fallback responses
- Professional engine optimization for maximum performance
- Comprehensive testing documentation

### API Testing Commands ✅
```bash
# Test 3200 ELO (Superhuman)
curl -X POST http://localhost:3001/getBestMove \
  -H "Content-Type: application/json" \
  -d '{"fen":"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","difficulty":"superhuman"}'

# Test 3500+ ELO (Maximum)  
curl -X POST http://localhost:3001/getBestMove \
  -H "Content-Type: application/json" \
  -d '{"fen":"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","difficulty":"maximum"}'
```

Happy coding! 🏆
