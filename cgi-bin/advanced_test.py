#!/usr/bin/env python3

import os
import sys
import urllib.parse

def print_header(content_type="text/html"):
    """Print CGI header"""
    print(f"Content-Type: {content_type}\r")
    print("\r")

def parse_query_string():
    """Parse QUERY_STRING from environment"""
    query_string = os.environ.get('QUERY_STRING', '')
    if query_string:
        return dict(urllib.parse.parse_qsl(query_string))
    return {}

def read_post_data():
    """Read POST data from stdin"""
    content_length = os.environ.get('CONTENT_LENGTH', '0')
    try:
        length = int(content_length)
        if length > 0:
            post_data = sys.stdin.read(length)
            return dict(urllib.parse.parse_qsl(post_data))
    except ValueError:
        pass
    return {}

def main():
    print_header()

    print("<!DOCTYPE html>")
    print("<html lang='en'>")
    print("<head><meta charset='UTF-8'><title>CGI Test Results</title>")
    print("<style>")
    print("body { font-family: Arial, sans-serif; margin: 40px; }")
    print("h1 { color: #2c3e50; }")
    print("table { border-collapse: collapse; width: 100%; margin: 20px 0; }")
    print("th, td { border: 1px solid #ddd; padding: 12px; text-align: left; }")
    print("th { background-color: #3498db; color: white; }")
    print("tr:nth-child(even) { background-color: #f2f2f2; }")
    print(".success { color: #27ae60; }")
    print(".info { background-color: #e8f4f8; padding: 15px; margin: 10px 0; }")
    print("</style>")
    print("</head>")
    print("<body>")

    print("<h1>✅ CGI Script Executed Successfully!</h1>")

    request_method = os.environ.get('REQUEST_METHOD', 'UNKNOWN')
    print(f"<p class='info'><strong>Request Method:</strong> {request_method}</p>")

    # Display query parameters (GET)
    query_params = parse_query_string()
    if query_params:
        print("<h2>Query String Parameters (GET)</h2>")
        print("<table>")
        print("<tr><th>Key</th><th>Value</th></tr>")
        for key, value in query_params.items():
            print(f"<tr><td>{key}</td><td>{value}</td></tr>")
        print("</table>")

    # Display POST data
    if request_method == 'POST':
        post_params = read_post_data()
        if post_params:
            print("<h2>POST Data Parameters</h2>")
            print("<table>")
            print("<tr><th>Key</th><th>Value</th></tr>")
            for key, value in post_params.items():
                print(f"<tr><td>{key}</td><td>{value}</td></tr>")
            print("</table>")

    # Display all environment variables
    print("<h2>CGI Environment Variables</h2>")
    print("<table>")
    print("<tr><th>Variable</th><th>Value</th></tr>")

    important_vars = [
        'GATEWAY_INTERFACE', 'SERVER_PROTOCOL', 'SERVER_SOFTWARE',
        'SERVER_NAME', 'SERVER_PORT', 'REQUEST_METHOD',
        'PATH_INFO', 'PATH_TRANSLATED', 'SCRIPT_NAME',
        'QUERY_STRING', 'REMOTE_ADDR', 'REMOTE_HOST',
        'CONTENT_TYPE', 'CONTENT_LENGTH', 'HTTP_USER_AGENT',
        'HTTP_ACCEPT', 'HTTP_HOST', 'HTTP_COOKIE'
    ]

    for var in important_vars:
        value = os.environ.get(var, '<em>not set</em>')
        print(f"<tr><td><strong>{var}</strong></td><td>{value}</td></tr>")

    print("</table>")

    print("<h2>All Environment Variables</h2>")
    print("<table>")
    print("<tr><th>Variable</th><th>Value</th></tr>")
    for key in sorted(os.environ.keys()):
        value = os.environ[key]
        print(f"<tr><td>{key}</td><td>{value}</td></tr>")
    print("</table>")

    print("</body>")
    print("</html>")

if __name__ == "__main__":
    main()
