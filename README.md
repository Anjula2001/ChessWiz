# ChessWizzz 🏆

A revolutionary intelligent chess system combining physical board automation with AI-powered gameplay. Built with ESP32, Arduino, React, and Stockfish engine integration.

## 🎯 Features

- **Physical Chess Board**: Automated piece movement with stepper motors
- **AI Integration**: Powered by Stockfish engine with 8 difficulty levels
- **Dual Game Modes**: Single-player vs AI and multiplayer modes
- **Real-time Detection**: Hall sensor matrix for move detection
- **Web Interface**: Modern React-based chess interface
- **Hardware Communication**: ESP32-Arduino bridge system
- **Smart Movement**: Intelligent knight pathfinding and collision avoidance

## 🛠️ Technology Stack

### Hardware
- **ESP32**: WiFi communication and sensor management
- **Arduino**: Motor control and movement execution
- **Hall Sensors**: 64-sensor matrix for piece detection
- **Stepper Motors**: Precise piece movement automation
- **Multiplexers**: Efficient sensor scanning

### Software
- **Frontend**: React + Vite
- **Backend**: Node.js + Express + Socket.io
- **AI Engine**: Stockfish chess engine
- **Real-time Communication**: WebSocket for live gameplay

## 📁 Project Structure

```
ChessWizzz/
├── frontend/           # React web interface
│   ├── src/
│   │   ├── components/
│   │   │   ├── ChessGame.jsx
│   │   │   ├── MultiplayerChess.jsx
│   │   │   └── GameModeSelection.jsx
│   │   └── images/     # Chess piece images
│   └── package.json
├── backend/            # Node.js server
│   ├── server.js       # Main server with Stockfish integration
│   └── package.json
├── Esp code/           # ESP32 firmware
│   └── esp.ino         # WiFi bridge and sensor management
├── Arduino code/       # Arduino firmware
│   └── arduino.ino     # Motor control and movement logic
└── README.md
```

## 🚀 Quick Start

### Prerequisites

1. **Stockfish Engine**: Install on your system
   ```bash
   # macOS (using Homebrew)
   brew install stockfish
   
   # Ubuntu/Debian
   sudo apt-get install stockfish
   
   # Windows: Download from https://stockfishchess.org/download/
   ```

2. **Node.js**: Version 16 or higher
3. **Arduino IDE**: For uploading firmware
4. **ESP32 Board Package**: Install in Arduino IDE

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/Anjula2001/Chesswizzz.git
   cd Chesswizzz
   ```

2. **Setup Backend**
   ```bash
   cd backend
   npm install
   npm start
   ```
   Server will run on `http://localhost:3001`

3. **Setup Frontend**
   ```bash
   cd frontend
   npm install
   npm run dev
   ```
   Web interface will run on `http://localhost:5173`

4. **Configure Hardware**
   - Update WiFi credentials in `Esp code/esp.ino`
   - Update server IP address in ESP32 code
   - Upload firmware to ESP32 and Arduino

## 🎮 How to Play

### Web Interface
1. Open `http://localhost:5173` in your browser
2. Choose game mode (Single Player vs AI or Multiplayer)
3. Select difficulty level (Beginner to Maximum)
4. Make moves on the web interface

### Physical Board
1. Ensure hardware is properly connected and powered
2. Physical moves are automatically detected by hall sensors
3. AI/web moves are executed by the motor system
4. Use the button to re-enable sensors after motor moves

## ⚙️ Configuration

### Server Settings
- **Port**: 3001 (configurable via environment variable)
- **Stockfish Path**: Auto-detected or configurable
- **CORS**: Enabled for all origins

### Hardware Settings
- **WiFi Credentials**: Configure in `esp.ino`
- **Server IP**: Update ESP32 endpoints
- **Motor Speed**: Adjustable in `arduino.ino`
- **Sensor Sensitivity**: Configurable debounce settings

### AI Difficulty Levels

