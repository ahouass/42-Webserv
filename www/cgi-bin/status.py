#!/usr/bin/env python3
"""
CGI script demonstrating custom status codes
"""
import os

# Get status from query string
query_string = os.environ.get('QUERY_STRING', '')
params = {}
if query_string:
    for pair in query_string.split('&'):
        if '=' in pair:
            key, value = pair.split('=', 1)
            params[key] = value

status_code = params.get('code', '200')
status_messages = {
    '200': 'OK',
    '201': 'Created',
    '204': 'No Content',
    '400': 'Bad Request',
    '401': 'Unauthorized',
    '403': 'Forbidden',
    '404': 'Not Found',
    '500': 'Internal Server Error',
    '503': 'Service Unavailable',
}

message = status_messages.get(status_code, 'Unknown Status')

# Output custom status
print(f"Status: {status_code} {message}")
print("Content-Type: text/html")
print("")

print(f"""<!DOCTYPE html>
<html>
<head>
    <title>Status {status_code}</title>
    <style>
        body {{ font-family: Arial; margin: 50px; text-align: center; }}
        .code {{ font-size: 72px; color: #666; }}
        .message {{ font-size: 24px; color: #999; }}
    </style>
</head>
<body>
    <div class="code">{status_code}</div>
    <div class="message">{message}</div>
    <p>Custom status code from CGI script</p>
    <h3>Try other status codes:</h3>
    <p>
        <a href="?code=200">200 OK</a> |
        <a href="?code=201">201 Created</a> |
        <a href="?code=400">400 Bad Request</a> |
        <a href="?code=404">404 Not Found</a> |
        <a href="?code=500">500 Server Error</a>
    </p>
    <p><a href="/cgi-bin/test.py">Back to CGI Test</a></p>
</body>
</html>
""")
