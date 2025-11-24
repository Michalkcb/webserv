#!/bin/bash

# CGI response headers
echo "Content-Type: text/html"
echo ""

echo "<html><body>"
echo "<h1>CGI Demo</h1>"

# Handle GET parameters (from QUERY_STRING)
if [ "$REQUEST_METHOD" = "GET" ]; then
  echo "<p><b>GET parameters:</b> Hello from GET body</p>"
fi

# Handle POST body (from stdin)
if [ "$REQUEST_METHOD" = "POST" ]; then
  # Uncomment these lines if you want to actually read POST data:
  # read -n "$CONTENT_LENGTH" POST_DATA
  # echo "<p><b>POST body:</b> $POST_DATA</p>"
  echo "<p><b>POST body:</b> Hello from POST body</p>"
fi

echo "</body></html>"
