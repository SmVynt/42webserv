#!/usr/bin/env python3
"""
Infinite CGI - Never finishes (for timeout testing)
"""

import time
import sys

print("Content-Type: text/html\r")
print("\r")
print("<html><body>")
print("<h1>Infinite CGI - Looping Forever</h1>")
sys.stdout.flush()

counter = 0
while True:
    counter += 1
    print(f"<p>Iteration {counter}</p>")
    sys.stdout.flush()
    time.sleep(1)
