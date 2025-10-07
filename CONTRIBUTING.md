# Contributing to ToyDFS

Thank you for your interest in contributing to ToyDFS! This document provides guidelines for contributing to this distributed file system project.

## 🚀 Quick Start

1. **Fork** the repository on GitHub
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/your-username/toydfs.git
   cd toydfs
   ```
3. **Set up** the development environment:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```
4. **Run tests** to ensure everything works:
   ```bash
   make check
   ```

## 📝 Development Workflow

### Branch Strategy

We follow a simplified GitHub Flow:

- **`main`** - Production-ready code, always deployable
- **`feature/*`** - New features or enhancements
- **`bugfix/*`** - Bug fixes
- **`docs/*`** - Documentation updates

### Making Changes

1. **Create a feature branch** from `main`:
   ```bash
   git checkout -b feature/amazing-feature
   ```

2. **Make your changes** following our coding standards

3. **Test your changes**:
   ```bash
   make check        # Run all tests
   make coverage     # Generate coverage report (if enabled)
   ```

4. **Format your code**:
   ```bash
   # clang-format will be run automatically by pre-commit hooks
   # But you can run it manually if needed:
   clang-format -i path/to/your/file.cpp
   ```

5. **Commit your changes**:
   ```bash
   git add .
   git commit -m "Add amazing feature

   - Describe what the feature does
   - Mention any breaking changes
   - Reference related issues"
   ```

6. **Push to your fork**:
   ```bash
   git push origin feature/amazing-feature
   ```

7. **Create a Pull Request** on GitHub

## 🧪 Testing

### Running Tests

```bash
# Run all tests
make check

# Run specific test
./build/test/test_metadata_store

# Run with verbose output
make check_verbose

# Generate coverage report (requires LLVM tools)
make coverage
```

### Writing Tests

- **Integration tests** are preferred over unit tests for this project
- Tests should be in `test/src/` directory
- Follow the naming convention: `test_<component>_service.cpp`
- Use descriptive test names that clearly indicate what behavior is being tested
- Group related tests within the same test file

## 📋 Code Quality

### Code Formatting

- **clang-format** is used for code formatting
- Configuration is in `.clang-format` (Google style with 4-space indentation)
- Pre-commit hooks automatically format C++ files

### Static Analysis

- **clang-tidy** is used for static analysis
- Configuration is in `.clang-tidy` (comprehensive checks enabled)
- Pre-push hooks run clang-tidy on modified files
- All warnings are treated as errors

### Compiler Warnings

The project uses strict compiler warnings:
- `-Wall -Wextra -Werror` are enabled
- Additional valuable warnings like `-Wuninitialized`, `-Wreturn-type`
- System headers are excluded from warnings

## 📚 Documentation

### README Updates

The project follows strict README maintenance guidelines. Any changes that affect:

- **Build system** (CMakeLists.txt modifications)
- **Public APIs** (header file changes)
- **Dependencies** (new libraries or tools)
- **Platform support** (OS-specific fixes)
- **Project structure** (new components or directories)
- **Architecture** (system design changes)

**Must** be reflected in the README.md file.

### Documentation Standards

- Keep all code examples current and working
- Update version numbers when dependencies change
- Document platform-specific workarounds
- Use clear, concise language with practical examples

## 🔄 Pull Request Process

1. **Ensure tests pass** - All CI checks must pass
2. **Update documentation** - README and other docs as needed
3. **Follow commit message format** - Clear, descriptive messages
4. **Request review** - At least one maintainer review required
5. **Address feedback** - Respond to review comments promptly
6. **Merge** - Maintainers will merge approved PRs

## 🚨 Code Standards

### C++ Best Practices

- **C++17** standard with modern features
- **RAII** resource management
- **Smart pointers** for memory safety
- **const-correctness** where applicable
- **Meaningful variable names** (no single letters except in loops)
- **Avoid magic numbers** (use named constants)

### Architecture Guidelines

- **Component responsibilities** should be clear and focused
- **Interface changes** require API reference updates
- **Error handling** should be graceful and informative
- **Thread safety** considerations for concurrent operations

## 🐛 Reporting Issues

1. **Check existing issues** - Search for similar problems first
2. **Create a clear title** - Describe the issue concisely
3. **Provide detailed information**:
   - Steps to reproduce
   - Expected vs actual behavior
   - Environment details (OS, compiler, versions)
   - Error messages and stack traces
4. **Include minimal reproduction** if possible

## 💬 Getting Help

- **Issues** - Use GitHub issues for bugs and feature requests
- **Discussions** - Use GitHub discussions for questions and ideas
- **Code Review** - All submissions require review

## 📜 License

This project is provided as an educational example. See individual source files for any specific licensing terms.

---

Thank you for contributing to ToyDFS! 🎉
