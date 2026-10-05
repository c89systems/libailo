#!/usr/bin/env python3
"""Exercise real CMake acquisition against a counted synthetic origin."""
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parent.parent


class ArchiveCache(unittest.TestCase):
    def setUp(self):
        (ROOT / 'build/tests').mkdir(parents=True, exist_ok=True)
        self.work = Path(tempfile.mkdtemp(prefix='cache-', dir=ROOT / 'build/tests'))
        self.content = b'synthetic pinned dependency archive\n'
        self.sha = hashlib.sha256(self.content).hexdigest()
        self.requests = 0
        owner = self

        class Origin(BaseHTTPRequestHandler):
            def do_GET(self):
                owner.requests += 1
                data = b'corrupt' if self.path == '/bad' else owner.content
                self.send_response(200)
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def log_message(self, *args):
                pass

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Origin)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        shutil.rmtree(self.work)

    def acquire(self, path='/first.tar.gz', sha=None):
        return subprocess.run([
            'cmake', '-DCPKT_DEPENDENCY_CACHE=' + str(self.work / 'shared'),
            '-DAILO_ARCHIVE_TEST_HTTP=ON', '-DAILO_ARCHIVE_COMPONENT=synthetic',
            '-DAILO_ARCHIVE_URL=http://127.0.0.1:' + str(self.server.server_port) + path,
            '-DAILO_ARCHIVE_SHA256=' + (sha or self.sha),
            '-DAILO_ARCHIVE_RESULT=' + str(self.work / 'result'),
            '-P', str(ROOT / 'cmake/AiloArchive.cmake')], capture_output=True, text=True)

    def test_miss_and_offline_alias_hit(self):
        result = self.acquire()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.requests, 1)
        original = Path((self.work / 'result').read_text().strip())
        self.assertEqual(original.read_bytes(), self.content)
        result = self.acquire('/different-name-and-url.tar.gz')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.requests, 1, 'verified digest hits must make zero requests')
        self.assertEqual(Path((self.work / 'result').read_text().strip()), original)

    def test_corrupt_hit_is_replaced(self):
        self.assertEqual(self.acquire().returncode, 0)
        cached = Path((self.work / 'result').read_text().strip())
        cached.write_bytes(b'corrupt')
        result = self.acquire()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.requests, 2)
        self.assertEqual(cached.read_bytes(), self.content)

    def test_wrong_digest_never_publishes(self):
        result = self.acquire('/bad')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('SHA-256 mismatch', result.stderr)
        self.assertEqual(list((self.work / 'shared/archives/sha256' / self.sha).iterdir()), [])
        self.assertFalse((self.work / 'result').exists())

    def test_invalid_pin_fails_before_network(self):
        result = self.acquire(sha='invalid')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.requests, 0)


if __name__ == '__main__':
    unittest.main()
