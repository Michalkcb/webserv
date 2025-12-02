#!/usr/bin/env python3
print("Content-Type: text/html\r\n\r\n")

print("<html>")
print("<head><title>File Contents</title></head>")
print("<body>")
print("<h1>Contents of data.txt</h1>")

# Try to open a file using a relative path
try:
    with open("data.txt", "r") as f:
        # Wrap the file contents in <pre> so formatting is preserved
        print("<pre>")
        print(f.read())
        print("</pre>")
except FileNotFoundError:
    print("<p><strong>Error:</strong> data.txt not found.</p>")

print("</body>")
print("</html>")
