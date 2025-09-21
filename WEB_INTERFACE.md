# AK Web Interface Documentation

## Overview

The AK Web Interface provides a modern, browser-based interface for managing API keys, profiles, and services. It connects to the same data files used by the AK CLI, ensuring consistency between the command-line and web interfaces.

## Architecture

### Backend API (`ak_backend_api.py`)
- **Python Flask REST API** that reads from the same data files as the AK CLI
- **Real-time data synchronization** with CLI operations
- **GPG support** for encrypted profile storage
- **CORS-enabled** for web interface integration

### Frontend (`web-app.html` + `web-app.js`)
- **Modern HTML5/CSS3** interface with Tailwind CSS styling
- **Responsive design** that works on desktop, tablet, and mobile
- **Real-time updates** via REST API calls
- **Profile synchronization** across all interface components

## Quick Start

### Method 1: Auto Setup (Recommended)
```bash
# Start both backend API and web server
./start-web-interface.sh

# Open web interface
open http://localhost:8080/web-app.html
```

### Method 2: Manual Setup
```bash
# Install dependencies
pip3 install -r requirements.txt

# Start backend API (terminal 1)
python3 ak_backend_api.py

# Start web server (terminal 2)
python3 serve-web-app.py

# Access web interface
open http://localhost:8080/web-app.html
```

## Data Integration

### CLI Data Compatibility
The web interface reads from the same data files as the AK CLI:

- **Profile files**: `~/.config/ak/profiles/{name}.profile`
- **Profile keys**: `~/.config/ak/profiles/{name}.keys.gpg` (encrypted) or `{name}.keys` (plain)
- **Configuration**: Uses XDG Base Directory Specification

### Real-time Synchronization
- Changes made via CLI are immediately visible in web interface after refresh
- Changes made in web interface are immediately available to CLI
- No data duplication or synchronization issues

### GPG Encryption Support
- Automatic GPG encryption/decryption for profile key storage
- Falls back to plain text if GPG is unavailable
- Respects `AK_DISABLE_GPG` and `AK_PASSPHRASE` environment variables

## Features

### 🔐 Profile-Based Key Management
- **Multi-profile support**: Create, switch between, and manage multiple profiles
- **Default profile logic**: New keys automatically go to selected profile
- **Profile isolation**: Keys are stored per-profile with no cross-contamination
- **Profile indicators**: Visual distinction between default and custom profiles

### 🗝️ Advanced Key Management
- **Full CRUD operations**: Create, read, update, and delete API keys
- **Service auto-detection**: Automatically detect service based on key name patterns
- **Multiple keys per service**: Support `OPENAI_API_KEY`, `OPENAI_API_KEY_2`, etc.
- **Key masking/visibility**: Toggle between masked and full key values
- **One-click copy**: Copy keys to clipboard with single click
- **Search and filter**: Real-time key filtering by name or service
- **Key testing**: Simulate API key validation (UI placeholder)

### ⚙️ Service Management
- **Built-in services**: 10+ pre-configured services (OpenAI, Anthropic, GitHub, etc.)
- **Service patterns**: Regex patterns for automatic key-to-service detection
- **Service information**: API URLs and key format documentation
- **Visual indicators**: Clear distinction between built-in and custom services

### 🎨 Modern User Interface
- **Responsive design**: Optimized for desktop, tablet, and mobile devices
- **Tabbed interface**: Clean separation between Key Manager, Profile Manager, and Service Manager
- **Modal dialogs**: Intuitive forms for adding/editing keys, profiles, and services
- **Toast notifications**: Real-time feedback for user actions
- **Professional styling**: Clean, modern design with Tailwind CSS
- **Accessibility**: Keyboard navigation and screen reader support

### 🔄 Data Operations
- **Real-time refresh**: Manual refresh button to sync with CLI changes
- **Error handling**: Graceful error messages and recovery
- **Loading states**: Visual feedback during API operations
- **Data validation**: Client and server-side validation for all inputs

## API Endpoints

### Profile Management
```http
GET    /api/profiles                    # List all profiles
POST   /api/profiles                    # Create new profile
DELETE /api/profiles/{profile_name}     # Delete profile
```

### Key Management
```http
GET    /api/profiles/{profile_name}/keys           # Get profile keys
POST   /api/profiles/{profile_name}/keys           # Add key to profile
PUT    /api/profiles/{profile_name}/keys/{key_name}  # Update key
DELETE /api/profiles/{profile_name}/keys/{key_name}  # Delete key
```

### Service Management
```http
GET    /api/services                    # List all services
```

### System
```http
GET    /api/health                      # Health check
```

## Configuration

### Environment Variables
- `XDG_CONFIG_HOME`: Config directory override (default: `~/.config`)
- `AK_DISABLE_GPG`: Set to "1" to disable GPG encryption
- `AK_PASSPHRASE`: Preset GPG passphrase for automation

