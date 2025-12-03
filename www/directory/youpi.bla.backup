#!/bin/sh
# simple CGI that echoes stdin back
# Default safe behavior: stream stdin to stdout without creating files unless debug is enabled.
if [ -n "$WEBSERV_DEBUG_CGI" ]; then
	TMPFILE=$(mktemp /tmp/youpi_stdin.XXXXXX)
	cat > "$TMPFILE"
	L=$(wc -c < "$TMPFILE" | tr -d ' ')
	printf "Status: 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %s\r\n\r\n" "$L"
	cat "$TMPFILE"
	rm -f "$TMPFILE"
	exit 0
else
	printf "Status: 200 OK\r\nContent-Type: text/plain\r\n\r\n"
	cat -
	exit 0
fi
