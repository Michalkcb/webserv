#!/usr/bin/env python3
import os, sys

print("Content-Type: text/plain")
print()
print("REQUEST_METHOD:", os.environ.get("REQUEST_METHOD"))
print("CONTENT_TYPE:", os.environ.get("CONTENT_TYPE"))
print("CONTENT_LENGTH:", os.environ.get("CONTENT_LENGTH"))
print("--- stdin start ---")
data = sys.stdin.read()
print(data)
print("--- stdin end ---")