### Directory Structure
```
~/.config/ak/
├── profiles/
│   ├── default.profile          # Profile key list
│   ├── default.keys.gpg         # Encrypted key storage
│   ├── prod.profile
│   ├── prod.keys.gpg
│   └── test.profile
├── persist/                     # Persistence data
└── instance_id                  # Unique instance identifier
```

### Built-in Services
The interface includes these pre-configured services:

| Service | Pattern | API URL |
|---------|---------|---------|
| OpenAI | `^OPENAI_.*` | https://api.openai.com/v1 |
| Anthropic | `^ANTHROPIC_.*` | https://api.anthropic.com/v1 |
| GitHub | `^GITHUB_.*` | https://api.github.com |
| GitLab | `^GITLAB_.*` | https://gitlab.com/api/v4 |
| Google | `^GOOGLE_.*` | https://www.googleapis.com |
| AWS | `^AWS_.*` | https://aws.amazon.com |
| Azure | `^AZURE_.*` | https://management.azure.com |
| Stripe | `^STRIPE_.*` | https://api.stripe.com/v1 |
| Twilio | `^TWILIO_.*` | https://api.twilio.com |
| SendGrid | `^SENDGRID_.*` | https://api.sendgrid.com/v3 |

## Browser Compatibility

### Minimum Requirements
- **Chrome 70+** (October 2018)
- **Firefox 65+** (January 2019)
- **Safari 12+** (September 2018)
- **Edge 79+** (January 2020)

### Required Features
- ES6+ JavaScript support
- Fetch API
- Local Storage (for temporary UI state only)
- CSS Grid and Flexbox

## Security Considerations

### Data Storage
- **No sensitive data in browser**: Keys are only loaded temporarily for display
- **Server-side encryption**: GPG encryption handled by backend API
- **Secure communication**: API calls over localhost (consider HTTPS for remote access)
- **Clipboard security**: Copy operations use secure clipboard API

### Network Security
- **CORS protection**: API configured with appropriate CORS headers
- **Local-only by default**: Both API and web server bind to localhost
- **No external dependencies**: All resources served locally

## Troubleshooting

### Common Issues

**"Failed to connect to AK backend"**
```bash
# Check if API server is running
curl http://localhost:5000/api/health

# Start API server manually
python3 ak_backend_api.py
```

**"GPG decryption failed"**
```bash
# Check GPG availability
gpg --version

# Test GPG key decryption
gpg --decrypt ~/.config/ak/profiles/default.keys.gpg

# Disable GPG temporarily
export AK_DISABLE_GPG=1
```

**"Empty profile list"**
```bash
# Check config directory
ls -la ~/.config/ak/profiles/

# Create default profile manually
echo "# Empty profile" > ~/.config/ak/profiles/default.profile
```

### Development Mode

For development with auto-reload:
```bash
# API server with debug mode
FLASK_ENV=development python3 ak_backend_api.py

# Web server with reload
python3 serve-web-app.py --reload
```

## Integration with AK CLI

### Seamless Operation
- **Shared data files**: Both interfaces use identical storage format
- **Immediate synchronization**: Changes are visible after refresh
- **Compatible operations**: All CLI operations work with web-created data
- **Profile consistency**: Profile names and structure match exactly

### Example Workflow
```bash
# Create profile via CLI
ak profile create production

# Add key via CLI  
ak key add OPENAI_API_KEY sk-... --profile production

# Switch to web interface
./start-web-interface.sh
# Keys and profiles immediately visible

# Add more keys via web interface
# Immediately available via CLI:
ak key list --profile production
```

## Deployment Options

### Local Development
```bash
./start-web-interface.sh
```

### Docker Deployment
```dockerfile
FROM python:3.9-slim
COPY . /app
WORKDIR /app
RUN pip install -r requirements.txt
EXPOSE 5000 8080
CMD ["./start-web-interface.sh"]
```

### Reverse Proxy Setup
```nginx
server {
    listen 80;
    server_name ak.example.com;
    
    location /api/ {
        proxy_pass http://localhost:5000/api/;
    }
    
    location / {
        proxy_pass http://localhost:8080/;
    }
}
```

## Future Enhancements

### Planned Features
- **Real API key testing**: Actual validation against service endpoints
- **Bulk operations**: Import/export multiple keys at once
- **Key templates**: Predefined key structures for common services
- **Audit logging**: Track all key operations with timestamps
- **Team collaboration**: Multi-user profile sharing
- **Dark mode**: Alternative UI theme
- **Keyboard shortcuts**: Power user navigation
- **WebSocket updates**: Real-time synchronization without refresh

### Extension Points
- **Plugin system**: Custom service definitions
- **Theme system**: Customizable UI appearance  
- **Export formats**: JSON, YAML, environment files
- **Integration hooks**: Webhooks for key operations
- **Monitoring**: Key usage statistics and alerts

---

*The AK Web Interface provides a modern, accessible way to manage API keys while maintaining full compatibility with the AK CLI. It bridges the gap between command-line efficiency and graphical convenience.*