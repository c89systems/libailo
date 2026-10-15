#!/usr/bin/env python3
"""Serve archive-cache inputs and count requests; no assertions or commands."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import sys

work = Path(sys.argv[1])


class Origin(BaseHTTPRequestHandler):
    def do_GET(self):
        with (work / 'requests').open('a') as journal:
            journal.write(self.path + '\n')
        data = b'corrupt' if self.path == '/bad' else (work / 'payload').read_bytes()
        self.send_response(200)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, *args):
        pass


server = HTTPServer(('127.0.0.1', 0), Origin)
(work / 'requests').touch()
(work / 'port').write_text(str(server.server_port))
server.serve_forever()
