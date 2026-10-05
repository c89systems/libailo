#!/usr/bin/env python3
import json
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parent.parent


class Lifecycle(unittest.TestCase):
    def test_pins_and_targets(self):
        lock = json.loads((ROOT / 'cmake/dependencies.json').read_text())
        targets = {a + '-linux-' + c for a in ['x86_64', 'aarch64', 'armhf'] for c in ['gnu', 'musl']}
        for component in ['cpkt', 'lonejson']:
            self.assertEqual(set(lock[component]['assets']), targets | {'arm64-apple-darwin'})
            self.assertIn('github.com/sa6mwa/', lock[component]['upstream'])
        for component in lock.values():
            for recipe in component.get('assets', {'source': component}).values():
                self.assertRegex(recipe['sha256'], r'^[0-9a-f]{64}$')
                self.assertTrue(recipe['url'].startswith('https://'))

    def test_serial_command_surface(self):
        result = subprocess.run(['make', 'help'], cwd=ROOT, text=True, capture_output=True, check=True)
        for command in ['deps', 'build', 'test', 'format', 'upstream-inventory']:
            self.assertIn(command, result.stdout)
        presets = json.loads((ROOT / 'CMakePresets.json').read_text())
        names = {p['name'] for p in presets['configurePresets']}
        self.assertTrue({'base', 'debug', 'cpu', 'valgrind'} <= names)
        self.assertEqual(sum(n.endswith('-release') for n in names), 6)
        self.assertIn('/build/', (ROOT / '.gitignore').read_text().splitlines())

    def test_owned_runtime_and_compilers(self):
        build = ROOT / 'build/debug'
        description = subprocess.check_output([str(ROOT / 'scripts/cpkt-toolchains.sh'), 'discover', 'x86_64-linux-gnu'], text=True)
        values = dict(line.split('=', 1) for line in description.splitlines() if '=' in line)
        cache = (build / 'CMakeCache.txt').read_text()
        self.assertIn('CMAKE_C_COMPILER:FILEPATH=' + values['cc'], cache)
        self.assertIn('CMAKE_CXX_COMPILER:FILEPATH=' + values['cxx'], cache)
        executable = build / 'ailo_backend_test'
        metadata = subprocess.check_output([values['readelf'], '-l', str(executable)], text=True)
        self.assertIn(values['sysroot'] + '/lib/ld-linux', metadata)
        helper = ROOT / '.cache/deps-build/x86_64-linux-gnu/llama/build/ggml/src/ggml-vulkan/vulkan-shaders-gen-prefix/src/vulkan-shaders-gen-build/CMakeCache.txt'
        compiler = re.search(r'^CMAKE_C_COMPILER:[^=]+=(.*)$', helper.read_text(), re.M)
        cxx = re.search(r'^CMAKE_CXX_COMPILER:[^=]+=(.*)$', helper.read_text(), re.M)
        self.assertEqual(compiler[1], values['cc'])
        self.assertEqual(cxx[1], values['cxx'])
        subprocess.run([str(executable)], check=True, capture_output=True)


if __name__ == '__main__':
    unittest.main()
