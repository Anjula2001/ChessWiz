import { useState, useEffect } from 'react'
import { Chess } from 'chess.js'
import { useSimpleAI } from '../useSimpleAI'
import './ChessGame.css'
import wP from '../images/wP.png'
import wR from '../images/wR.png'
import wN from '../images/wN.png'
import wB from '../images/wB.png'
import wQ from '../images/wQ.png'
import wK from '../images/wK.png'
import bP from '../images/bP.png'
import bR from '../images/bR.png'
import bN from '../images/bN.png'
import bB from '../images/bB.png'
import bQ from '../images/bQ.png'
import bK from '../images/bK.png'

// Create a mapping of piece codes to images
const pieceImages = {
  wp: wP, wr: wR, wn: wN, wb: wB, wq: wQ, wk: wK,
  bp: bP, br: bR, bn: bN, bb: bB, bq: bQ, bk: bK,
};

function ChessGame({ difficulty, playerColor, onBackToHome }) {
  const [game, setGame] = useState(new Chess());
  const [pieces, setPieces] = useState({});
  const [draggedPiece, setDraggedPiece] = useState(null);
  const [draggedFrom, setDraggedFrom] = useState(null);
  const [lastMoveFrom, setLastMoveFrom] = useState(null);
  const [lastMoveTo, setLastMoveTo] = useState(null);
  const [status, setStatus] = useState('');
  const [actualPlayerColor, setActualPlayerColor] = useState('white');
  const [moveHistory, setMoveHistory] = useState([]);
  const [showEvaluation, setShowEvaluation] = useState(true);
  const [showNewGameConfirm, setShowNewGameConfirm] = useState(false);
  
  // Initialize game stats from localStorage or default values
  const [gameStats, setGameStats] = useState(() => {
    const savedStats = localStorage.getItem('chessGameStats');
    if (savedStats) {
      try {
        return JSON.parse(savedStats);
      } catch (error) {
        console.error('Error parsing saved stats:', error);
      }
    }
    return {
      playerWins: 0,
      aiWins: 0,
      draws: 0,
      totalGames: 0
    };
  });

  const { 
    isReady, 
    bestMove, 
    getBestMove, 
    isThinking, 
    eloRating, 
    currentSettings, 
    resetAI,
    setBestMove
  } = useSimpleAI(difficulty);

  // Determine actual player color based on selection
  useEffect(() => {
    if (playerColor === 'random') {
      setActualPlayerColor(Math.random() > 0.5 ? 'white' : 'black');
    } else {
      setActualPlayerColor(playerColor);
    }
  }, [playerColor]);

  // Convert chess.js board state to our pieces format
  const updateBoardState = (gameInstance = game) => {
    const newPieces = {};
    for (let i = 0; i < 8; i++) {
      for (let j = 0; j < 8; j++) {
        const square = String.fromCharCode('a'.charCodeAt(0) + j) + (8 - i);
        const piece = gameInstance.get(square);
        if (piece) {
          const pieceCode = (piece.color === 'w' ? 'w' : 'b') + piece.type.toUpperCase();
          newPieces[square] = pieceCode;
        }
      }
    }
    setPieces(newPieces);
    updateGameStatus(gameInstance);
  };

  // Update game status
  const updateGameStatus = (gameInstance = game) => {
    let statusText = '';
    
    if (gameInstance.isGameOver()) {
      if (gameInstance.isCheckmate()) {
        const winner = gameInstance.turn() === 'w' ? 'Black' : 'White';
        statusText = `🎉 Checkmate! ${winner} wins!`;
        
        // Update stats based on actual player vs AI, not piece color
        const playerColorCode = actualPlayerColor === 'white' ? 'White' : 'Black';
        const isPlayerWin = winner === playerColorCode;
        
        if (isPlayerWin) {
          setGameStats(prev => ({
            ...prev,
            playerWins: prev.playerWins + 1,
            totalGames: prev.totalGames + 1
          }));
        } else {
          setGameStats(prev => ({
            ...prev,
            aiWins: prev.aiWins + 1,
            totalGames: prev.totalGames + 1
          }));
        }
      } else if (gameInstance.isDraw()) {
        if (gameInstance.isStalemate()) {
          statusText = '🤝 Draw by stalemate';
        } else if (gameInstance.isThreefoldRepetition()) {
          statusText = '🤝 Draw by repetition';
        } else if (gameInstance.isInsufficientMaterial()) {
          statusText = '🤝 Draw by insufficient material';
        } else {
          statusText = '🤝 Draw';
        }
        setGameStats(prev => ({
          ...prev,
          draws: prev.draws + 1,
          totalGames: prev.totalGames + 1
        }));
      }
    } else if (gameInstance.isCheck()) {
      statusText = `⚠️ ${gameInstance.turn() === 'w' ? 'White' : 'Black'} is in check`;
    } else {
      statusText = `${gameInstance.turn() === 'w' ? 'White' : 'Black'} to move`;
    }
    
    setStatus(statusText);
  };

  const handleDragStart = (e, square) => {
    const piece = pieces[square];
    const playerColorCode = actualPlayerColor === 'white' ? 'w' : 'b';
    // Only allow dragging of player's pieces on their turn
    if (piece && piece[0] === playerColorCode && game.turn() === playerColorCode) {
      setDraggedPiece(piece);
      setDraggedFrom(square);
      e.dataTransfer.setData('text/plain', square);
      e.target.style.opacity = '0.5';
    } else {
      e.preventDefault();
      e.stopPropagation();
    }
  };

  const handleDragEnd = (e) => {
    e.target.style.opacity = '1';
  };

  const handleDragOver = (e) => {
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';
  };

  const handleDrop = (e, toSquare) => {
    e.preventDefault();
    const fromSquare = e.dataTransfer.getData('text/plain');
    const playerTurn = actualPlayerColor === 'white' ? 'w' : 'b';
    
    if (fromSquare && pieces[fromSquare] && pieces[fromSquare][0] === playerTurn && game.turn() === playerTurn) {
      makeMove(fromSquare, toSquare);
    }
    setDraggedPiece(null);
    setDraggedFrom(null);
  };

  const makeMove = (from, to) => {
    try {
      const move = game.move({
        from: from,
        to: to,
        promotion: 'q' // Always promote to queen for simplicity
      });

      if (move) {
        setLastMoveFrom(from);
        setLastMoveTo(to);
        
        // Create new game state
        const newGame = new Chess(game.fen());
        setGame(newGame);
        
        // Add move to history
        setMoveHistory(prev => [...prev, {
          move: move.san,
          from: from,
          to: to,
          fen: newGame.fen(),
          moveNumber: Math.ceil(newGame.history().length / 2)
        }]);
        
        updateBoardState();

        // Trigger AI move after player's move - use newGame state
        const aiColor = actualPlayerColor === 'white' ? 'b' : 'w';
        const currentTurn = newGame.turn();
        const gameOver = newGame.isGameOver();
        
        console.log(`🎯 Move analysis:`, {
          playerColor: actualPlayerColor,
          aiColor,
          currentTurn,
          gameOver,
          engineReady: isReady,
          shouldTriggerAI: currentTurn === aiColor && !gameOver && isReady
        });
        
        if (currentTurn === aiColor && !gameOver && isReady) {
          console.log(`🤖 Triggering AI move. Current FEN:`, newGame.fen());
          setTimeout(() => {
            console.log('🔄 Calling getBestMove...');
            getBestMove(newGame.fen());
          }, 300); // Reduced delay for faster response
        } else {
          console.log(`❌ AI move not triggered because:`, {
            wrongTurn: currentTurn !== aiColor,
            gameOver: gameOver,
            engineNotReady: !isReady
          });
        }
      }
    } catch (error) {
      console.log('Invalid move!', error);
    }
  };

  // Handle AI move
  useEffect(() => {
    const aiColor = actualPlayerColor === 'white' ? 'b' : 'w';
    console.log('AI move effect triggered:', { bestMove, aiColor, currentTurn: game.turn(), gameOver: game.isGameOver() });
    
    if (bestMove && game.turn() === aiColor && !game.isGameOver()) {
      console.log(`🤖 AI attempting move: ${bestMove}`);
      try {
        const move = game.move(bestMove);
        if (move) {
          console.log(`✅ AI move successful:`, move);
          setLastMoveFrom(move.from);
          setLastMoveTo(move.to);
          
          // Add AI move to history
          setMoveHistory(prev => [...prev, {
            move: move.san,
            from: move.from,
            to: move.to,
            fen: game.fen(),
            moveNumber: Math.ceil(game.history().length / 2),
            isAI: true
          }]);
          
          const newGame = new Chess(game.fen());
          setGame(newGame);
          updateBoardState();
          
          // Clear the best move to prevent re-application
          setBestMove(null);
          
        } else {
          console.error('❌ AI move failed - invalid move:', bestMove);
          setBestMove(null);
        }
      } catch (error) {
        console.error('❌ AI move error:', error, 'Move:', bestMove);
        setBestMove(null);
      }
    }
  }, [bestMove, actualPlayerColor, game]);

  // Initialize AI when ready
  useEffect(() => {
    if (isReady) {
      console.log(`✅ Chess Engine ${difficulty} ready with ${eloRating} ELO`);
      console.log('Current settings:', currentSettings);
    }
  }, [isReady, difficulty, eloRating]);

  useEffect(() => {
    console.log('ChessGame component initializing...');
    // Initialize the board
    const newGame = new Chess();
    setGame(newGame);
    updateBoardState();
    
    // Wait for Chess Engine to be ready before making AI moves
    const checkAndMakeAIMove = () => {
      if (actualPlayerColor === 'black' && isReady) {
        console.log('Player is black and Chess Engine is ready, requesting AI first move...');
        setTimeout(() => {
          getBestMove(newGame.fen());
        }, 300);
      } else if (actualPlayerColor === 'black' && !isReady) {
        console.log('Player is black but Chess Engine not ready yet, waiting...');
        setTimeout(checkAndMakeAIMove, 1000);
      }
    };
    
    checkAndMakeAIMove();
  }, [actualPlayerColor, isReady]);

  const handleNewGameClick = () => {
    // If it's a new game (no moves made), start directly
    if (moveHistory.length === 0) {
      startNewGame();
    } else {
      // Show confirmation dialog if game is in progress
      setShowNewGameConfirm(true);
    }
  };

  const confirmNewGame = () => {
    setShowNewGameConfirm(false);
    startNewGame();
  };

  const cancelNewGame = () => {
    setShowNewGameConfirm(false);
  };

  const startNewGame = () => {
    const newGame = new Chess();
    setGame(newGame);
    setLastMoveFrom(null);
    setLastMoveTo(null);
    setMoveHistory([]);
    updateBoardState(newGame);
    resetAI();
    
    // If player is black, AI should make the first move
    if (actualPlayerColor === 'black' && isReady) {
      setTimeout(() => {
        getBestMove(newGame.fen());
      }, 300);
    }
  };

  const resetGameStats = () => {
    const defaultStats = {
      playerWins: 0,
      aiWins: 0,
      draws: 0,
      totalGames: 0
    };
    setGameStats(defaultStats);
    localStorage.setItem('chessGameStats', JSON.stringify(defaultStats));
  };

  const renderBoard = () => {
    const squares = [];
    const boardFlipped = actualPlayerColor === 'black';
    
    // Add file labels (a-h)
    for (let i = 0; i < 8; i++) {
      const fileIndex = boardFlipped ? 7 - i : i;
      squares.push(
        <div
          key={`file-${i}`}
          className="board-label file-label"
          style={{ left: `${i * 70 + 35}px` }}
        >
          {String.fromCharCode('a'.charCodeAt(0) + fileIndex)}
        </div>
      );
    }

    // Add rank labels (1-8)
    for (let i = 0; i < 8; i++) {
      const rankNumber = boardFlipped ? i + 1 : 8 - i;
      squares.push(
        <div
          key={`rank-${i}`}
          className="board-label rank-label"
          style={{ top: `${i * 70 + 35}px` }}
        >
          {rankNumber}
        </div>
      );
    }

    // Add chess squares
    for (let i = 0; i < 64; i++) {
      const file = i % 8;
      const rank = Math.floor(i / 8);
      const light = (file + rank) % 2 === 0;
      
      // Flip the board when player is black
      const actualFile = boardFlipped ? 7 - file : file;
      const actualRank = boardFlipped ? rank + 1 : 8 - rank;
      const squareName = String.fromCharCode('a'.charCodeAt(0) + actualFile) + actualRank;
      
      const piece = pieces[squareName];
      const isInCheck = game.isCheck() && piece?.toLowerCase().endsWith('k') && piece[0] === game.turn();
      
      squares.push(
        <div 
          key={i}
          className={`square ${light ? 'light' : 'dark'} ${
            squareName === lastMoveFrom || squareName === lastMoveTo ? 'highlighted' : ''
          } ${isInCheck ? 'check' : ''}`}
          data-index={i}
          onDragOver={handleDragOver}
          onDrop={(e) => handleDrop(e, squareName)}
        >
          {piece && (
            <div 
              className="piece"
              draggable={piece[0] === (actualPlayerColor === 'white' ? 'w' : 'b') && game.turn() === (actualPlayerColor === 'white' ? 'w' : 'b')}
              onDragStart={(e) => handleDragStart(e, squareName)}
              onDragEnd={handleDragEnd}
            >
              <img src={pieceImages[piece.toLowerCase()]} alt={piece} />
            </div>
          )}
        </div>
      );
    }
    return squares;
  };

  // Save game stats to localStorage whenever they change
  useEffect(() => {
    localStorage.setItem('chessGameStats', JSON.stringify(gameStats));
  }, [gameStats]);

  // Test function to verify backend communication
  const testBackendCommunication = async () => {
    console.log('🧪 Testing backend communication...');
    try {
      const response = await fetch('http://localhost:3001/getBestMove', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ 
          fen: game.fen(),
          depth: 10 
        }),
      });

      if (response.ok) {
        const data = await response.json();
        console.log('✅ Backend communication successful!');
        console.log('📡 Response:', data);
        alert(`Backend Test Successful!\nBest move: ${data.bestMove}\nEngine: ${data.message}`);
      } else {
        console.error('❌ Backend response error:', response.status);
        alert(`Backend Error: ${response.status} ${response.statusText}`);
      }
    } catch (error) {
      console.error('❌ Backend communication failed:', error);
      alert(`Backend Communication Failed: ${error.message}`);
    }
  };

  return (
    <div className="chess-game">
      <div className="game-header">
        <button className="back-button" onClick={onBackToHome}>
          ← Back to Home
        </button>
        <div className="game-title">
          <h1><span className="chess">Chess</span><span className="wiz">Wiz</span></h1>
        </div>
        <button className="new-game-button" onClick={handleNewGameClick}>
          New Game
        </button>
        <button className="test-backend-button" onClick={testBackendCommunication} style={{marginLeft: '10px', backgroundColor: '#4CAF50'}}>
          Test Backend
        </button>
      </div>

      <div className="game-container">
        <div className="game-info-panel">
          <div className="game-stats">
            <div className="stats-header">
              <h3>Game Statistics</h3>
              <button 
                className="reset-stats-btn" 
                onClick={resetGameStats}
                title="Reset all game statistics"
              >
                🔄 Reset
              </button>
            </div>
            <div className="stats-grid">
              <div className="stat-item">
                <span className="stat-value">{gameStats.playerWins}</span>
                <span className="stat-label">Your Wins</span>
              </div>
              <div className="stat-item">
                <span className="stat-value">{gameStats.aiWins}</span>
                <span className="stat-label">AI Wins</span>
              </div>
              <div className="stat-item">
                <span className="stat-value">{gameStats.draws}</span>
                <span className="stat-label">Draws</span>
              </div>
              <div className="stat-item">
                <span className="stat-value">{gameStats.totalGames}</span>
                <span className="stat-label">Total Games</span>
              </div>
            </div>
          </div>

          {/* Enhanced Engine Status */}
          <div className="engine-info">
            {!isReady && <div className="engine-status loading">🔄 Loading Chess Engine...</div>}
            {isReady && !isThinking && (
              <div className="engine-status ready">
                ✅ Chess Engine Ready • {difficulty.charAt(0).toUpperCase() + difficulty.slice(1)} • {eloRating} ELO
              </div>
            )}
            {isThinking && (
              <div className="engine-status thinking">
                🧠 Chess Engine Thinking... • Level {currentSettings?.depth || 2} • {eloRating} ELO
              </div>
            )}
            
            {/* Position Evaluation */}
            {/* {showEvaluation && evaluation && (
              <div className="position-evaluation">
                <h4>Position Analysis</h4>
                <div className="eval-display">
                  {evaluation.type === 'cp' ? (
                    <span className={`eval-score ${evaluation.value > 0 ? 'positive' : evaluation.value < 0 ? 'negative' : 'neutral'}`}>
                      {evaluation.value > 0 ? '+' : ''}{evaluation.value.toFixed(1)}
                    </span>
                  ) : (
                    <span className={`eval-mate ${evaluation.value > 0 ? 'mate-positive' : 'mate-negative'}`}>
                      Mate in {Math.abs(evaluation.value)}
                    </span>
                  )}
                  <span className="eval-depth">Depth: {evaluation.depth}</span>
                </div>
              </div>
            )} */}
            
            <div className="engine-controls">
              {/* Debug info for Chess Engine */}
              <div style={{ fontSize: '0.8rem', color: '#94a3b8', marginBottom: '0.5rem' }}>
                Debug: Ready={isReady ? 'Yes' : 'No'} | Thinking={isThinking ? 'Yes' : 'No'} | Move={bestMove || 'None'}
              </div>
              
              {/* <button 
                onClick={() => setShowEvaluation(!showEvaluation)}
                className="toggle-eval-btn"
              >
                {showEvaluation ? '📊 Hide Eval' : '📊 Show Eval'}
              </button> */}
              {isThinking && (
                <button 
                  onClick={() => console.log('Stop thinking not implemented')}
                  className="stop-thinking-btn"
                >
                  ⏹️ Stop
                </button>
              )}
            </div>
          </div>
        </div>

        <div className="board-section">
          <div className="board-container">
            <div className="board-wrapper">
              <div className="row-labels">
                {(actualPlayerColor === 'black' ? [1, 2, 3, 4, 5, 6, 7, 8] : [8, 7, 6, 5, 4, 3, 2, 1]).map(num => (
                  <div key={num} className="row-label">{num}</div>
                ))}
              </div>
              <div className="board" id="board">
                {renderBoard()}
              </div>
            </div>
            <div className="column-labels">
              {(actualPlayerColor === 'black' ? ['h', 'g', 'f', 'e', 'd', 'c', 'b', 'a'] : ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h']).map(letter => (
                <div key={letter} className="column-label">{letter}</div>
              ))}
            </div>
            <div className="game-status">{status}</div>
          </div>
        </div>

        <div className="move-info-panel">
          <div className="move-history">
            <h3>Game Info</h3>
            <div className="current-position">
              <strong>Turn:</strong> {game.turn() === (actualPlayerColor === 'white' ? 'w' : 'b') ? 'You' : 'AI'}
            </div>
            <div className="move-count">
              <strong>Move:</strong> {Math.ceil(game.history().length / 2)}
            </div>
            <div className="difficulty-info">
              <strong>Difficulty:</strong> {difficulty.charAt(0).toUpperCase() + difficulty.slice(1)}
            </div>
            <div className="elo-info">
              <strong>AI Strength:</strong> {eloRating} ELO
            </div>
            
            {/* Move History */}
            <div className="moves-list">
              <h4>Recent Moves</h4>
              <div className="moves-container">
                {moveHistory.slice(-10).map((moveData, index) => (
                  <div key={index} className={`move-item ${moveData.isAI ? 'ai-move' : 'player-move'}`}>
                    <span className="move-number">{moveData.moveNumber}.</span>
                    <span className="move-notation">{moveData.move}</span>
                    <span className="move-squares">{moveData.from}-{moveData.to}</span>
                    {moveData.isAI && <span className="ai-badge">🤖</span>}
                  </div>
                ))}
                {moveHistory.length === 0 && (
                  <div className="no-moves">No moves yet</div>
                )}
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* New Game Confirmation Modal */}
      {showNewGameConfirm && (
        <div className="modal-overlay">
          <div className="confirmation-modal">
            <div className="modal-content">
              <h3>Start New Game?</h3>
              <p>Are you sure you want to start a new game? The current game will be lost.</p>
              <div className="modal-buttons">
                <button className="confirm-btn" onClick={confirmNewGame}>
                  Yes, Start New Game
                </button>
                <button className="cancel-btn" onClick={cancelNewGame}>
                  No, Continue Current Game
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

export default ChessGame;
