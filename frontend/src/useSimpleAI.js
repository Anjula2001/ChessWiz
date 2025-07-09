// Chess AI with Stockfish Backend Integration and Simple AI Fallback
import { useEffect, useState, useCallback } from 'react';
import { Chess } from 'chess.js';

// Communication function to get best move from backend Stockfish API
const getStockfishMove = async (fen, difficulty = 'intermediate') => {
  try {
    console.log(`🌐 Sending request to Stockfish backend (difficulty: ${difficulty})...`);
    
    const response = await fetch('http://localhost:3001/getBestMove', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ 
        fen: fen,
        difficulty: difficulty  // Send difficulty instead of depth
      }),
    });

    if (!response.ok) {
      throw new Error(`Backend error: ${response.status} ${response.statusText}`);
    }

    const data = await response.json();
    console.log('📡 Backend response:', data);
    
    if (data.bestMove && data.bestMove !== '(none)') {
      return {
        move: data.bestMove,
        eloRating: data.eloRating,
        analysisTime: data.analysisTime,
        depth: data.analysisDepth,
        strength: data.strength,
        engine: data.engine
      };
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
  const [engineInfo, setEngineInfo] = useState(null);

  // Define the complete difficulty mapping that matches backend
  const DIFFICULTY_SETTINGS = {
    beginner: { eloRating: 1000, depth: 6, timeLimit: 500, skillLevel: 5 },
    easy: { eloRating: 1300, depth: 8, timeLimit: 800, skillLevel: 10 },
    intermediate: { eloRating: 1600, depth: 10, timeLimit: 1200, skillLevel: 15 },
    advanced: { eloRating: 2000, depth: 12, timeLimit: 2000, skillLevel: 18 },
    expert: { eloRating: 2400, depth: 15, timeLimit: 3000, skillLevel: 20 },
    grandmaster: { eloRating: 2800, depth: 18, timeLimit: 5000, skillLevel: 20 },
    superhuman: { eloRating: 3200, depth: 22, timeLimit: 8000, skillLevel: 20 },
    maximum: { eloRating: 3500, depth: 25, timeLimit: 12000, skillLevel: 20 }
  };

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
      // First try to get move from Stockfish backend with proper difficulty
      try {
        const stockfishResult = await getStockfishMove(fen, difficulty);
        if (stockfishResult && stockfishResult.move) {
          setBestMove(stockfishResult.move);
          setEngineInfo(stockfishResult);
          console.log(`🎯 Stockfish move (${difficulty}):`, stockfishResult.move, `(${stockfishResult.eloRating} ELO)`);
          setIsThinking(false);
          return;
        }
      } catch (backendError) {
        console.warn('⚠️ Backend unavailable, falling back to simple AI:', backendError.message);
      }
      
      // Fallback to simple AI if backend fails
      console.log('🔄 Using fallback simple AI...');
      
      // Simulate thinking time for fallback based on difficulty
      const difficultyConfig = DIFFICULTY_SETTINGS[difficulty] || DIFFICULTY_SETTINGS.intermediate;
      await new Promise(resolve => setTimeout(resolve, Math.min(difficultyConfig.timeLimit / 4, 2000)));
      
      const game = new Chess(fen);
      let move;
      const difficultySettings = {
        beginner: () => getRandomMove(game),
        easy: () => Math.random() > 0.3 ? getBestMoveSimple(game, 1) : getRandomMove(game),
        intermediate: () => getBestMoveSimple(game, 1),
        advanced: () => getBestMoveSimple(game, 2),
        expert: () => getBestMoveSimple(game, 2),
        grandmaster: () => getBestMoveSimple(game, 2),
        superhuman: () => getBestMoveSimple(game, 2),
        maximum: () => getBestMoveSimple(game, 2)
      };
      
      move = difficultySettings[difficulty] ? difficultySettings[difficulty]() : getBestMoveSimple(game, 1);
      
      if (move) {
        setBestMove(move);
        setEngineInfo({
          move,
          eloRating: difficultyConfig.eloRating,
          engine: 'Simple AI Fallback',
          fallback: true
        });
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
    setEngineInfo(null);
    console.log('🔄 AI reset');
  };

  // Get current difficulty configuration
  const currentConfig = DIFFICULTY_SETTINGS[difficulty] || DIFFICULTY_SETTINGS.intermediate;

  return {
    isReady,
    bestMove,
    getBestMove,
    isThinking,
    resetAI,
    setBestMove,
    eloRating: engineInfo?.eloRating || currentConfig.eloRating,
    engineInfo,
    currentSettings: { 
      difficulty,
      eloRating: currentConfig.eloRating,
      depth: currentConfig.depth,
      timeLimit: currentConfig.timeLimit,
      skillLevel: currentConfig.skillLevel,
      engine: engineInfo?.engine || 'Stockfish with Simple AI fallback',
      strength: difficulty === 'superhuman' || difficulty === 'maximum' ? 'UNLIMITED' : `${currentConfig.eloRating} ELO`
    },
    availableDifficulties: Object.keys(DIFFICULTY_SETTINGS)
  };
};
