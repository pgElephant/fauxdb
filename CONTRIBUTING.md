# Contributing to FauxDB

Thank you for your interest in contributing to FauxDB! This document provides guidelines and information for contributors.

## Development Setup

### Prerequisites

- Rust 1.70 or later
- PostgreSQL 17 or later
- Git

### Getting Started

1. **Fork the repository**
   ```bash
   git clone https://github.com/your-username/fauxdb.git
   cd fauxdb
   ```

2. **Install dependencies**
   ```bash
   cargo build
   ```

3. **Set up PostgreSQL**
   ```bash
   # Create test database
   createdb fauxdb_test
   ```

4. **Run tests**
   ```bash
   cargo test --test fauxdb_tests
   ```

## Code Style

### Rust Formatting

We use `rustfmt` for consistent code formatting:

```bash
cargo fmt
```

### Linting

We use `clippy` for additional linting:

```bash
cargo clippy -- -D warnings
```

### Testing

All code must be tested:

```bash
# Run all tests
cargo test

# Run specific test suite
cargo test --test fauxdb_tests test_crud_operations

# Run with coverage
cargo tarpaulin --out Html
```

## Pull Request Process

1. **Create a feature branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes**
   - Write tests for new functionality
   - Update documentation as needed
   - Ensure all tests pass

3. **Submit a pull request**
   - Provide a clear description of changes
   - Reference any related issues
   - Ensure CI passes

## Testing Guidelines

### MongoDB Compatibility Testing

All new features must be tested with `mongosh`:

```bash
# Test with real MongoDB client
mongosh mongodb://localhost:27018/test --eval "db.runCommand({yourCommand: 1})"
```

### Test Categories

- **Unit Tests**: Test individual functions and modules
- **Integration Tests**: Test component interactions
- **Compatibility Tests**: Test MongoDB wire protocol compatibility

## Code Coverage

We maintain high code coverage. New code should include tests:

```bash
# Check current coverage
cargo tarpaulin --out Html
open tarpaulin-report.html
```

## Documentation

### Code Documentation

- Use Rust doc comments for public APIs
- Include examples in documentation
- Document error conditions

### README Updates

- Update README.md for new features
- Keep examples current and working

## Issue Reporting

When reporting issues:

1. **Check existing issues** first
2. **Provide reproduction steps**
3. **Include system information**
4. **Add relevant logs**

## Release Process

1. **Update version** in Cargo.toml
2. **Update CHANGELOG.md**
3. **Create release tag**
4. **Build and test release**

## Community Guidelines

- Be respectful and constructive
- Help others learn and contribute
- Follow the code of conduct
- Ask questions if unsure

## Getting Help

- **GitHub Issues**: For bugs and feature requests
- **GitHub Discussions**: For questions and ideas
- **Documentation**: Check README and inline docs

Thank you for contributing to FauxDB!
