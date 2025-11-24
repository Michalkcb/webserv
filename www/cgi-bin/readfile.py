#!/usr/bin/env python3
print("Content-Type: text/plain\r\n\r\n")

# Try to open a file using a relative path
with open("data.txt", "r") as f:
    print(f.read())
