#!/usr/bin/env python3
import sys

print("Content-Type: text/plain\n")
# Simulate a server error by raising an exception
raise Exception("Intentional CGI error for testing 500 Internal Server Error")
