#!/usr/bin/env python3
"""Deterministic TLS fixtures for the native model cache/transport boundary."""
import argparse
import concurrent.futures
import fcntl
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import os
import shutil
import ssl
import subprocess
import tempfile
import threading
import time
import unittest
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent
OPTIONS = None


class ModelDownload(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.payload = Path(OPTIONS.fixture).read_bytes()
        cls.sha = hashlib.sha256(cls.payload).hexdigest()
        cls.split_payload = [Path(OPTIONS.fixture + suffix).read_bytes() for suffix in
                             ('-00001-of-00002.gguf', '-00002-of-00002.gguf')]
        cls.split_sha = [hashlib.sha256(data).hexdigest() for data in cls.split_payload]
        cls.suite = Path(tempfile.mkdtemp(prefix='model-tls-', dir=ROOT / 'build/tests'))
        cls.cert = cls.suite / 'cert.pem'
        cls.key = cls.suite / 'key.pem'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                        '-days', '1', '-subj', '/CN=localhost', '-addext',
                        'subjectAltName=DNS:localhost,IP:127.0.0.1',
                        '-keyout', str(cls.key), '-out', str(cls.cert)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        cls.wrong_cert = cls.suite / 'wrong-cert.pem'
        cls.wrong_key = cls.suite / 'wrong-key.pem'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                        '-days', '1', '-subj', '/CN=unrelated.invalid', '-addext',
                        'subjectAltName=DNS:unrelated.invalid', '-keyout', str(cls.wrong_key),
                        '-out', str(cls.wrong_cert)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.suite)

    def setUp(self):
        self.work = Path(tempfile.mkdtemp(prefix='case-', dir=self.suite))
        self.requests = []
        self.mutex = threading.Lock()
        self.started = threading.Event()
        self.servers = []
        owner = self

        class Origin(BaseHTTPRequestHandler):
            protocol_version = 'HTTP/1.1'

            def handle(self):
                try:
                    super().handle()
                except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                    pass  # Cancellation and rejected TLS peers intentionally disconnect.

            def do_GET(self):
                name = unquote(urlsplit(self.path).path).rsplit('/', 1)[-1]
                with owner.mutex:
                    owner.requests.append((self.server.server_port, self.path,
                                           self.headers.get('Authorization')))
                    attempt = sum(1 for _, path, _ in owner.requests if path == self.path)
                owner.started.set()
                if name == 'redirect.gguf':
                    return self.redirect('/cdn/model.gguf')
                if name == 'cross.gguf':
                    return self.redirect('https://localhost:%d/cdn/model.gguf' % owner.other.server_port)
                if name == 'http.gguf':
                    return self.redirect('http://127.0.0.1:%d/cdn/model.gguf' % owner.other.server_port)
                if name == 'loop.gguf':
                    return self.redirect(self.path)
                if name == 'credentials.gguf':
                    return self.redirect('https://secret:password@localhost:%d/cdn/model.gguf' % owner.other.server_port)
                if name == 'error.gguf' or (name == 'retry.gguf' and attempt == 1):
                    self.send_response(503)
                    self.send_header('Content-Length', '0')
                    self.end_headers()
                    return
                data = owner.payload
                if name.startswith('part-00001-of-00002'):
                    data = owner.split_payload[0]
                elif name.startswith('part-00002-of-00002'):
                    data = owner.split_payload[1]
                if name == 'bad.gguf':
                    data = b'HTML-error-page' * 100
                if name == 'short.gguf':
                    data = b'GGUF'
                if name == 'stall.gguf':
                    time.sleep(0.2)
                self.send_response(200)
                chunked = name == 'chunked.gguf'
                if chunked:
                    self.send_header('Transfer-Encoding', 'chunked')
                else:
                    self.send_header('Content-Length', str(len(data) + (100 if name == 'truncated.gguf' else 0)))
                self.send_header('Connection', 'close')
                self.end_headers()
                try:
                    for offset in range(0, len(data), 4096):
                        chunk = data[offset:offset + 4096]
                        if chunked:
                            self.wfile.write(('%x\r\n' % len(chunk)).encode() + chunk + b'\r\n')
                        else:
                            self.wfile.write(chunk)
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

        def serve(cert=None, key=None):
            server = ThreadingHTTPServer(('127.0.0.1', 0), Origin)
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(str(cert or self.cert), str(key or self.key))
            server.socket = context.wrap_socket(server.socket, server_side=True)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            self.servers.append((server, thread))
            return server

        self.origin = serve()
        self.other = serve()
        self.serve = serve
        self.url = 'https://127.0.0.1:%d' % self.origin.server_port

    def tearDown(self):
        for server, thread in self.servers:
            server.shutdown()
            server.server_close()
            thread.join()
        shutil.rmtree(self.work)

    def invoke(self, filename='model.gguf', *, root=None, repo='owner/model', revision='fixed',
               sha=None, allow=True, mode='normal', limit=0, attempts=1, timeout=2000,
               origin=None, ca=None, env=None):
        command = [OPTIONS.driver, origin or self.url, str(ca or self.cert),
                   str(root if root is not None else self.work / 'cache'), repo, revision,
                   filename, self.sha if sha is None else sha, '1' if allow else '0',
                   mode, str(limit), str(attempts), str(timeout)]
        environment = dict(os.environ, HTTPS_PROXY='', https_proxy='', ALL_PROXY='',
                           all_proxy='', NO_PROXY='*', no_proxy='*')
        if env:
            environment.update(env)
        if OPTIONS.valgrind:
            command = ['valgrind', '--error-exitcode=99', '--leak-check=full',
                       '--show-leak-kinds=all', '--errors-for-leak-kinds=all',
                       '--track-origins=yes'] + command
        result = subprocess.run(command, env=environment, capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        output = dict(line.split('=', 1) for line in result.stdout.splitlines())
        self.assertNotIn('SYNTHETIC_SECRET', output['message'])
        return output

    def status(self, output, expected):
        self.assertEqual(output['status'], expected, output['message'])
        if expected != 'ok':
            self.assertEqual(output['output_null'], '1')
        self.assertEqual(list(self.work.rglob('.ailo-partial-*')), [], 'partial files were not removed')

    def test_download_offline_hit_and_no_network(self):
        output = self.invoke(filename='nested/model.gguf')
        self.status(output, 'ok')
        path = Path(output['path'])
        self.assertEqual(path.read_bytes(), self.payload)
        requests = len(self.requests)
        self.status(self.invoke(filename='nested/model.gguf', allow=False), 'ok')
        self.assertEqual(len(self.requests), requests)
        self.status(self.invoke(filename='missing.gguf', allow=False), 'download_failed')
        self.assertEqual(len(self.requests), requests)
        self.assertFalse(path.parent.joinpath('missing.gguf').exists())

    def test_identity_encoding_and_environment_precedence(self):
        output = self.invoke(filename='nested/a space%?#.gguf', revision='refs/pr/1')
        self.status(output, 'ok')
        self.assertIn('/resolve/refs%2Fpr%2F1/nested/a%20space%25%3F%23.gguf?download=true', self.requests[-1][1])
        other = self.invoke(filename='nested/a space%?#.gguf', revision='other', mode='path')
        self.assertNotEqual(output['path'], other['path'])
        override = self.invoke(mode='path', env={'XDG_CACHE_HOME': str(self.work / 'xdg'), 'HOME': str(self.work / 'home')})
        self.assertTrue(override['path'].startswith(str(self.work / 'cache') + '/models/'))
        xdg = self.invoke(root='-', mode='path', env={'XDG_CACHE_HOME': str(self.work / 'xdg'), 'HOME': str(self.work / 'home')})
        self.assertTrue(xdg['path'].startswith(str(self.work / 'xdg/ailo/models') + '/'))
        home = self.invoke(root='-', mode='path', env={'XDG_CACHE_HOME': '', 'HOME': str(self.work / 'home')})
        self.assertTrue(home['path'].startswith(str(self.work / 'home/.cache/ailo/models') + '/'))
        self.status(self.invoke(root='-', mode='path', env={'XDG_CACHE_HOME': 'relative'}), 'invalid_argument')
        self.status(self.invoke(root='-', mode='path', env={'XDG_CACHE_HOME': '', 'HOME': ''}), 'invalid_argument')
        self.assertFalse((self.work / 'xdg').exists())
        self.assertFalse((self.work / 'home').exists())

    def test_invalid_sources_fail_before_network(self):
        for filename in ('../evil.gguf', '/absolute.gguf', 'a/../evil.gguf', 'a//evil.gguf', 'a\\evil.gguf', '', '.ailo-state.gguf', 'nested/.ailo-state/model.gguf'):
            self.status(self.invoke(filename=filename), 'invalid_argument')
        for repo in ('../model', 'model', 'a/b/c', 'owner/../model'):
            self.status(self.invoke(repo=repo), 'invalid_argument')
        self.status(self.invoke(revision='../bad'), 'invalid_argument')
        self.status(self.invoke(sha='bad'), 'invalid_argument')
        self.status(self.invoke(root='relative'), 'invalid_argument')
        self.status(self.invoke(mode='bad-token'), 'invalid_argument')
        self.status(self.invoke(attempts=7), 'invalid_argument')
        self.status(self.invoke(timeout=-1), 'invalid_argument')
        self.assertEqual(self.requests, [])
        self.assertFalse((self.work / 'cache').exists())

    def test_http_errors_wrong_content_checksum_truncation_and_limits(self):
        for filename in ('error.gguf', 'bad.gguf', 'short.gguf', 'truncated.gguf'):
            self.status(self.invoke(filename=filename), 'download_failed')
        self.status(self.invoke(filename='wrong-sha.gguf', sha='0' * 64), 'download_failed')
        self.status(self.invoke(filename='too-big.gguf', limit=1024), 'download_failed')
        self.status(self.invoke(filename='chunked.gguf', limit=1024), 'download_failed')
        self.status(self.invoke(filename='stall.gguf', timeout=25), 'download_failed')
        self.assertEqual(list((self.work / 'cache').rglob('*.gguf')), [])

    def test_redirects_tls_and_credential_isolation(self):
        self.status(self.invoke(filename='redirect.gguf'), 'ok')
        self.assertEqual(self.requests[-1][2], 'Bearer SYNTHETIC_SECRET')
        self.status(self.invoke(filename='cross.gguf'), 'ok')
        self.assertEqual(self.requests[-1][0], self.other.server_port)
        self.assertIsNone(self.requests[-1][2])
        other_count = sum(port == self.other.server_port for port, _, _ in self.requests)
        for filename in ('http.gguf', 'credentials.gguf', 'loop.gguf'):
            self.status(self.invoke(filename=filename), 'download_failed')
        self.assertEqual(sum(port == self.other.server_port for port, _, _ in self.requests), other_count)
        count = len(self.requests)
        self.status(self.invoke(filename='origin.gguf', origin=self.url.replace('https:', 'http:')), 'download_failed')
        self.status(self.invoke(filename='untrusted.gguf', ca='-'), 'download_failed')
        wrong = self.serve(self.wrong_cert, self.wrong_key)
        self.status(self.invoke(filename='wrong-host.gguf', ca=self.wrong_cert,
                               origin='https://127.0.0.1:%d' % wrong.server_port), 'download_failed')
        self.assertEqual(len(self.requests), count)

    def test_retry_and_chunked_lengths(self):
        self.status(self.invoke(filename='retry.gguf', attempts=2), 'ok')
        self.assertEqual(sum('retry.gguf' in path for _, path, _ in self.requests), 2)
        self.status(self.invoke(filename='error.gguf', attempts=3), 'download_failed')
        self.assertEqual(sum('error.gguf' in path for _, path, _ in self.requests), 3)
        self.status(self.invoke(filename='chunked.gguf'), 'ok')

    def test_independent_threads_share_transport_lifetime(self):
        output = self.invoke(filename='slow.gguf', mode='threaded')
        self.status(output, 'ok')
        self.assertEqual(len(self.requests), 2)
        self.assertEqual(len(list((self.work / 'cache').rglob('*.gguf'))), 2)

    def test_cancel_and_callback_exception_cleanup(self):
        self.status(self.invoke(filename='before.gguf', mode='cancel-before'), 'cancelled')
        self.assertEqual(self.requests, [])
        self.status(self.invoke(filename='slow.gguf', mode='cancel-transfer'), 'cancelled')
        self.status(self.invoke(filename='throw.gguf', mode='throw-transfer'), 'internal_error')
        self.status(self.invoke(filename='throw-before.gguf', mode='throw-before'), 'internal_error')
        self.assertEqual(list((self.work / 'cache').rglob('*.gguf')), [])

    def test_cached_checksum_mismatch_preserves_file(self):
        output = self.invoke()
        self.status(output, 'ok')
        path = Path(output['path'])
        bad = bytearray(self.payload)
        bad[-1] ^= 1
        path.write_bytes(bad)
        count = len(self.requests)
        self.status(self.invoke(), 'download_failed')
        self.assertEqual(path.read_bytes(), bad)
        self.assertEqual(len(self.requests), count)

    def test_publication_preserves_racing_caller_file(self):
        output = self.invoke(mode='clobber')
        self.status(output, 'busy')
        self.assertEqual(Path(output['path']).read_bytes(), b'caller-owned sentinel')

    def test_concurrent_process_publication_and_lock_cancellation(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            first = pool.submit(self.invoke, filename='slow.gguf')
            self.assertTrue(self.started.wait(5))
            second = pool.submit(self.invoke, filename='slow.gguf')
            self.status(first.result(), 'ok')
            self.status(second.result(), 'ok')
        self.assertEqual(len(self.requests), 1)
        output = self.invoke(mode='path')
        directory = Path(output['path']).parent
        directory.mkdir(parents=True, exist_ok=True)
        name = '.ailo-' + hashlib.sha256(b'model.gguf').hexdigest() + '.lock'
        with (directory / name).open('w') as lock:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
            self.status(self.invoke(mode='busy-lock'), 'busy')
            self.status(self.invoke(mode='cancel-lock'), 'cancelled')
        self.assertEqual(len(self.requests), 1)

    def test_split_shards_and_partial_set_recovery(self):
        name = 'nested/part-00001-of-00002.gguf'
        hashes = self.split_sha[0] + ',' + '0' * 64
        failed = self.invoke(filename=name, mode='split', sha=hashes)
        self.status(failed, 'download_failed')
        first = Path(failed['path'])
        self.assertEqual(first.read_bytes(), self.split_payload[0])
        self.assertFalse(first.with_name('part-00002-of-00002.gguf').exists())
        output = self.invoke(filename=name, mode='split', sha=','.join(self.split_sha))
        self.status(output, 'ok')
        self.assertEqual(first.with_name('part-00002-of-00002.gguf').read_bytes(), self.split_payload[1])
        consumer = subprocess.run([OPTIONS.model_consumer, str(first)],
                                  capture_output=True, text=True, timeout=20)
        self.assertEqual(consumer.returncode, 0, consumer.stderr)
        self.assertIn('C89 CPU GGUF inference', consumer.stdout)
        self.assertEqual(len(self.requests), 3, 'valid first shard should be reused')
        count = len(self.requests)
        self.status(self.invoke(filename='part-00002-of-00002.gguf', mode='split'), 'invalid_argument')
        self.assertEqual(len(self.requests), count)

    def test_interrupted_process_never_publishes_and_next_attempt_cleans(self):
        if OPTIONS.valgrind:
            self.skipTest('SIGKILL has no Memcheck process-exit report; checked in the ordinary suite')
        command = [OPTIONS.driver, self.url, str(self.cert), str(self.work / 'cache'),
                   'owner/model', 'fixed', 'slow.gguf', self.sha, '1', 'normal', '0', '1', '2000']
        environment = dict(os.environ, HTTPS_PROXY='', https_proxy='', ALL_PROXY='',
                           all_proxy='', NO_PROXY='*', no_proxy='*')
        child = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            self.assertTrue(self.started.wait(5))
            deadline = time.monotonic() + 5
            partials = []
            while time.monotonic() < deadline:
                partials = [p for p in self.work.rglob('.ailo-partial-*') if p.stat().st_size > 0]
                if partials:
                    break
                self.assertIsNone(child.poll(), 'transfer ended before interruption')
                time.sleep(0.002)
            self.assertTrue(partials, 'no streamed partial bytes were observed')
            self.assertIsNone(child.poll())
            child.kill()
            child.communicate(timeout=5)
            self.assertEqual(child.returncode, -9)
        finally:
            if child.poll() is None:
                child.kill()
                child.communicate(timeout=5)
        self.assertEqual(list((self.work / 'cache').rglob('slow.gguf')), [])
        self.assertTrue(list(self.work.rglob('.ailo-partial-*')))
        output = self.invoke(filename='slow.gguf')
        self.status(output, 'ok')
        self.assertEqual(Path(output['path']).read_bytes(), self.payload)

    def test_individual_shard_uses_bundle_lock_and_only_target_partials_are_removed(self):
        primary = 'part-00001-of-00002.gguf'
        secondary = 'part-00002-of-00002.gguf'
        planned = self.invoke(filename=secondary, mode='path')
        directory = Path(planned['path']).parent
        directory.mkdir(parents=True)
        lock_name = '.ailo-' + hashlib.sha256(primary.encode()).hexdigest() + '.lock'
        with (directory / lock_name).open('w') as lock:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
            self.status(self.invoke(filename=secondary, mode='busy-lock'), 'busy')
        target = '.ailo-partial-' + hashlib.sha256(secondary.encode()).hexdigest() + '-' + 'a' * 32
        foreign = '.ailo-partial-' + hashlib.sha256(b'other.gguf').hexdigest() + '-' + 'b' * 32
        (directory / target).write_bytes(b'interrupted target')
        (directory / foreign).write_bytes(b'other model partial')
        output = self.invoke(filename=secondary, sha=self.split_sha[1])
        self.assertEqual(output['status'], 'ok', output['message'])
        self.assertFalse((directory / target).exists())
        self.assertEqual((directory / foreign).read_bytes(), b'other model partial')

    def test_symlinks_and_sparse_full_width_limits(self):
        output = self.invoke(mode='path')
        path = Path(output['path'])
        path.parent.mkdir(parents=True)
        target = self.work / 'caller.gguf'
        target.write_bytes(self.payload)
        path.symlink_to(target)
        self.status(self.invoke(), 'io_failed')
        self.assertEqual(target.read_bytes(), self.payload)
        path.unlink()
        shutil.rmtree(self.work / 'cache')
        (self.work / 'cache').symlink_to(self.work)
        self.status(self.invoke(), 'io_failed')
        self.assertEqual(self.requests, [])
        good = self.invoke(root=self.work / 'wide', mode='sparse', sha='-')
        self.status(good, 'ok')
        self.assertEqual(good['total_hi'], '2')
        self.status(self.invoke(root=self.work / 'wide', mode='sparse', sha='-', limit=(1 << 33) - 1), 'download_failed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--driver', required=True)
    parser.add_argument('--fixture', required=True)
    parser.add_argument('--model-consumer', required=True)
    parser.add_argument('--valgrind', action='store_true')
    OPTIONS, rest = parser.parse_known_args()
    (ROOT / 'build/tests').mkdir(parents=True, exist_ok=True)
    unittest.main(argv=[__file__] + rest)
