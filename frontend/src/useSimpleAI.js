// Chess AI with Stockfish Backend Integration and Simple AI Fallback
import { useEffect, useState, useCallback } from 'react';
import { Chess } from 'chess.js';

// Communication function to get best move from backend Stockfish API
const getStockfishMove = async (fen, depth = 15) => {
  try {
    console.log('🌐 Sending request to Stockfish backend...');
    
    const response = await fetch('http://localhost:3001/getBestMove', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ 
        fen: fen,
        depth: depth 
      }),
    });

    if (!response.ok) {
      throw new Error(`Backend error: ${response.status} ${response.statusText}`);
    }

    const data = await response.json();
    console.log('📡 Backend response:', data);
    
    if (data.bestMove && data.bestMove !== '(none)') {
      return data.bestMove;
    } else {
      throw new Error('No valid move received from backend');
    }
    
  } catch (error) {
    console.error('❌ Backend communication error:', error);
    throw error;
  }
};

// Simple chess AI using basic evaluation
const evaluatePosition = (game) => {
  const pieces = {
    p: 1, r: 5, n: 3, b: 3, q: 9, k: 0
  };
  
  let score = 0;
  const board = game.board();
  
  for (let i = 0; i < 8; i++) {
    for (let j = 0; j < 8; j++) {
      const piece = board[i][j];
      if (piece) {
        const value = pieces[piece.type] || 0;
        score += piece.color === 'w' ? value : -value;
      }
    }
  }
  
  return score;
};

const getRandomMove = (game) => {
  const moves = game.moves();
  return moves[Math.floor(Math.random() * moves.length)];
};

const getBestMoveSimple = (game, depth = 2) => {
  const moves = game.moves();
  if (moves.length === 0) return null;
  
  let bestMove = moves[0];
  let bestScore = -Infinity;
  
  for (const move of moves.slice(0, Math.min(moves.length, 10))) { // Limit moves for performance
    const gameCopy = new Chess(game.fen());
    gameCopy.move(move);
    
    let score = evaluatePosition(gameCopy);
    
    if (depth > 1 && moves.length < 20) { // Only go deeper if not too many moves
      const opponentMoves = gameCopy.moves().slice(0, 5); // Limit opponent moves
      let worstOpponentScore = Infinity;
      
      for (const opponentMove of opponentMoves) {
        const opponentGameCopy = new Chess(gameCopy.fen());
        opponentGameCopy.move(opponentMove);
        const opponentScore = evaluatePosition(opponentGameCopy);
        worstOpponentScore = Math.min(worstOpponentScore, opponentScore);
      }
      
      score = worstOpponentScore;
    }
    
    // Add some randomness for more natural play
    score += (Math.random() - 0.5) * 0.1;
    
    if (score > bestScore) {
      bestScore = score;
      bestMove = move;
    }
  }
  
  return bestMove;
};

export const useSimpleAI = (difficulty = 'intermediate') => {
  const [isReady, setIsReady] = useState(false);
  const [bestMove, setBestMove] = useState(null);
  const [isThinking, setIsThinking] = useState(false);

  useEffect(() => {
    // Simple AI is always ready
    setTimeout(() => {
      setIsReady(true);
      console.log('✅ Simple AI ready');
    }, 500);
  }, []);

  const getBestMove = useCallback(async (fen) => {
    console.log('🧠 AI calculating move for:', fen);
    
    if (!isReady) {
      console.warn('⚠️ AI not ready');
      return;
    }
    
    setIsThinking(true);
    setBestMove(null);
    
    try {
      // First try to get move from Stockfish backend
      const depthMap = {
        beginner: 8,
        intermediate: 12,
        advanced: 15,
        master: 18,
        grandmaster: 20
      };
      
      const depth = depthMap[difficulty] || 15;
      
      try {
        const stockfishMove = await getStockfishMove(fen, depth);
        if (stockfishMove) {
          setBestMove(stockfishMove);
          console.log('🎯 Stockfish move:', stockfishMove);
          setIsThinking(false);
          return;
        }
      } catch (backendError) {
        console.warn('⚠️ Backend unavailable, falling back to simple AI:', backendError.message);
      }
      
      // Fallback to simple AI if backend fails
      console.log('🔄 Using fallback simple AI...');
      
      // Simulate thinking time for fallback
      await new Promise(resolve => setTimeout(resolve, 500 + Math.random() * 1500));
      
      const game = new Chess(fen);
      let move;
      const difficultySettings = {
        beginner: () => getRandomMove(game),
        intermediate: () => Math.random() > 0.3 ? getBestMoveSimple(game, 1) : getRandomMove(game),
        advanced: () => getBestMoveSimple(game, 2),
        master: () => getBestMoveSimple(game, 2),
        grandmaster: () => getBestMoveSimple(game, 2)
      };
      
      move = difficultySettings[difficulty] ? difficultySettings[difficulty]() : getBestMoveSimple(game, 1);
      
      if (move) {
        setBestMove(move);
        console.log('🎯 Fallback AI move:', move);
      } else {
        console.warn('⚠️ No move found');
      }
      
      setIsThinking(false);
    } catch (error) {
      console.error('❌ AI error:', error);
      setIsThinking(false);
    }
  }, [isReady, difficulty]);

  const resetAI = () => {
    setBestMove(null);
    setIsThinking(false);
    console.log('🔄 AI reset');
  };

  return {
    isReady,
    bestMove,
    getBestMove,
    isThinking,
    resetAI,
    setBestMove,
    eloRating: difficulty === 'beginner' ? 1000 : difficulty === 'intermediate' ? 1400 : difficulty === 'advanced' ? 1800 : difficulty === 'master' ? 2200 : 2600,
    currentSettings: { 
      depth: difficulty === 'beginner' ? 8 : difficulty === 'intermediate' ? 12 : difficulty === 'advanced' ? 15 : difficulty === 'master' ? 18 : 20,
      engine: 'Stockfish with Simple AI fallback'
    }
  };
};
