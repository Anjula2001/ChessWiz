import { useState } from 'react';
import './HomePage.css';

function HomePage({ onPlayGame }) {
  const handlePlayGame = () => {
    onPlayGame('intermediate'); // Default difficulty
  };

  return (
    <div className="home-page">
      <div className="hero-section">
        <div className="hero-content">
          <div className="logo-section">
            <div className="chess-pieces">
              <span className="piece king">♔</span>
              <span className="piece queen">♕</span>
              <span className="piece rook">♖</span>
            </div>
            <h1 className="main-title">
              <span className="chess-text">Chess</span>
              <span className="wiz-text">Wiz</span>
            </h1>
            <p className="subtitle">Master the game of kings with intelligent AI</p>
          </div>

          <div className="cta-section">
            <button className="play-button" onClick={handlePlayGame}>
              <span className="play-icon">▶</span>
              Start Game
            </button>
            <p className="cta-subtitle">Choose your difficulty and game mode in the next step</p>
          </div>

          <div className="features-grid">
            <div className="feature-card">
              <div className="feature-icon">🎯</div>
              <h3>Professional Engine</h3>
              <p>Powered by advanced chess algorithms and intelligent AI</p>
            </div>
            <div className="feature-card">
              <div className="feature-icon">📈</div>
              <h3>Adaptive Difficulty</h3>
              <p>5 skill levels from beginner (1000 ELO) to grandmaster (3500+ ELO)</p>
            </div>
            <div className="feature-card">
              <div className="feature-icon">⚡</div>
              <h3>Instant Response</h3>
              <p>Fast, accurate moves with professional-grade analysis</p>
            </div>
            <div className="feature-card">
              <div className="feature-icon">🧩</div>
              <h3>Real-Board Multiplayer</h3>
              <p>Seamlessly connect with a real physical chess board — one player moves on the board, the other plays online in real time.</p>
            </div>
          </div>
        </div>
      </div>

      <div className="stats-section">
        <div className="stats-container">
          <div className="stat-item">
            <div className="stat-number">AI</div>
            <div className="stat-label">AI Engine</div>
          </div>
          <div className="stat-item">
            <div className="stat-number">3500+</div>
            <div className="stat-label">Max ELO Rating</div>
          </div>
          <div className="stat-item">
            <div className="stat-number">5</div>
            <div className="stat-label">Difficulty Levels</div>
          </div>
        </div>
      </div>

      <div className="footer">
        <p>Experience chess at its finest • No sound, pure strategy</p>
      </div>
    </div>
  );
}

export default HomePage;
