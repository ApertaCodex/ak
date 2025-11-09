# Debug Mode Guide

This guide explains how to run AK in debug mode to see detailed logs and diagnostic information.

## CLI Commands (C++ Application)

### Testing with Debug Output

For testing commands, use the `--debug` flag to see detailed curl commands, HTTP status codes, and response snippets:

```bash
# Test a specific service with debug
./build/ak test ollama --debug

# Test all services with debug
./build/ak test --all --debug

# Test with profile and debug
./build/ak test -p default --debug
```

**Debug output includes:**
- Full curl command (with masked API keys)
- HTTP status codes
- Exit codes
- Response snippets (for failures or when debug is enabled)

### Environment Variables for Debug

You can also set environment variables to enable debug mode:

```bash
# Enable debug for tests (alternative to --debug flag)
export AK_DEBUG_TESTS=1
./build/ak test ollama

# Disable GPG (useful for debugging encryption issues)
export AK_DISABLE_GPG=1
./build/ak test

# Set GPG passphrase (for automated testing)
export AK_PASSPHRASE="your-passphrase"
./build/ak test
```

## Backend API (Python Flask Server)

### Enable Debug Logging

The backend API supports debug logging via the `AK_DEBUG` environment variable:

```bash
# Run with debug logging enabled
export AK_DEBUG=1
python3 ak_backend_api.py

# Or inline
AK_DEBUG=1 python3 ak_backend_api.py
```

**Debug output includes:**
- Timestamped log entries
- Log level (DEBUG, INFO, WARNING, ERROR)
- Detailed operation logs
- Error stack traces

### Flask Debug Mode

The Flask app already runs with `debug=True` by default (line 619), which enables:
- Auto-reload on code changes
- Detailed error pages
- Interactive debugger (if errors occur)

## GUI Application

### Enable Debug Logging

The GUI supports debug logging via environment variables. All logs will appear in the terminal/console where the GUI was launched:

```bash
# Enable GUI debug logging
export AK_DEBUG_GUI=1
./build/ak gui

# Or use the general debug flag (also enables GUI debug)
export AK_DEBUG=1
./build/ak gui

# Or inline
AK_DEBUG_GUI=1 ./build/ak gui
```

**Debug output includes:**
- GUI initialization messages
- Config directory and GPG status
- Qt debug messages (if enabled)
- Window creation and event loop messages
- Error messages with file locations

### What You'll See

With `AK_DEBUG_GUI=1`, you'll see output like:
```
[GUI] Debug mode enabled
[GUI] Config dir: /home/user/.config/ak
[GUI] GPG available: yes
[GUI] Creating main window...
[GUI] Main window shown, starting event loop...
[GUI] [DEBUG] [qt.qpa.xcb] QXcbConnection: XCB error...
```

### Additional Debug Options

```bash
# Enable debug for tests within GUI
export AK_DEBUG_TESTS=1
export AK_DEBUG_GUI=1
./build/ak gui

# Disable GPG for debugging encryption issues
export AK_DISABLE_GPG=1
export AK_DEBUG_GUI=1
./build/ak gui
```

### Viewing Logs

- **Console output**: All GUI logs appear in the terminal where you launched the GUI
- **Qt messages**: Debug, Warning, Critical, and Fatal messages are shown
- **Error details**: File and line numbers are included when debug is enabled

## Common Debug Scenarios

### Debugging Service Tests

```bash
# Test a specific service with full debug output
./build/ak test ollama --debug

# This shows:
# - curl command used
# - HTTP status code
# - Response body (if available)
# - Exit codes
```

### Debugging Backend API

```bash
# Start backend with debug logging
AK_DEBUG=1 python3 ak_backend_api.py

# Then make requests and watch the logs
curl http://localhost:5000/api/health
```

### Debugging Key Operations

```bash
# Test with debug to see what's happening
./build/ak test --debug

# Check audit log for key operations
./build/ak audit
```

## Log Locations

- **Audit Log**: `~/.config/ak/audit.log` (or `$XDG_CONFIG_HOME/ak/audit.log`)
- **Backend Logs**: Console output when `AK_DEBUG=1` is set
- **Test Debug Output**: Console output when `--debug` flag is used

## Examples

### Example 1: Debug a failing service test

```bash
./build/ak test ollama --debug
```

Output will show:
```
🧪 Testing Ollama API connection...
├── Finding API key... ✓
├── Connecting to API endpoint... ❌
└── Verifying authentication... ❌

❌ Ollama API: HTTP 405: 405 method not allowed
    curl: curl -sS --connect-timeout 5 --max-time 12 -X GET -H 'Authorization: Bearer [REDACTED]' http://localhost:11434/api/chat
    status: HTTP 405
    response:
      405 method not allowed
```

### Example 2: Debug backend API operations

```bash
# Terminal 1: Start backend with debug
AK_DEBUG=1 python3 ak_backend_api.py

# Terminal 2: Make API calls
curl http://localhost:5000/api/profiles/default/keys

# Terminal 1 will show detailed logs:
# 2024-01-01 12:00:00 - __main__ - DEBUG - Loading keys for profile: default
# 2024-01-01 12:00:00 - __main__ - DEBUG - Decrypting GPG file: /home/user/.config/ak/profiles/default.keys.gpg
# ...
```

### Example 3: Debug all test operations

```bash
export AK_DEBUG_TESTS=1
./build/ak test --all
```

This enables debug output for all tests without needing `--debug` on each test.

