# Git Workflow Guide

## Overview

This guide explains the Git workflow for contributing to the Summer/Autumn 3.0 project. Follow these practices to ensure clean, maintainable code and efficient collaboration.

## Fork Setup

### 1. Initial Fork Configuration

After forking the repository, set up your local development environment:

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/Summer.git
cd Summer

# Add upstream remote (original repository)
git remote add upstream https://github.com/ORIGINAL_OWNER/Summer.git

# Verify remotes
git remote -v
```

Expected output:

```
origin    https://github.com/YOUR_USERNAME/Summer.git (fetch)
origin    https://github.com/YOUR_USERNAME/Summer.git (push)
upstream  https://github.com/ORIGINAL_OWNER/Summer.git (fetch)
upstream  https://github.com/ORIGINAL_OWNER/Summer.git (push)
```

### 2. Keep Fork Synchronized

Before starting new work, always sync with upstream:

```bash
# Fetch latest changes from upstream
git fetch upstream

# Switch to main branch
git checkout main

# Merge upstream changes
git merge upstream/main

# Push updates to your fork
git push origin main
```

## Branch Strategy

### Branch Naming Conventions

Use descriptive branch names with prefixes:

- `docs/` - Documentation improvements
- `feature/` - New features
- `fix/` - Bug fixes
- `refactor/` - Code refactoring
- `test/` - Testing improvements
- `chore/` - Maintenance tasks

Examples:

```bash
git checkout -b docs/update-installation-guide
git checkout -b feature/audio-volume-control
git checkout -b fix/memory-leak-in-gsi
git checkout -b refactor/cleanup-installer-code
```

### Branch Lifecycle

1. **Create Feature Branch**: Always branch from latest `main`
2. **Work in Isolation**: Keep changes focused and atomic
3. **Regular Commits**: Commit early and often with descriptive messages
4. **Push to Fork**: Push feature branch to your fork
5. **Create Pull Request**: Open PR when ready for review
6. **Clean Up**: Delete feature branch after merge

## Commit Guidelines

### Commit Message Format

Follow conventional commit format:

```
type(scope): brief description

Detailed explanation of what was changed and why.
Include any breaking changes or important notes.

Fixes #123
Closes #456
```

### Commit Types

- `feat:` - New feature
- `fix:` - Bug fix
- `docs:` - Documentation changes
- `style:` - Code style changes (formatting, etc.)
- `refactor:` - Code refactoring
- `test:` - Adding or updating tests
- `chore:` - Maintenance tasks

### Good Commit Examples

```bash
# Feature addition
git commit -m "feat(audio): add volume control for bomb sounds

Implements adjustable volume levels for bomb countdown audio.
Adds slider control in settings UI and persistence in config.

Fixes #45"

# Bug fix
git commit -m "fix(gsi): resolve memory leak in MixChunkPtr

Properly manages SDL audio chunk lifecycle to prevent
use-after-free errors during bomb sound playback.

Closes #67"

# Documentation
git commit -m "docs: add comprehensive Git workflow guide

Includes fork setup, branch strategy, commit guidelines,
and pull request process for new contributors."
```

## Pull Request Process

### 1. Pre-PR Checklist

Before creating a pull request:

- [ ] Code compiles without errors
- [ ] All tests pass (if applicable)
- [ ] Code follows project style guidelines
- [ ] Documentation updated (if needed)
- [ ] Commit messages are clear and descriptive
- [ ] Branch is up-to-date with main

### 2. Creating the Pull Request

1. **Push to Fork**:

   ```bash
   git push origin feature/your-branch-name
   ```

2. **Open PR on GitHub**:
   - Navigate to your fork on GitHub
   - Click "Compare & pull request"
   - Fill out PR template completely
   - Add descriptive title and detailed description

### 3. PR Title and Description

**Good PR Title**:

```
feat(installer): add unified configuration installer system
```

**PR Description Template**:

```markdown
## Description

Brief summary of changes and motivation.

## Changes Made

- List specific changes
- Include file modifications
- Mention any breaking changes

## Testing

- How was this tested?
- What scenarios were covered?

## Screenshots (if applicable)

Include visual changes or new UI elements.

## Checklist

- [ ] Code compiles successfully
- [ ] Tests pass
- [ ] Documentation updated
- [ ] Breaking changes documented
```

## Code Review Process

### What to Expect

1. **Initial Review**: Maintainers will review within 48-72 hours
2. **Feedback**: Address comments and suggestions promptly
3. **Iteration**: Make requested changes in new commits
4. **Approval**: Once approved, PR will be merged
5. **Cleanup**: Delete feature branch after merge

### Responding to Feedback

```bash
# Make requested changes
# Add new commits (don't force push)
git add .
git commit -m "address review comments: improve error handling"
git push origin feature/your-branch-name

# If squashing is needed before merge
git rebase -i HEAD~n  # where n is number of commits
```

## Advanced Workflows

### Interactive Rebase

Clean up commit history before PR submission:

```bash
# Rebase last 3 commits
git rebase -i HEAD~3

# Options in interactive rebase:
# pick - keep commit as-is
# squash - combine with previous commit
# reword - change commit message
# drop - remove commit
```

### Conflict Resolution

When merge conflicts occur:

```bash
# Fetch latest upstream changes
git fetch upstream

# Rebase onto upstream/main
git rebase upstream/main

# Resolve conflicts in editor
# After resolving each file:
git add resolved-file.cpp

# Continue rebase
git rebase --continue

# Force push to update PR
git push --force-with-lease origin feature/your-branch-name
```

## Best Practices

### Development Workflow

1. **Start Small**: Make incremental, focused changes
2. **Test Thoroughly**: Verify changes don't break existing functionality
3. **Document Changes**: Update relevant documentation
4. **Follow Conventions**: Adhere to project coding standards
5. **Communicate**: Discuss major changes before implementation

### Git Hygiene

1. **Atomic Commits**: Each commit should represent one logical change
2. **Clear Messages**: Write commit messages for future maintainers
3. **Regular Sync**: Keep fork updated with upstream
4. **Branch Cleanup**: Delete merged branches promptly
5. **Avoid Force Push**: Use `--force-with-lease` when necessary

### Common Pitfalls

- **Large PRs**: Break big changes into smaller, reviewable chunks
- **Mixed Concerns**: Don't combine unrelated changes in one PR
- **Poor Messages**: Avoid vague commit messages like "fix stuff"
- **Outdated Branch**: Always rebase before submitting PR
- **Missing Tests**: Include tests for new functionality

## Troubleshooting

### Common Issues

**Fork is behind upstream**:

```bash
git fetch upstream
git checkout main
git merge upstream/main
git push origin main
```

**Accidentally committed to main**:

```bash
# Create new branch from current state
git checkout -b feature/my-changes

# Reset main to upstream
git checkout main
git reset --hard upstream/main
git push --force-with-lease origin main
```

**Need to update PR after review**:

```bash
# Make changes and commit
git add .
git commit -m "address review feedback"
git push origin feature/branch-name
```

For additional help, check the project's issue tracker or contact maintainers.