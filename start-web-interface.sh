#!/bin/bash

# AK Web Interface Startup Script
# This script starts the backend API and serves the web interface

set -e

echo "🚀 Starting AK Web Interface..."

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 is required but not installed."
    exit 1
fi

# Check if pip is available
if ! command -v pip3 &> /dev/null; then
    echo "❌ pip3 is required but not installed."
    exit 1
fi

# Install Python dependencies if they're not already installed
echo "📦 Installing Python dependencies..."
pip3 install -r requirements.txt

# Check if the AK CLI is available (optional)
if command -v ak &> /dev/null; then
    echo "✅ AK CLI found: $(ak --version 2>/dev/null || echo 'version unknown')"
else
    echo "⚠️  AK CLI not found in PATH. Web interface will still work but may show empty data initially."
fi

# Check GPG availability
if command -v gpg &> /dev/null; then
    echo "🔐 GPG available: $(gpg --version | head -n1)"
else
    echo "⚠️  GPG not found. Encrypted profiles will be stored as plain text."
fi

# Show config directory
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/ak"
echo "📁 AK config directory: $CONFIG_DIR"

# Create config directory if it doesn't exist
mkdir -p "$CONFIG_DIR"

# Start the backend API in the background
echo "🔧 Starting backend API server..."
python3 ak_backend_api.py &
API_PID=$!

# Wait a moment for the API to start
sleep 2

# Check if API is running
if ! curl -s http://localhost:5000/api/health > /dev/null; then
    echo "❌ Failed to start backend API"
    kill $API_PID 2>/dev/null || true
    exit 1
fi

echo "✅ Backend API started (PID: $API_PID)"

# Start the web server
echo "🌐 Starting web server..."
python3 serve-web-app.py &
WEB_PID=$!

# Wait a moment for the web server to start
sleep 2

echo "✅ Web server started (PID: $WEB_PID)"
echo ""
echo "🎉 AK Web Interface is now running!"
echo "   📱 Web Interface: http://localhost:8080/web-app.html"
echo "   🔌 Backend API:   http://localhost:5000/api/health"
echo ""
echo "📋 Available profiles and keys will be loaded from: $CONFIG_DIR"
echo "💡 Use Ctrl+C to stop both servers"
echo ""

# Function to cleanup on exit
cleanup() {
    echo ""
    echo "🛑 Shutting down servers..."
    kill $API_PID 2>/dev/null || true
    kill $WEB_PID 2>/dev/null || true
    echo "✅ Shutdown complete"
    exit 0
}

# Set up signal handlers
trap cleanup SIGINT SIGTERM

# Keep the script running
wait