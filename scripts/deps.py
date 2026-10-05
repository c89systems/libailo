#!/usr/bin/env python3
"""Acquire pinned components through the shared CMake archive cache."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parent.parent
LOCK = json.loads((ROOT / 'cmake/dependencies.json').read_text())


def acquire(name, target):
    dep = LOCK[name]
    recipe = dep.get('assets', {}).get(target, dep)
    if 'sha256' not in recipe:
        raise SystemExit(f'{name}: no pinned SDK for target {target}')
    base = ROOT / '.cache/deps-build' / target / name
    install = ROOT / '.cache/deps' / target / name / 'install'
    contract = ROOT / '.cache/dependency-contracts' / target / (name + '.txt')
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    if contract.exists() and contract.read_text().strip() == identity and install.exists():
        return install if 'assets' in dep else base / 'source'
    work = ROOT / 'build/deps'
    work.mkdir(parents=True, exist_ok=True)
    result = work / (name + '-' + target + '.txt')
    args = ['cmake', '-DAILO_ARCHIVE_COMPONENT=' + name,
            '-DAILO_ARCHIVE_URL=' + recipe['url'],
            '-DAILO_ARCHIVE_SHA256=' + recipe['sha256'],
            '-DAILO_ARCHIVE_RESULT=' + str(result)]
    if os.environ.get('CPKT_DEPENDENCY_CACHE'):
        args.append('-DCPKT_DEPENDENCY_CACHE=' + os.environ['CPKT_DEPENDENCY_CACHE'])
    subprocess.run(args + ['-P', str(ROOT / 'cmake/AiloArchive.cmake')], check=True)
    archive = Path(result.read_text().strip())
    for owned in [base, install]:
        if owned.exists():
            shutil.rmtree(owned)
    base.mkdir(parents=True)
    with tarfile.open(archive) as tar:
        tar.extractall(base / 'extract', filter='data')
    roots = list((base / 'extract').iterdir())
    if len(roots) != 1 or not roots[0].is_dir():
        raise SystemExit(f'{name}: archive must have one directory root')
    if 'assets' in dep:
        install.parent.mkdir(parents=True, exist_ok=True)
        roots[0].rename(install)
    else:
        roots[0].rename(base / 'source')
        install.mkdir(parents=True)
    contract.parent.mkdir(parents=True, exist_ok=True)
    contract.write_text(identity + '\n')
    return install if 'assets' in dep else base / 'source'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', default='x86_64-linux-gnu')
    parser.add_argument('--component', choices=list(LOCK), action='append')
    args = parser.parse_args()
    for component in args.component or LOCK:
        print(component + '=' + str(acquire(component, args.target)), flush=True)
