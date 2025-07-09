# Contributing to ChessWizzz

Thank you for your interest in contributing to ChessWizzz! This document provides guidelines and instructions for contributing to the project.

## 🚀 Getting Started

### Prerequisites
- Node.js (v16 or higher)
- npm or yarn
- Git
- Stockfish chess engine

### Setting up the Development Environment

1. Fork the repository on GitHub
2. Clone your fork locally:
```bash
git clone https://github.com/yourusername/chesswizzz.git
cd chesswizzz
```

3. Install dependencies for all parts of the project:
```bash
npm run install:all
```

4. Create a new branch for your feature:
```bash
git checkout -b feature/your-feature-name
```

## 🛠️ Development Workflow

### Running the Development Environment
```bash
# Run both frontend and backend together
npm run dev

# Or run them separately:
npm run dev:backend   # Backend only
npm run dev:frontend  # Frontend only
```

### Project Structure
- `backend/` - Express.js server with Stockfish integration
- `frontend/` - React + Vite application
- Root level contains project-wide configuration

## 🎯 Areas for Contribution

### High Priority
- [ ] **Multiplayer Support**: WebSocket implementation for real-time games
- [ ] **Mobile Optimization**: Responsive design improvements
- [ ] **Game Analysis**: Enhanced position evaluation display
- [ ] **User Accounts**: Registration and rating system
- [ ] **Opening Book**: Chess opening database integration

### Medium Priority
- [ ] **Chess Puzzles**: Training mode with tactical puzzles
- [ ] **Tournament Mode**: Multiple game tournaments
- [ ] **Advanced Statistics**: Detailed game analytics
- [ ] **Game Import/Export**: PGN file support
- [ ] **Theme Customization**: Multiple board themes

### Low Priority
- [ ] **Sound Effects**: Move sounds and notifications
- [ ] **Animations**: Smooth piece movement
- [ ] **Accessibility**: Screen reader support
- [ ] **Internationalization**: Multi-language support

## 📝 Code Style Guidelines

### Frontend (React)
- Use functional components with hooks
- Follow React best practices
- Use descriptive component and variable names
- Add PropTypes for component props
- Keep components focused and reusable

### Backend (Node.js)
- Use async/await for asynchronous operations
- Implement proper error handling
- Add input validation for API endpoints
- Use meaningful HTTP status codes
- Follow RESTful API design principles

### General
- Use 2 spaces for indentation
- Add comments for complex logic
- Write descriptive commit messages
- Keep functions small and focused

## 🧪 Testing

### Running Tests
```bash
# Run all tests
npm test

# Test specific parts
cd backend && npm test
cd frontend && npm test
```

### Writing Tests
- Add unit tests for new functions
- Include integration tests for API endpoints
- Test React components with proper mocking
- Ensure good test coverage

## 📖 Documentation

### Code Documentation
- Add JSDoc comments for functions
- Update README.md for new features
- Include inline comments for complex logic
- Document API changes

### User Documentation
- Update game instructions for new features
- Add screenshots for UI changes
- Document new configuration options

## 🔍 Pull Request Process

### Before Submitting
1. Ensure all tests pass
2. Update documentation as needed
3. Test your changes thoroughly
4. Follow the code style guidelines
5. Rebase your branch on the latest main

### PR Requirements
- **Clear Title**: Describe what the PR does
- **Detailed Description**: Explain the changes and why they're needed
- **Screenshots**: Include for UI changes
- **Breaking Changes**: Clearly document any breaking changes
- **Testing**: Describe how you tested the changes

### PR Template
```markdown
## Description
Brief description of the changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Manual testing completed

## Screenshots (if applicable)
[Add screenshots here]

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] Tests added/updated
```

## 🐛 Bug Reports

### Reporting Bugs
1. Check existing issues first
2. Use the bug report template
3. Include steps to reproduce
4. Add screenshots/videos if helpful
5. Specify your environment details

### Bug Report Template
```markdown
**Bug Description**
Clear description of the bug

**Steps to Reproduce**
1. Step 1
2. Step 2
3. Step 3

**Expected Behavior**
What should happen

**Actual Behavior**
What actually happens

**Environment**
- OS: [e.g., macOS 13.0]
- Browser: [e.g., Chrome 120]
- Node.js version: [e.g., 18.17.0]

**Screenshots**
[Add screenshots if applicable]
```

## 🌟 Feature Requests

### Requesting Features
1. Check if the feature already exists
2. Use the feature request template
3. Explain the use case clearly
4. Consider the complexity and impact

### Feature Request Template
```markdown
**Feature Description**
Clear description of the proposed feature

**Use Case**
Why is this feature needed?

**Proposed Solution**
How should it work?

**Alternatives Considered**
Other ways to solve this problem

**Additional Context**
Any other relevant information
```

## 🎖️ Recognition

Contributors will be:
- Listed in the README.md
- Mentioned in release notes
- Invited to join the core team (for significant contributions)

## 📞 Getting Help

- Create an issue for bugs or feature requests
- Start a discussion for questions or ideas
- Join our community discussions

## 📜 Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help others learn and grow
- Maintain professionalism

---

Thank you for contributing to ChessWizzz! 🏆
