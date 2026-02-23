#!/usr/bin/env python3
"""
Slow CGI - Deliberately slow to test timeout
"""

import time
import sys
import os

print("Content-Type: text/html\r")
print("\r")
print("<html><body>")
print("<h1>Slow CGI Starting...</h1>")
sys.stdout.flush()

# Get sleep time from query string
sleep_time = int(os.environ.get('QUERY_STRING', '60'))

print(f"<p>Sleeping for {sleep_time} seconds...</p>")
sys.stdout.flush()

time.sleep(sleep_time)

print("<p>Done sleeping!</p>")
print("</body></html>")
