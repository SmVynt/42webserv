#!/usr/bin/env python3
"""
CGI Upload Handler - Test file upload functionality
"""

import os
import sys

def print_header(content_type="text/html"):
    print(f"Content-Type: {content_type}\r")
    print("\r")

def main():
    print_header()

    content_length = os.environ.get('CONTENT_LENGTH', '0')
    request_method = os.environ.get('REQUEST_METHOD', 'GET')

    print("<!DOCTYPE html>")
    print("<html><head><title>Upload Handler</title></head><body>")
    print(f"<h1>Upload Handler</h1>")
    print(f"<p>Method: {request_method}</p>")
    print(f"<p>Content-Length: {content_length}</p>")

    if request_method == 'POST':
        try:
            length = int(content_length)
            data = sys.stdin.read(length)

            # Save to upload directory
            upload_dir = "/tmp/uploads"
            os.makedirs(upload_dir, exist_ok=True)

            filename = os.path.join(upload_dir, "uploaded_file.txt")
            with open(filename, 'w') as f:
                f.write(data)

            print(f"<p style='color: green;'>✅ File uploaded successfully!</p>")
            print(f"<p>Saved to: {filename}</p>")
            print(f"<p>Size: {len(data)} bytes</p>")
            print(f"<pre>{data[:500]}</pre>")
        except Exception as e:
            print(f"<p style='color: red;'>❌ Error: {e}</p>")
    else:
        print("<p>Use POST method to upload files</p>")

    print("</body></html>")

if __name__ == "__main__":
    main()
