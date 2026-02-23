#!/usr/bin/env python3
"""
Simple CGI test script - validates basic CGI workflow
"""

import sys
import os

# CGI Response headers
print("Content-Type: text/html\r")
print("\r")

# HTML output
print("<!DOCTYPE html>")
print("<html><head><title>Python CGI Test</title></head><body>")
print("<h1>Python CGI Test - Success!</h1>")

# Request method
method = os.environ.get('REQUEST_METHOD', 'UNKNOWN')
print(f"<p><strong>Request Method:</strong> {method}</p>")

# Query string
query = os.environ.get('QUERY_STRING', '')
if query:
    print(f"<p><strong>Query String:</strong> {query}</p>")
    # Parse query parameters
    print("<ul>")
    for param in query.split('&'):
        if '=' in param:
            key, value = param.split('=', 1)
            print(f"<li>{key} = {value}</li>")
    print("</ul>")

# POST data
content_length = os.environ.get('CONTENT_LENGTH', '')
if content_length and int(content_length) > 0:
    post_data = sys.stdin.read(int(content_length))
    print(f"<p><strong>POST Data:</strong> {post_data}</p>")
    # Parse POST parameters
    print("<ul>")
    for param in post_data.split('&'):
        if '=' in param:
            key, value = param.split('=', 1)
            print(f"<li>{key} = {value}</li>")
    print("</ul>")

# Server info
print("<h2>Server Information:</h2>")
print("<ul>")
print(f"<li>Server: {os.environ.get('SERVER_SOFTWARE', 'N/A')}</li>")
print(f"<li>Gateway: {os.environ.get('GATEWAY_INTERFACE', 'N/A')}</li>")
print(f"<li>Protocol: {os.environ.get('SERVER_PROTOCOL', 'N/A')}</li>")
print("</ul>")

print("</body></html>")
