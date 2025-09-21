# AK HTTP Server Implementation

This document describes the new HTTP server implementation for AK, which replaces the Python backend with a native C++ server using cpp-httplib.

## Overview

The HTTP server exposes AK's functionality via REST API endpoints, allowing the web interface to communicate directly with the C++ backend without duplicating key management logic.

## Setup Instructions

### 1. Install cpp-httplib Header

The implementation requires the cpp-httplib single-header library:

```bash
# Method 1: Direct download (recommended)
wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -O third_party/httplib.h

# Method 2: Using package manager
# Ubuntu/Debian:
sudo apt install libhttplib-dev
# Then copy header to third_party/httplib.h

# macOS:
brew install cpp-httplib
# Then copy header to third_party/httplib.h
```

### 2. Build AK with HTTP Server Support

```bash
mkdir build && cd build
cmake ..
make
```

### 3. Start the HTTP Server

```bash
# Default configuration (127.0.0.1:8765)
./ak serve

# Custom host and port
./ak serve --host 0.0.0.0 --port 3000

# Show help
./ak serve --help
```

### 4. Use the Web Interface

1. Start the server: `./ak serve`
2. Open `web-app.html` in your browser
3. The web interface will automatically connect to the C++ server

## API Endpoints

### Health Check
- **GET** `/api/health` - Server status and version info

### Services Management
- **GET** `/api/services` - List all available services

### Profile Management
- **GET** `/api/profiles` - List all profiles
- **POST** `/api/profiles` - Create a new profile
- **DELETE** `/api/profiles/:name` - Delete a profile

### Key Management (Profile-specific)
- **GET** `/api/profiles/:name/keys` - Get keys in a profile
- **POST** `/api/profiles/:name/keys` - Add a key to a profile
- **PUT** `/api/profiles/:name/keys/:key` - Update a key in a profile
- **DELETE** `/api/profiles/:name/keys/:key` - Delete a key from a profile

### Key Management (Global)
- **GET** `/api/keys` - List all keys in global vault
- **POST** `/api/keys` - Add a key to global vault
- **PUT** `/api/keys/:key` - Update a key in global vault
- **DELETE** `/api/keys/:key` - Delete a key from global vault

## Request/Response Examples

### Create Profile
```bash
curl -X POST http://127.0.0.1:8765/api/profiles \
  -H "Content-Type: application/json" \
  -d '{"name": "production"}'
```

### Add Key to Profile
```bash
curl -X POST http://127.0.0.1:8765/api/profiles/production/keys \
  -H "Content-Type: application/json" \
  -d '{"name": "OPENAI_API_KEY", "value": "sk-..."}'
```

### Get Profile Keys
```bash
curl http://127.0.0.1:8765/api/profiles/production/keys
```

### Health Check
```bash
curl http://127.0.0.1:8765/api/health
```

## Features

### Security
- CORS headers for cross-origin requests
- Binds to localhost by default
- Input validation and sanitization
- Proper error handling

### Integration
- Uses existing AK storage functions (`storage::*`)
- Compatible with GPG encryption
- Maintains profile-specific key storage
- Backward compatibility with global vault

### Web Interface
- Automatic connection to C++ server
- Real-time updates
- Service detection
- Key masking/revealing
- Profile switching

## Architecture

### Files Added/Modified

**New Files:**
- `include/http/server.hpp` - HTTP server class definition
- `src/http/server.cpp` - HTTP server implementation
- `third_party/httplib.h` - cpp-httplib header (placeholder)

**Modified Files:**
- `include/commands/commands.hpp` - Added `cmd_serve` declaration
- `src/commands/commands.cpp` - Implemented `cmd_serve` function
- `src/main.cpp` - Added "serve" to command dispatch table
- `src/cli/cli.cpp` - Added serve command to help text
- `CMakeLists.txt` - Added HTTP server files and third_party include
- `web-app.js` - Updated to use C++ server endpoints

### Class Structure

```cpp
namespace ak::http {
    class Server {
        Server(const core::Config& cfg);
        bool start(const std::string& host = "127.0.0.1", uint16_t port = 8765);
        void stop();
        bool isRunning() const;
    };
}
```

## Testing

### Manual Testing
1. Start server: `./ak serve`
2. Test health endpoint: `curl http://127.0.0.1:8765/api/health`
3. Open web interface: Open `web-app.html` in browser
4. Create profiles and keys through the web UI
5. Verify data persistence with CLI: `./ak profiles`, `./ak ls`

### Integration Testing
1. Add keys via CLI: `./ak add TEST_KEY test_value`
2. Start server: `./ak serve`
3. Check key appears in web interface
4. Add key via web interface
5. Verify with CLI: `./ak get TEST_KEY`

## Troubleshooting

### Compilation Errors
- Ensure `third_party/httplib.h` contains the actual cpp-httplib header
- Check that C++17 standard is enabled in CMakeLists.txt

### Server Won't Start
- Check if port is already in use: `lsof -i :8765`
- Try different port: `./ak serve --port 3000`
- Check firewall settings

### Web Interface Connection Issues
- Verify server is running on correct port
- Check browser console for CORS errors
- Ensure `web-app.js` has correct `apiBaseUrl`

### Data Sync Issues
- Restart server after making changes via CLI
- Check that profile exists: `./ak profiles`
- Verify GPG is working: `./ak backend`

## Migration from Python Backend

The new C++ server replaces the Python backend (`ak_backend_api.py` and `serve-web-app.py`). Key changes:

1. **Port Change**: Default port changed from 5000 to 8765
2. **Response Format**: Responses now follow C++ server format
3. **Direct Integration**: No separate process needed - integrated into AK binary
4. **Better Performance**: Native C++ implementation
5. **Consistent Data**: Uses same storage functions as CLI

## Command Line Options

```bash
ak serve [OPTIONS]

OPTIONS:
  --host, -h HOST    Host to bind to (default: 127.0.0.1)
  --port, -p PORT    Port to listen on (default: 8765)  
  --help             Show help message

EXAMPLES:
  ak serve                     # Start on 127.0.0.1:8765
  ak serve --port 3000         # Start on port 3000
  ak serve --host 0.0.0.0      # Bind to all interfaces
```

The server will run until stopped with Ctrl+C.