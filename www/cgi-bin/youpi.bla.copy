#!/bin/sh
# simple CGI that echoes stdin back
# Read all stdin to a temp file
TMPFILE=$(mktemp /tmp/youpi_stdin.XXXXXX)
cat > "$TMPFILE"
L=$(wc -c < "$TMPFILE" | tr -d ' ')
printf "Status: 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %s\r\n\r\n" "$L"
cat "$TMPFILE"
rm -f "$TMPFILE"
exit 0
