#!/usr/bin/env python3
"""Build a semantic inventory of the pinned upstream public C API."""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
GGML = ['ggml.h', 'ggml-cpu.h', 'ggml-alloc.h', 'ggml-backend.h',
        'ggml-blas.h', 'ggml-cann.h', 'ggml-cuda.h', 'ggml-opt.h',
        'ggml-metal.h', 'ggml-rpc.h', 'ggml-virtgpu.h', 'ggml-sycl.h',
        'ggml-vulkan.h', 'ggml-webgpu.h', 'ggml-zendnn.h',
        'ggml-openvino.h', 'gguf.h']
HEADERS = ['include/llama.h'] + ['ggml/include/' + h for h in GGML] + [
    'tools/mtmd/mtmd.h', 'tools/mtmd/mtmd-helper.h']
PREFIX = re.compile(r'^(llama_|ggml_|gguf_|mtmd_)')


def inventory(source, clang):
    cmake = (source / 'ggml/CMakeLists.txt').read_text()
    public = re.search(r'set\(GGML_PUBLIC_HEADERS(.*?)\)', cmake, re.S)
    if not public:
        raise SystemExit('Cannot determine upstream installed GGML headers')
    installed = set(re.findall(r'include/[^\s]+\.h', public[1]))
    expected = {'include/' + h for h in GGML} | {'include/ggml-cpp.h'}
    if installed != expected:
        raise SystemExit(f'Uninventoried public GGML header change: {installed ^ expected}')
    work = ROOT / 'build/inventory'
    work.mkdir(parents=True, exist_ok=True)
    tu = work / 'public.cpp'
    tu.write_text(''.join('#include "' + h + '"\n' for h in HEADERS))
    args = [clang, '-x', 'c++', '-std=c++17', '-Wno-deprecated-declarations',
            '-I' + str(source), '-I' + str(source / 'ggml/include'),
            '-I' + str(source / 'include'), '-fsyntax-only', str(tu)]
    ast_file = work / 'ast.json'
    with ast_file.open('w') as output:
        subprocess.run(args + ['-Xclang', '-ast-dump=json'], stdout=output, check=True)
    ast = json.loads(ast_file.read_text())
    records = {}
    functions = {}
    typedefs = {}
    enums = {}
    constants = {}
    excluded_cpp_wrappers = set()

    def canonical(type_):
        return type_.replace(str(source) + '/', '')

    def field(f):
        result = {'name': f.get('name', ''), 'type': canonical(f.get('type', {}).get('qualType', ''))}
        if f.get('isBitfield'):
            result['bitfield'] = True
        return result

    def walk(node, namespace=()):
        kind = node.get('kind')
        name = node.get('name', '')
        if kind == 'NamespaceDecl':
            namespace = namespace + (name,)
        if kind == 'VarDecl' and not namespace and re.match(r'^(?:GGML|GGUF|LLAMA|MTMD)_', name):
            constants[name] = {'name': name, 'type': canonical(node['type']['qualType'])}
        if PREFIX.match(name) and namespace:
            if kind in {'FunctionDecl', 'CXXRecordDecl', 'TypedefDecl', 'EnumDecl'}:
                excluded_cpp_wrappers.add('::'.join(namespace + (name,)))
        elif PREFIX.match(name):
            if kind == 'FunctionDecl':
                functions[name] = {'name': name, 'type': canonical(node['type']['qualType']),
                                   'parameters': [field(n) for n in node.get('inner', []) if n['kind'] == 'ParmVarDecl'],
                                   'variadic': node.get('variadic', False)}
            elif kind == 'TypedefDecl':
                typedefs[name] = {'name': name, 'type': canonical(node['type']['qualType'])}
            elif kind == 'CXXRecordDecl' and node.get('completeDefinition'):
                records[name] = {'name': name, 'tag': node['tagUsed'],
                                 'fields': [field(n) for n in node.get('inner', []) if n['kind'] == 'FieldDecl']}
                # Preserve anonymous union/record fields, not only named outer fields.
                def nested(n):
                    return {'tag': n.get('tagUsed'), 'fields': [field(f) for f in n.get('inner', []) if f['kind'] == 'FieldDecl'],
                            'nested': [nested(c) for c in n.get('inner', []) if c['kind'] == 'CXXRecordDecl' and c.get('completeDefinition')]}
                records[name]['nested'] = [nested(n) for n in node.get('inner', []) if n['kind'] == 'CXXRecordDecl' and n.get('completeDefinition')]
            elif kind == 'EnumDecl':
                vals = []
                current = -1
                for entry in node.get('inner', []):
                    if entry['kind'] != 'EnumConstantDecl':
                        continue
                    def value(n):
                        if n['kind'] == 'ConstantExpr' and 'value' in n:
                            return int(n['value'])
                        for child in n.get('inner', []):
                            found = value(child)
                            if found is not None:
                                return found
                        return None
                    explicit = value(entry)
                    current = current + 1 if explicit is None else explicit
                    vals.append({'name': entry['name'], 'value': current})
                enums[name] = {'name': name, 'values': vals}
        for child in node.get('inner', []):
            walk(child, namespace)

    walk(ast)
    macros_text = subprocess.check_output([a for a in args if a != '-fsyntax-only'] + ['-E', '-dM'], text=True)
    macros = []
    for line in macros_text.splitlines():
        match = re.match(r'#define ((?:LLAMA|GGML|GGUF|MTMD)_[A-Za-z0-9_]+)(\([^ ]*\))?\s*(.*)', line)
        if match:
            macros.append({'name': match[1], 'parameters': match[2], 'value': canonical(match[3])})
    return {'revision': '8345f333951c661d166b00e6f9362e553768f292', 'headers': HEADERS,
            'excluded_headers': {'ggml/include/ggml-cpp.h': 'C++ utilities, excluded by product scope'},
            'excluded_cpp_wrappers': sorted(excluded_cpp_wrappers),
            'functions': sorted(functions.values(), key=lambda x: x['name']),
            'records': sorted(records.values(), key=lambda x: x['name']),
            'typedefs': sorted(typedefs.values(), key=lambda x: x['name']),
            'enums': sorted(enums.values(), key=lambda x: x['name']),
            'constants': sorted(constants.values(), key=lambda x: x['name']),
            'macros': sorted(macros, key=lambda x: x['name'])}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / '.cache/deps-build/x86_64-linux-gnu/llama/source')
    parser.add_argument('--clang', default='clang++')
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    result = inventory(args.source.resolve(), args.clang)
    output = json.dumps(result, indent=2) + '\n'
    destination = ROOT / 'docs/upstream-api.json'
    if args.write:
        destination.write_text(output)
    elif not destination.exists() or destination.read_text() != output:
        raise SystemExit('Upstream public API inventory differs; inspect changes before updating it.')
    print(', '.join(f'{key}={len(result[key])}' for key in ['headers', 'functions', 'records', 'typedefs', 'enums', 'macros']))
