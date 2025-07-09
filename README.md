# ChessWizzz 🏆

A modern, full-stack chess application with an intelligent AI opponent powered by Stockfish. Experience professional-grade chess gameplay with beautiful UI and advanced engine analysis.

![ChessWizzz](https://img.shields.io/badge/ChessWizzz-Full%20Stack%20Chess%20Game-blue)
![React](https://img.shields.io/badge/React-Frontend-61DAFB)
![Node.js](https://img.shields.io/badge/Node.js-Backend-green)
![Stockfish](https://img.shields.io/badge/Stockfish-Chess%20Engine-red)

## ✨ Features

### 🎮 Game Features
- **Intelligent AI Opponent**: Powered by latest Stockfish dev-20250702-ce73441f
- **Advanced Difficulty Levels**: 8 levels from beginner (1000 ELO) to maximum (3500+ ELO)
- **3000+ ELO Engine Support**: Superhuman (3200 ELO) and Maximum (3500+ ELO) difficulty
- **Unlimited Strength Mode**: Full engine power for highest difficulties
- **Flexible Player Color**: Play as White, Black, or Random
- **Real-time Move Validation**: Legal move checking with chess.js
- **Game Statistics**: Track your wins, losses, and draws
- **Move History**: Complete game notation and move tracking
- **Check & Checkmate Detection**: Visual indicators and game state management

### 🎨 User Interface
- **Modern Responsive Design**: Beautiful chess board with piece images
- **Drag & Drop Interface**: Intuitive piece movement
- **Board Orientation**: Automatic board flipping for black pieces
- **Move Highlighting**: Visual feedback for last moves
- **Real-time Status**: Live game status and turn indicators

### 🔧 Technical Features
- **Full-stack Architecture**: React frontend + Express.js backend
- **Latest Stockfish Integration**: dev-20250702-ce73441f with optimized settings
- **3000+ ELO Support**: Superhuman AI with unlimited strength mode
- **Enhanced Error Handling**: No more 500 errors, comprehensive fallback responses
- **Multi-threaded Analysis**: 8-thread parallel processing with 1GB hash
- **Real-time Communication**: REST API for move analysis
- **Persistent Statistics**: Local storage for game tracking
- **Professional Engine Configuration**: Optimized for maximum performance

## 🚀 Quick Start

### Prerequisites
- Node.js (v16 or higher)
- npm or yarn
- Stockfish chess engine

### Installation

1. **Clone the repository**
```bash
git clone https://github.com/yourusername/chesswizzz.git
cd chesswizzz
```

2. **Install Stockfish**
```bash
# macOS
brew install stockfish

# Ubuntu/Debian
sudo apt-get install stockfish

# Windows
# Download from https://stockfishchess.org/
```

3. **Setup Backend**
```bash
cd backend
npm install
npm run dev  # Development mode with auto-restart
```

4. **Setup Frontend** (in a new terminal)
```bash
cd frontend
npm install
npm run dev  # Starts Vite development server
```

5. **Open the game**
   - Frontend: http://localhost:5173
   - Backend: http://localhost:3001

## 📁 Project Structure

```
chesswizzz/
├── backend/                 # Express.js server
│   ├── server.js           # Main server file with Stockfish integration
│   ├── package.json        # Backend dependencies
│   ├── README.md           # Backend documentation
│   └── .gitignore         # Backend ignore rules
├── frontend/               # React + Vite application
│   ├── src/
│   │   ├── components/     # React components
│   │   │   ├── ChessGame.jsx       # Main game component
│   │   │   ├── GameModeSelection.jsx
│   │   │   ├── HomePage.jsx
│   │   │   └── MultiplayerChess.jsx
│   │   ├── images/         # Chess piece images
│   │   ├── useSimpleAI.js  # AI integration hook
│   │   ├── App.jsx         # Main app component
│   │   └── main.jsx        # App entry point
│   ├── public/             # Static assets
│   ├── package.json        # Frontend dependencies
│   ├── vite.config.js      # Vite configuration
│   └── index.html          # HTML template
├── README.md               # Main project documentation
└── .gitignore             # Project ignore rules
```

## 🎯 Game Modes

### Single Player vs AI
- **Beginner**: 800-1200 ELO - Perfect for learning
- **Intermediate**: 1200-1600 ELO - Challenging gameplay
- **Advanced**: 1600-2000 ELO - Strong tactical play
- **Expert**: 2000-2400 ELO - Master-level analysis
- **Grandmaster**: 2400+ ELO - World-class strength

### Customization Options
- Choose your color (White/Black/Random)
- Adjustable thinking time
- Real-time position evaluation
- Move history and game analysis

## 🔌 API Endpoints

### Backend API
```
POST /getBestMove
- Request: { fen: string, depth: number }
- Response: { bestMove: string, evaluation: object, message: string }

GET /health
- Response: { status: "healthy", stockfish: boolean }
```

## 🛠️ Development

### Backend Development
```bash
cd backend
npm run dev  # Auto-restart on file changes
```

### Frontend Development
```bash
cd frontend
npm run dev  # Hot reload development server
```

### Build for Production
```bash
# Backend
cd backend
npm start

# Frontend
cd frontend
npm run build
npm run preview
```

## 🧪 Testing

Test the backend connection:
```bash
curl -X POST http://localhost:3001/getBestMove \
  -H "Content-Type: application/json" \
  -d '{"fen":"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","depth":10}'
```

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📝 Future Improvements

- [ ] Multiplayer support with WebSocket
- [ ] Opening book database
- [ ] Advanced position analysis
- [ ] Game save/load functionality
- [ ] Tournament mode
- [ ] Chess puzzles and training
- [ ] Mobile responsive optimization
- [ ] User accounts and ratings
- [ ] Game replay system
- [ ] Advanced statistics dashboard

## 🐛 Troubleshooting

### Common Issues

**Stockfish not found:**
```bash
# Ensure Stockfish is installed and in PATH
which stockfish
stockfish --help
```

**Backend connection fails:**
- Check if backend is running on port 3001
- Verify CORS settings
- Check console for error messages

**Frontend build issues:**
```bash
# Clear cache and reinstall
rm -rf node_modules package-lock.json
npm install
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **Stockfish**: The powerful open-source chess engine
- **chess.js**: Chess game logic and validation
- **React**: Frontend framework
- **Vite**: Fast build tool and development server
- **Express.js**: Backend web framework

---

**Enjoy playing ChessWizzz! 🎯**

*Made with ❤️ for chess enthusiasts*
