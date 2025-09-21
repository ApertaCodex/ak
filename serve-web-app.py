#!/usr/bin/env python3
"""
Simple HTTP server to serve the AK Web Application
Usage: python3 serve-web-app.py [port]
"""
import http.server
import socketserver
import sys
import os

def main():
    # Get port from command line or use default
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    
    # Set up server
    handler = http.server.SimpleHTTPRequestHandler
    
    # Add CORS headers for local development
    class CORSRequestHandler(handler):
        def end_headers(self):
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
            self.send_header('Access-Control-Allow-Headers', 'Content-Type')
            super().end_headers()
    
    try:
        with socketserver.TCPServer(("", port), CORSRequestHandler) as httpd:
            print(f"🌐 AK Web Application Server")
            print(f"📡 Serving at: http://localhost:{port}")
            print(f"🔗 Direct link: http://localhost:{port}/web-app.html")
            print(f"📁 Document root: {os.getcwd()}")
            print("\n" + "="*50)
            print("✨ Features:")
            print("  • Profile-based API key management")
            print("  • Real-time profile synchronization")
            print("  • Service CRUD operations")
            print("  • Responsive design")
            print("  • Local storage persistence")
            print("="*50)
            print("\nPress Ctrl+C to stop the server\n")
            
            httpd.serve_forever()
            
    except KeyboardInterrupt:
        print("\n\n🛑 Server stopped by user")
    except OSError as e:
        if e.errno == 98:  # Address already in use
            print(f"❌ Port {port} is already in use. Try a different port:")
            print(f"   python3 serve-web-app.py {port + 1}")
        else:
            print(f"❌ Error starting server: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()