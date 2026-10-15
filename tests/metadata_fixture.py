#!/usr/bin/env python3
"""Serve small deterministic publisher metadata inputs; assertions live in C++."""
import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import ssl
import sys


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        parts = self.path.split('/')
        if (len(parts) != 7 or parts[1:3] != ['api', 'models'] or
                parts[5] != 'revision' or parts[6] != 'a' * 40):
            self.send_error(404)
            return
        repo = '/'.join(parts[3:5])
        case = parts[4]
        payload = {'id': repo, 'sha': 'a' * 40, 'cardData': {
            'license': 'apache-2.0', 'base_model': 'fixture/base',
            'ignored_private': 'SYNTHETIC_SECRET'}}
        if case == 'mismatch-sha':
            payload['sha'] = 'b' * 40
        if case == 'mismatch-repo':
            payload['id'] = 'fixture/foreign'
        data = json.dumps(payload).encode()
        if case == 'bad-json':
            data = b'{'
        if case == 'duplicate':
            data = b'{"id":"fixture/duplicate","id":"fixture/duplicate"}'
        if case == 'too-large':
            data = b' ' * (1024 * 1024 + 1)
        self.send_response(503 if case == 'http-error' else 200)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass

    def log_message(self, *_):
        pass


if __name__ == '__main__':
    cert, key, port_file = sys.argv[1:]
    server = HTTPServer(('127.0.0.1', 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    Path(port_file).write_text(str(server.server_port), encoding='ascii')
    server.serve_forever()
