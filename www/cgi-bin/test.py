#!/usr/bin/env python3
# Simple CGI test script

import os
import sys

# CGI scripts must output headers first, then a blank line, then content
print("Content-Type: text/html")
print("")  # Blank line separates headers from body

print("""<!DOCTYPE html>
<html>
<head><title>CGI Test</title></head>
<body>
<h1>CGI Script Running!</h1>
<h2>Environment Variables:</h2>
<ul>""")

# Print relevant CGI environment variables
cgi_vars = [
    'REQUEST_METHOD', 'QUERY_STRING', 'CONTENT_TYPE', 'CONTENT_LENGTH',
    'SCRIPT_NAME', 'PATH_INFO', 'SERVER_NAME', 'SERVER_PORT',
    'HTTP_HOST', 'HTTP_USER_AGENT'
]

for var in cgi_vars:
    value = os.environ.get(var, 'Not Set')
    print(f"<li><b>{var}:</b> {value}</li>")

print("""</ul>
</body>
</html>""")