| Level | Depth | Time Limit | ELO Rating | Description |
|-------|-------|------------|------------|-------------|
| Beginner | 6 | 500ms | 1000 | Learning friendly |
| Easy | 8 | 800ms | 1300 | Casual play |
| Intermediate | 10 | 1200ms | 1600 | Balanced challenge |
| Advanced | 12 | 2000ms | 2000 | Strong opponent |
| Expert | 15 | 3000ms | 2400 | Tournament level |
| Grandmaster | 18 | 5000ms | 2800 | Master level |
| Superhuman | 22 | 8000ms | 3200 | Near perfect |
| Maximum | 25 | 12000ms | 3500 | Absolute strongest |

## 🔧 API Endpoints

### Chess Engine
- `POST /getBestMove` - Get AI move for position
- `GET /engine-info` - Stockfish engine information
- `GET /health` - Server health check

### Hardware Communication
- `POST /physicalMove` - Receive physical board moves
- `GET /getLastMove` - Poll for web/AI moves
- `GET /getAnyMove` - Unified move polling for ESP32

## 🏗️ Hardware Setup

### Wiring Diagram
```
ESP32 → Arduino: UART communication (GPIO1/3)
ESP32 → Sensors: 4x 16-channel multiplexers
Arduino → Motors: Stepper motor drivers
```

### Pin Configuration

#### ESP32
- **MUX Control**: GPIO 14, 27, 26, 25
- **MUX Signals**: GPIO 4, 16, 32, 33
- **Magnet**: GPIO 23
- **Buttons**: GPIO 19 (sensor enable), GPIO 18 (reset)
- **Arduino Reset**: GPIO 5

#### Arduino
- **Stepper A**: Step GPIO 2, Dir GPIO 5
- **Stepper B**: Step GPIO 3, Dir GPIO 6
- **Enable**: GPIO 8
- **Limit Switches**: GPIO 9 (X), GPIO 10 (Y)

## 🧠 System Architecture

### Dual-Core Processing (ESP32)
- **Core 0**: WiFi communication and Arduino coordination
- **Core 1**: Sensor scanning and button handling
- **FreeRTOS**: Task management and inter-core communication

### Movement Intelligence
- **Smart Pathfinding**: Optimized knight movement paths
- **Collision Avoidance**: Real-time piece detection
- **Minimal Grid Usage**: Direct L-paths when possible
- **Castling Support**: Automatic rook movement handling

## 📊 Performance Optimizations

- **Fast Sensor Scanning**: 15ms intervals with 20ms debouncing
- **Motor Speed**: 350μs step delay for quick movements
- **Memory Efficiency**: Bit-packed board state (8 bytes vs 64 bytes)
- **Network Optimization**: Minimal WiFi checks and auto-reconnection

## 🐛 Troubleshooting

### Common Issues

1. **Stockfish Not Found**
   ```bash
   which stockfish  # Check if installed
   brew install stockfish  # Install on macOS
   ```

2. **ESP32 Connection Issues**
   - Verify WiFi credentials
   - Check server IP address
   - Ensure network connectivity

3. **Motor Movement Problems**
   - Check stepper motor connections
   - Verify power supply
   - Calibrate limit switches

4. **Sensor Detection Issues**
   - Verify hall sensor wiring
   - Check multiplexer connections
   - Adjust debounce settings

## 🔮 Future Enhancements

- [ ] Mobile app integration
- [ ] Tournament mode with time controls
- [ ] Online multiplayer with user accounts
- [ ] Voice move input
- [ ] Advanced chess variants support
- [ ] 3D printed board case design
- [ ] Machine learning move prediction

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🎖️ Acknowledgments

- **Stockfish**: The world's strongest chess engine
- **ESP32 Community**: For excellent hardware documentation
- **React Chess Libraries**: For UI inspiration
- **Arduino Community**: For motor control examples

## 📞 Contact

- **GitHub**: [@Anjula2001](https://github.com/Anjula2001)
- **Project**: [ChessWizzz](https://github.com/Anjula2001/Chesswizzz)

---

<div align="center">

**🏆 ChessWizzz - Where Physical Meets Digital Chess 🏆**

*Built with ❤️ by the ChessWizzz Team*

</div>
