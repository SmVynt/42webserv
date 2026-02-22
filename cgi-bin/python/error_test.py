#!/usr/bin/env python3
"""
CGI Error Test - Simulate various error conditions
"""

import sys
import os

error_type = os.environ.get('QUERY_STRING', 'none')

if error_type == 'crash':
    # Simulate crash
    sys.exit(1)
elif error_type == 'timeout':
    # Simulate timeout
    import time
    time.sleep(60)
elif error_type == 'stderr':
    # Write to stderr
    print("Content-Type: text/html\r")
    print("\r")
    print("<h1>Testing stderr</h1>")
    sys.stderr.write("This is an error message to stderr\n")
elif error_type == 'no_header':
    # Forget to print CGI header
    print("<html><body>Missing CGI header!</body></html>")
elif error_type == 'invalid_header':
    # Invalid header format
    print("Invalid-Header-Format")
    print("Content-Type: text/html")
    print("")
    print("<html><body>Invalid header</body></html>")
else:
    # Normal response
    print("Content-Type: text/html\r")
    print("\r")
    print("<html><body>")
    print("<h1>Error Test Menu</h1>")
    print("<ul>")
    print("<li><a href='?crash'>Simulate crash (exit 1)</a></li>")
    print("<li><a href='?timeout'>Simulate timeout (60s sleep)</a></li>")
    print("<li><a href='?stderr'>Write to stderr</a></li>")
    print("<li><a href='?no_header'>Missing CGI header</a></li>")
    print("<li><a href='?invalid_header'>Invalid header format</a></li>")
    print("</ul>")
    print("</body></html>")
