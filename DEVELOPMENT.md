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
- Frontend: http://localhost:5173
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

# Test Stockfish integration
curl -X POST http://localhost:3001/getBestMove \
  -H "Content-Type: application/json" \
  -d '{"fen":"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","depth":10}'

# Health check
curl http://localhost:3001/health
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
- Stockfish process management
- REST API endpoints
- CORS configuration
- Error handling

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

Happy coding! 🏆
