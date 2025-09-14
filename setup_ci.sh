#!/bin/bash

echo "Setting up CI/CD environment for FauxDB"
echo "======================================="

# Create necessary directories
mkdir -p .github/workflows
mkdir -p .circleci

echo "✓ Created GitHub Actions and CircleCI directories"

# Add .gitignore entries for CI artifacts
echo "" >> .gitignore
echo "# CI/CD artifacts" >> .gitignore
echo "coverage/" >> .gitignore
echo "*.coverage" >> .gitignore
echo "tarpaulin-report.html" >> .gitignore
echo "cobertura.xml" >> .gitignore

echo "✓ Updated .gitignore for CI artifacts"

# Make scripts executable
chmod +x setup_ci.sh

echo "✓ Made setup script executable"

echo ""
echo "CI/CD Setup Complete!"
echo "====================="
echo ""
echo "Files created:"
echo "  • .github/workflows/ci.yml - GitHub Actions workflow"
echo "  • .circleci/config.yml - CircleCI configuration"
echo "  • codecov.yml - Code coverage configuration"
echo ""
echo "Next steps:"
echo "  1. Push to GitHub repository"
echo "  2. Enable GitHub Actions in repository settings"
echo "  3. Connect repository to CircleCI"
echo "  4. Enable Codecov integration"
echo ""
echo "Badges will automatically appear in README once CI is running!"
