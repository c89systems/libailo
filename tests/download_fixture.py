#!/usr/bin/env python3
"""Serve synthetic model bytes and a request journal; never run test commands."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import ssl
import sys
import threading
import time
from urllib.parse import unquote, urlsplit

work, fixture = map(Path, sys.argv[1:])
payload = fixture.read_bytes()
parts = [Path(str(fixture) + suffix).read_bytes() for suffix in
         ('-00001-of-00002.gguf', '-00002-of-00002.gguf')]
requests = []
mutex = threading.Lock()


class Origin(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def handle(self):
        try:
            super().handle()
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass  # Cancellation and TLS rejection intentionally disconnect.

    def do_GET(self):
        name = unquote(urlsplit(self.path).path).rsplit('/', 1)[-1]
        with mutex:
            requests.append(self.path)
            attempt = requests.count(self.path)
            with (work / 'requests').open('a') as journal:
                journal.write('%d\t%s\t%s\n' % (self.server.server_port,
                              self.path, self.headers.get('Authorization', '-')))
        if name == 'redirect.gguf':
            return self.redirect('/cdn/model.gguf')
        if name == 'cross.gguf':
            return self.redirect('https://localhost:%d/cdn/model.gguf' % other.server_port)
        if name == 'http.gguf':
            return self.redirect('http://127.0.0.1:%d/cdn/model.gguf' % other.server_port)
        if name == 'credentials.gguf':
            return self.redirect('https://secret:password@localhost:%d/cdn/model.gguf' % other.server_port)
        if name == 'loop.gguf':
            return self.redirect(self.path)
        if name == 'error.gguf' or (name == 'retry.gguf' and attempt == 1):
            self.send_response(503)
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        data = payload
        if name.startswith('part-00001-of-00002'):
            data = parts[0]
        elif name.startswith('part-00002-of-00002'):
            data = parts[1]
        elif name == 'bad.gguf':
            data = b'HTML-error-page' * 100
        elif name == 'short.gguf':
            data = b'GGUF'
        if name == 'stall.gguf':
            time.sleep(0.2)
        self.send_response(200)
        chunked = name == 'chunked.gguf'
        self.send_header('Transfer-Encoding' if chunked else 'Content-Length',
                         'chunked' if chunked else str(len(data) + (100 if name == 'truncated.gguf' else 0)))
        self.send_header('Connection', 'close')
        self.end_headers()
        try:
            for offset in range(0, len(data), 4096):
                chunk = data[offset:offset + 4096]
                self.wfile.write(('%x\r\n' % len(chunk)).encode() + chunk + b'\r\n' if chunked else chunk)
                self.wfile.flush()
                if name == 'slow.gguf':
                    time.sleep(0.005)
            if chunked:
                self.wfile.write(b'0\r\n\r\n')
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass
        self.close_connection = True

    def redirect(self, location):
        self.send_response(302)
        self.send_header('Location', location)
        self.send_header('Content-Length', '0')
        self.end_headers()

    def log_message(self, *args):
        pass


def serve(certificate, key):
    server = ThreadingHTTPServer(('127.0.0.1', 0), Origin)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(work / certificate), str(work / key))
    server.socket = context.wrap_socket(server.socket, server_side=True)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


origin = serve('cert.pem', 'key.pem')
other = serve('cert.pem', 'key.pem')
wrong = serve('wrong-cert.pem', 'wrong-key.pem')
(work / 'requests').touch()
(work / 'ports').write_text('%d %d %d\n' % (origin.server_port, other.server_port, wrong.server_port))
threading.Event().wait()
