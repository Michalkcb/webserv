#!/usr/bin/env php
<?php
// CGI response headers
echo "Content-Type: text/html\r\n\r\n";

echo "<html><body>\n";
echo "<h1>CGI Demo</h1>\n";

// Handle GET parameters
if ($_SERVER['REQUEST_METHOD'] === 'GET') {
    echo "<p><b>GET parameters:</b> Hello from GET body</p>\n";
}

// Handle POST body
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    // Uncomment these lines if you want to actually read POST data:
    // $post_data = file_get_contents("php://stdin");
    // echo "<p><b>POST body:</b> $post_data</p>";
    echo "<p><b>POST body:</b> Hello from POST body</p>\n";
}
echo "</body></html>\n";
echo "</body></html>";
?>
