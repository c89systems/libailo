#!/usr/bin/env python3
"""Serialized local configure/build entry point."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
from deps import ROOT, acquire


def run(*args):
    subprocess.run([str(a) for a in args], check=True, cwd=ROOT)


def configure_dependency(name, target, options=()):
    source = acquire(name, target)
    base = source.parent
    install = ROOT / '.cache/deps' / target / name / 'install'
    run('cmake', '-S', source, '-B', base / 'build', '-G', 'Ninja',
        '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/AiloToolchain.cmake'),
        '-DAILO_TARGET_ID=' + target, '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_INSTALL_PREFIX=' + str(install), '-DCMAKE_INSTALL_LIBDIR=lib',
        '-DCMAKE_INSTALL_MESSAGE=NEVER',
        *options)
    run('cmake', '--build', base / 'build', '--parallel', os.environ.get('AILO_JOBS', '2'))
    run('cmake', '--install', base / 'build')
    return install


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preset', default='debug')
    parser.add_argument('--configure-only', action='store_true')
    args = parser.parse_args()
    presets = json.loads((ROOT / 'CMakePresets.json').read_text())['configurePresets']
    preset = next(p for p in presets if p['name'] == args.preset)
    target = preset.get('cacheVariables', {}).get('AILO_TARGET_ID', 'x86_64-linux-gnu')
    vulkan = preset.get('cacheVariables', {}).get('AILO_VULKAN', True)
    for name in ['cpkt', 'lonejson', 'llama']:
        acquire(name, target)
    source = ROOT / '.cache/deps-build' / target / 'llama/source'
    for patch in (ROOT / 'vendor/llama/patches/series').read_text().splitlines():
        path = ROOT / 'vendor/llama/patches' / patch
        applied = subprocess.run(['patch', '--dry-run', '--reverse', '--silent', '-p1', '-i', str(path)], cwd=source,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if applied.returncode:
            subprocess.run(['patch', '--batch', '--forward', '-p1', '-i', str(path)], cwd=source, check=True)
    # A toolchain added to an already configured CMake project is not loaded.
    # Discard only the lifecycle-owned helper when its compiler route is stale.
    helper = source.parent / 'build/ggml/src/ggml-vulkan/vulkan-shaders-gen-prefix'
    cache = helper / 'src/vulkan-shaders-gen-build/CMakeCache.txt'
    if cache.exists() and ('CMAKE_C_COMPILER:FILEPATH=/usr/' in cache.read_text()
                           or 'CMAKE_CXX_COMPILER:FILEPATH=/usr/' in cache.read_text()):
        shutil.rmtree(helper)
    if vulkan:
        headers = configure_dependency('vulkan-headers', target, ['-DVULKAN_HEADERS_ENABLE_TESTS=OFF'])
        configure_dependency('spirv-headers', target, ['-DSPIRV_HEADERS_ENABLE_TESTS=OFF'])
        configure_dependency('vulkan-loader', target, [
            '-DVulkanHeaders_DIR=' + str(headers / 'share/cmake/VulkanHeaders'),
            '-DBUILD_TESTS=OFF', '-DBUILD_WERROR=OFF',
            '-DBUILD_WSI_XCB_SUPPORT=OFF', '-DBUILD_WSI_XLIB_SUPPORT=OFF',
            '-DBUILD_WSI_WAYLAND_SUPPORT=OFF', '-DBUILD_WSI_XLIB_XRANDR_SUPPORT=OFF'])
    run('cmake', '--preset', args.preset)
    if not args.configure_only:
        run('cmake', '--build', '--preset', args.preset, '--parallel', os.environ.get('AILO_JOBS', '2'))
