#!/bin/bash

# NGINX Setup Script
# Creates an NGINX configuration matching webserv setup for comparison testing

set -euo pipefail

# Configuration
NGINX_PORT="${NGINX_PORT:-8081}"
NGINX_CONF_DIR="/tmp/nginx_test"
PROJECT_ROOT="$(cd ../.. && pwd)"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "=========================================="
echo "NGINX Configuration Setup"
echo "=========================================="

# Check if nginx is installed
if ! command -v nginx &>/dev/null; then
    echo -e "${RED}[ERROR]${NC} nginx not installed"
    echo "Install with: sudo apt-get install nginx"
    exit 1
fi

# Create nginx config directory
mkdir -p "$NGINX_CONF_DIR"
mkdir -p "$NGINX_CONF_DIR/logs"

echo "Creating NGINX configuration..."

# Create nginx.conf matching webserv setup
cat > "$NGINX_CONF_DIR/nginx.conf" << EOF
# NGINX Configuration for WebServ Comparison
# Generated automatically - matches webserv config as closely as possible

daemon off;
worker_processes 1;
error_log $NGINX_CONF_DIR/logs/error.log;
pid $NGINX_CONF_DIR/nginx.pid;

events {
    worker_connections 1024;
    use epoll;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    access_log $NGINX_CONF_DIR/logs/access.log;

    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    keepalive_timeout 65;
    types_hash_max_size 2048;

    client_max_body_size 100M;
    client_body_buffer_size 128k;
    client_header_buffer_size 1k;
    large_client_header_buffers 4 8k;

    server {
        listen $NGINX_PORT;
        server_name localhost;

        root $PROJECT_ROOT/www;
        index index.html index.htm;

        # Access log
        access_log $NGINX_CONF_DIR/logs/access.log;
        error_log $NGINX_CONF_DIR/logs/error.log;

        # Default location
        location / {
            try_files \$uri \$uri/ =404;
            autoindex on;
        }

        # Static files
        location ~* \.(html|css|js|jpg|jpeg|png|gif|ico|svg|woff|woff2|ttf|eot)$ {
            expires 30d;
            add_header Cache-Control "public, immutable";
        }

        # CGI scripts
        location /cgi-bin/ {
            fastcgi_pass unix:/var/run/fcgiwrap.socket;
            include fastcgi_params;
            fastcgi_param SCRIPT_FILENAME $PROJECT_ROOT\$fastcgi_script_name;
        }

        # Upload endpoint
        location /upload {
            client_max_body_size 100M;

            # Simple upload handling (requires additional setup)
            # For basic comparison, this will return 404 unless configured
            return 404;
        }

        # Custom error pages
        error_page 404 /error/404.html;
        error_page 500 502 503 504 /error/500.html;

        location = /error/404.html {
            root $PROJECT_ROOT/www;
            internal;
        }

        location = /error/500.html {
            root $PROJECT_ROOT/www;
            internal;
        }

        # Uploads directory
        location /uploads/ {
            alias $PROJECT_ROOT/www/uploads/;
            autoindex on;
        }
    }
}
EOF

echo -e "${GREEN}✓${NC} Configuration file created"
echo "  Location: $NGINX_CONF_DIR/nginx.conf"
echo "  Port: $NGINX_PORT"
echo "  Document root: $PROJECT_ROOT/www"

# Test configuration
echo ""
echo "Testing NGINX configuration..."
if nginx -t -c "$NGINX_CONF_DIR/nginx.conf" 2>&1 | grep -q "successful"; then
    echo -e "${GREEN}✓${NC} Configuration test passed"
else
    echo -e "${RED}✗${NC} Configuration test failed"
    nginx -t -c "$NGINX_CONF_DIR/nginx.conf"
    exit 1
fi

echo ""
echo "=========================================="
echo "Setup Complete"
echo "=========================================="
echo ""
echo "To start NGINX:"
echo "  nginx -c $NGINX_CONF_DIR/nginx.conf"
echo ""
echo "To stop NGINX:"
echo "  nginx -s stop -c $NGINX_CONF_DIR/nginx.conf"
echo ""
echo "To test both servers:"
echo "  WebServ: http://localhost:8080"
echo "  NGINX:   http://localhost:$NGINX_PORT"
echo ""
echo "Logs located at:"
echo "  $NGINX_CONF_DIR/logs/"

exit 0
