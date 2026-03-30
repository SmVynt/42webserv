#!/bin/bash

echo "Content-Type: text/html"
echo ""
echo "<html>"
echo "<head><title>Shell CGI Test</title></head>"
echo "<body>"
echo "<h1>Shell CGI Script</h1>"
echo "<p>QUERY_STRING: $QUERY_STRING</p>"
echo "<p>REQUEST_METHOD: $REQUEST_METHOD</p>"
echo "<p>SERVER_NAME: $SERVER_NAME</p>"
echo "<p>SERVER_PORT: $SERVER_PORT</p>"
echo "</body>"
echo "</html>"
