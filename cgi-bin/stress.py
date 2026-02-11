#!/usr/bin/env python3
"""
CGI Stress Test - Generate large output to test buffering
"""

import sys
import os

def print_header():
    print("Content-Type: text/html\r")
    print("\r")

def main():
    print_header()

    size = os.environ.get('QUERY_STRING', '1000')
    try:
        num_lines = int(size)
    except ValueError:
        num_lines = 1000

    print("<!DOCTYPE html>")
    print("<html><head><title>Stress Test</title></head><body>")
    print(f"<h1>Generating {num_lines} lines of output</h1>")
    print("<ul>")

    for i in range(num_lines):
        print(f"<li>Line {i+1}: This is test data to stress the CGI buffer handling</li>")

    print("</ul>")
    print(f"<p>Total lines generated: {num_lines}</p>")
    print("</body></html>")

if __name__ == "__main__":
    main()
