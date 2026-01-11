#!/usr/bin/env python3
"""
CGI script demonstrating redirect
"""
import os

# Get redirect target from query string
query_string = os.environ.get('QUERY_STRING', '')
params = {}
if query_string:
    for pair in query_string.split('&'):
        if '=' in pair:
            key, value = pair.split('=', 1)
            params[key] = value

target = params.get('url', '/cgi-bin/test.py')

# Output redirect headers
print(f"Status: 302 Found")
print(f"Location: {target}")
print("Content-Type: text/html")
print("")

print(f"""<!DOCTYPE html>
<html>
<head><title>Redirecting...</title></head>
<body>
<p>Redirecting to <a href="{target}">{target}</a>...</p>
</body>
</html>
""")
