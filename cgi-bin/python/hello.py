#!/usr/bin/env python3
import os
import urllib.parse

print("Content-Type: text/plain\n")

query = os.environ.get("QUERY_STRING", "")
params = urllib.parse.parse_qs(query)
name = params.get("name", [None])[0]
if name:
	print(name)
else:
	print("Hello, CGI")
