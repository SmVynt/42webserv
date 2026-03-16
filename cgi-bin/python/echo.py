#!/usr/bin/env python3
import sys
import os

print("Content-Type: text/plain\n")

if os.environ.get("REQUEST_METHOD", "") == "POST":
    try:
        length = int(os.environ.get("CONTENT_LENGTH", 0))
    except Exception:
        length = 0
    if length > 0:
        data = sys.stdin.read(length)
    else:
        data = ""
    print(f"You posted: {data}")
else:
    print("Send a POST request!")
