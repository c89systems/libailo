#!/usr/bin/env python3
"""Generate typed C89 bindings from the compiler-derived upstream inventory.

This is AST/inventory processing. Generated code is tracked; builds never invoke
this generator. Unsupported constructs remain explicit coverage gaps.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
DATA = json.loads((ROOT / 'docs/upstream-api.json').read_text())
ALIASES = {a['name']: a['type'] for a in DATA['typedefs']}
RECORDS = {r['name']: r for r in DATA['records']}
ENUMS = {e['name']: e for e in DATA['enums']}
SCALARS = {'bool': 'ailo_bool', 'int8_t': 'ailo_i8', 'uint8_t': 'ailo_u8',
           'int16_t': 'ailo_i16', 'uint16_t': 'ailo_u16',
           'int32_t': 'ailo_i32', 'uint32_t': 'ailo_u32',
           'int64_t': 'ailo_i64', 'uint64_t': 'ailo_u64',
           'char': 'char', 'unsigned char': 'unsigned char',
           'int': 'int', 'unsigned int': 'unsigned int',
           'float': 'float', 'double': 'double', 'void': 'void',
           'size_t': 'size_t', 'FILE': 'FILE'}
SPECIAL_MAP = 'std::map<ggml_backend_dev_t, size_t>'
MAP_PUBLIC = 'struct ailo_backend_memory_map'
STORAGES = {}
TAGS = set(RECORDS)
GAPS = []
H = []
C = []
EXPORTS = []
CALLBACKS = {}
COVERAGE = {'functions': [], 'fields': [], 'records': [], 'typedefs': [], 'enums': [], 'macros': []}
MACRO_ADAPTERS = {
    'GGML_ABORT', 'GGML_API', 'GGML_ASSERT', 'GGML_ATTRIBUTE_FORMAT',
    'GGML_BACKEND_API', 'GGML_DEPRECATED', 'GGML_NORETURN', 'GGML_N_TASKS_MAX',
    'GGML_PAD', 'GGML_RESTRICT', 'GGML_TENSOR_BINARY_OP_LOCALS',
    'GGML_TENSOR_BINARY_OP_LOCALS01', 'GGML_TENSOR_LOCALS',
    'GGML_TENSOR_LOCALS_1', 'GGML_TENSOR_LOCALS_2', 'GGML_TENSOR_LOCALS_3',
    'GGML_TENSOR_TERNARY_OP_LOCALS', 'GGML_TENSOR_UNARY_OP_LOCALS',
    'GGML_UNREACHABLE', 'GGML_UNUSED', 'GGML_UNUSED_VARS', 'LLAMA_API', 'MTMD_API'
}
NONCAPABILITY_MACROS = {'LLAMA_H', 'MTMD_H', 'MTMD_HELPER_H'}
BACKEND_FUNCTIONS = {}
MANUAL_FUNCTIONS = {'ggml_abort', 'ggml_format_name'}
for backend in ['blas', 'cann', 'cuda', 'metal', 'rpc', 'virtgpu', 'sycl', 'vulkan', 'webgpu', 'zendnn', 'openvino']:
    header = ROOT / '.cache/deps-build/x86_64-linux-gnu/llama/source/ggml/include' / ('ggml-' + backend + '.h')
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', header.read_text(), flags=re.S)
    for name in re.findall(r'\b(ggml_[A-Za-z0-9_]+)\s*\(', source):
        BACKEND_FUNCTIONS[name] = backend


class Gap(Exception):
    pass


def clean(type_):
    return re.sub(r'\s+', ' ', type_.replace('__restrict', '').replace('restrict ', '')).strip()


class Type:
    def __init__(self, text):
        self.original = clean(text)
        text = self.original
        seen = set()
        while True:
            if text == SPECIAL_MAP:
                break
            match = next((m for m in re.finditer(r'\b(' + '|'.join(map(re.escape, ALIASES)) + r')\b', text)
                          if not text[:m.start()].endswith(('struct ', 'enum '))), None)
            if not match:
                break
            name = match[1]
            if name == 'ggml_guid':
                # Native and C89 GUID bytes have exactly the same element type.
                break
            if name in seen:
                raise Gap('recursive alias ' + name)
            seen.add(name)
            replacement = ALIASES[name]
            text = text[:match.start()] + replacement + text[match.end():]
        self.native = clean(text)
        self.native = self.native.replace('struct ggml_bf16_t', 'ggml_bf16_t')
        callback = re.fullmatch(r'(.*?)\(\s*(\*+)\s*\)\s*\((.*)\)(\s*\*)?', self.native)
        if callback:
            signature = clean(callback[1]) + ' (*)(' + clean(callback[3]) + ')'
            if signature not in CALLBACKS:
                CALLBACKS[signature] = {'name': 'ailo_callback_' + hashlib.sha256(signature.encode()).hexdigest()[:12],
                                       'return': clean(callback[1]), 'arguments': split_arguments(callback[3]),
                                       'signature': signature, 'aliases': []}
            self.callback = CALLBACKS[signature]
            self.kind = 'callback'
            self.name = self.callback['name']
            self.base = self.name + '_native'
            self.depth = len(callback[2]) + bool(callback[4])
            self.const = False
            self.storage = None
            self.native = self.base + ' *' * (self.depth - 1)
            self.public_base = 'struct ' + self.name
            return
        if '(' in self.native:
            raise Gap('anonymous record must be expanded by field path')
        if '[' in self.native:
            raise Gap('array needs element access')
        parts = self.native.split('*')
        self.depth = len(parts) - 1
        self.qualifiers = parts[1:]
        base = parts[0].strip()
        self.const = base.startswith('const ')
        self.base = base.removeprefix('const ').strip()
        if self.base.startswith('struct ') or self.base == 'ggml_bf16_t':
            self.kind = 'record'
            self.name = self.base[7:] if self.base.startswith('struct ') else self.base
            TAGS.add(self.name)
            self.public_base = 'struct ailo_' + self.name
        elif self.base.startswith('enum '):
            self.kind = 'enum'
            self.name = self.base[5:]
            if self.name not in ENUMS:
                raise Gap('unknown enum ' + self.name)
            self.public_base = 'enum ailo_' + self.name
        elif self.base == SPECIAL_MAP:
            self.kind = 'map'
            self.name = 'backend_memory_map'
            self.public_base = MAP_PUBLIC
        elif self.base == 'ggml_guid':
            self.kind = 'guid'
            self.name = 'ggml_guid'
            self.public_base = 'ailo_ggml_guid'
        elif self.base in SCALARS:
            self.kind = 'scalar'
            self.name = self.base
            self.public_base = SCALARS[self.base]
        else:
            raise Gap('unknown type ' + self.native)
        self.storage = None
        if self.depth and ((self.kind in {'record', 'map'} and self.depth > 1)
                           or self.kind == 'enum'
                           or self.base in {'bool', 'int64_t', 'uint64_t'}):
            # The last pointer addresses native elements. Preserve the element
            # type and its qualifiers; strip only top-level element constness.
            element = self.native.rsplit('*', 1)[0].strip()
            if self.depth > 1:
                element = re.sub(r'\s*\bconst\s*$', '', element).rstrip()
            if self.depth == 1:
                element = element.removeprefix('const ')
            key = clean(element)
            slug = re.sub(r'[^A-Za-z0-9]+', '_', key).strip('_')
            self.storage = 'ailo_storage_' + slug
            if key not in STORAGES:
                STORAGES[key] = {'name': self.storage, 'element': key}

    def public(self, input=False):
        if self.kind == 'callback':
            return self.public_base + ' *' * self.depth
        if self.storage:
            readonly = self.const if self.depth == 1 else 'const' in self.qualifiers[-2]
            return ('const ' if readonly else '') + 'struct ' + self.storage + ' *'
        if self.depth:
            base = ('const ' if self.const else '') + self.public_base
            return base + ''.join(' *' + q for q in self.qualifiers)
        if self.kind in {'record', 'map'}:
            return ('const ' if input else '') + self.public_base + ' *'
        return self.public_base

    def to_native(self, expression):
        if self.kind == 'callback':
            if self.depth != 1:
                raise Gap('Callback output needs a temporary native function pointer')
            return '(' + expression + ' ? ' + expression + '->target : nullptr)'
        if self.depth:
            if self.storage or self.kind in {'record', 'map'}:
                return 'reinterpret_cast<' + self.native + '>(' + expression + ')'
            return 'static_cast<' + self.native + '>(' + expression + ')'
        if self.kind in {'record', 'map'}:
            return '*reinterpret_cast<const ' + self.base + ' *>(' + expression + ')'
        if self.base in {'int64_t', 'uint64_t'}:
            return 'ailo::detail::to_native(' + expression + ')'
        if self.base == 'bool':
            return '(' + expression + ' != 0)'
        return 'static_cast<' + self.native + '>(' + expression + ')'

    def from_native(self, expression):
        if self.kind == 'callback':
            return self.name + '_view(' + expression + ')'
        if self.depth:
            if self.storage or self.kind in {'record', 'map'}:
                return 'reinterpret_cast<' + self.public() + '>(' + expression + ')'
            return 'static_cast<' + self.public() + '>(' + expression + ')'
        if self.kind in {'record', 'map'}:
            raise Gap('record result requires owned allocation')
        if self.base == 'int64_t':
            return 'ailo::detail::from_signed(' + expression + ')'
        if self.base == 'uint64_t':
            return 'ailo::detail::from_unsigned(' + expression + ')'
        return 'static_cast<' + self.public() + '>(' + expression + ')'

    def required(self, expression):
        return not self.depth and self.kind in {'record', 'map'}


def emit(name, parameters, body, doc, result=None, validate=(), prelude=''):
    params = list(parameters)
    if result:
        params.append((result + ' *', 'output'))
    params.append(('ailo_error *', 'error'))
    signature = ', '.join(type_ + ' ' + param for type_, param in params)
    H.append('/** ' + doc + ' */\nailo_status ' + name + '(' + signature + ');')
    cpp = 'extern "C" __attribute__((visibility("default"))) ailo_status ' + name + '(' + signature + ') {\n'
    if result:
        cpp += 'if (!output) return ailo_error_set(error, AILO_INVALID_ARGUMENT, "Result output is required");\n*output = {};\n'
    cpp += prelude
    for expression in validate:
        cpp += 'if (!' + expression + ') return ailo_error_set(error, AILO_INVALID_ARGUMENT, "Native-backed record or storage is required");\n'
    cpp += 'return ailo::detail::native_boundary(error, [&]() {\n' + body + '\n});\n}'
    C.append(cpp)
    EXPORTS.append(name)


def result_body(type_, expression):
    if not type_.depth and type_.kind in {'record', 'map'}:
        return ('std::unique_ptr<' + type_.base + '[]> result(new ' + type_.base + '[1]());\n'
                'result[0] = ' + expression + ';\n'
                '*output = reinterpret_cast<' + type_.public() + '>(result.release());')
    return '*output = ' + type_.from_native(expression) + ';'


def split_arguments(text):
    if text.strip() in {'', 'void'}:
        return []
    result, depth, start = [], 0, 0
    for index, char in enumerate(text):
        depth += char in '(['
        depth -= char in ')]'
        if char == ',' and not depth:
            result.append(clean(text[start:index]))
            start = index + 1
    result.append(clean(text[start:]))
    return result


def ffi_description(t, layout='self.core.layout'):
    if t.kind == 'record' and not t.depth:
        def record_type(record):
            nested = iter(record['nested'])
            members = []
            for field in record['fields']:
                if '(unnamed ' in field['type'] or '(anonymous ' in field['type']:
                    members.append(record_type(next(nested)))
                else:
                    array = re.fullmatch(r'(.+)\[(\d+)\]', field['type'])
                    value = ffi_description(Type(array[1] if array else field['type']), layout)
                    if array:
                        value = layout + '.aggregate(std::vector<ffi_type *>(' + array[2] + ', ' + value + '))'
                    members.append(value)
            return layout + '.aggregate({' + ', '.join(members) + '})'
        return record_type(RECORDS[t.name])
    return 'ailo::detail::ffi_scalar<' + t.native + '>()'


def generate_callbacks():
    declarations, definitions = [], []
    # Callback arguments can themselves be callbacks. Discover them first.
    handled = set()
    while len(handled) < len(CALLBACKS):
        for key, cb in list(CALLBACKS.items()):
            if key not in handled:
                handled.add(key)
                Type(cb['return'])
                for arg in cb['arguments']:
                    Type(arg)
    for cb in CALLBACKS.values():
        name = cb['name']
        native = name + '_native'
        ret = Type(cb['return'])
        args = [Type(arg) for arg in cb['arguments']]
        definitions.append('using ' + native + ' = ' + cb['signature'] + ';')
        declarations.append('typedef ' + ret.public() + ' (*' + name + '_fn)(' + ', '.join(
            [t.public() for t in args] + ['void *bridge_user_data']) + ');')
        definitions.append('struct ' + name + ' { ' + native + ' target = nullptr; ' + name + '_fn function = nullptr; void *user = nullptr; ailo::detail::callback_core core; };')
    for cb in CALLBACKS.values():
        name = cb['name']
        native = name + '_native'
        definitions.append('static ' + name + ' * ' + name + '_view(' + native + ' target) { if (!target) return nullptr; std::unique_ptr<' + name + '> view(new ' + name + '); view->target = target; return view.release(); }')
    for cb in CALLBACKS.values():
        name = cb['name']
        native = name + '_native'
        ret = Type(cb['return'])
        args = [Type(arg) for arg in cb['arguments']]
        is_void = ret.base == 'void' and not ret.depth
        body = ['static void ' + name + '_dispatch(ffi_cif *, void *result, void **arguments, void *data) noexcept {',
                'auto & self = *static_cast<' + name + ' *>(data);', '(void)result; (void)arguments;', 'try {']
        converted = []
        for index, t in enumerate(args):
            var = 'value_' + str(index)
            body.append('using type_' + str(index) + ' = ' + t.native + ';')
            body.append('auto ' + var + ' = *static_cast<type_' + str(index) + ' *>(arguments[' + str(index) + ']);')
            if t.kind == 'callback':
                body.append(t.name + ' view_' + str(index) + '; view_' + str(index) + '.target = ' + var + ';')
                converted.append(var + ' ? &view_' + str(index) + ' : nullptr')
            else:
                converted.append(t.from_native(var))
        call = 'self.function(' + ', '.join(converted + ['self.user']) + ')'
        if is_void:
            body.append(call + ';')
        else:
            body.append('auto output = ' + call + ';')
            if ret.required('output'):
                body.append('if (!output) throw std::runtime_error("Callback returned a missing record");')
            body.append('ailo::detail::ffi_store(result, ' + ret.to_native('output') + ');')
        body += ['} catch (...) { self.core.failed();']
        if not is_void:
            if 'ggml_abort_callback' not in cb['aliases']:
                body.append('using result_type = ' + ret.native + ';')
            fallback = 'true' if 'ggml_abort_callback' in cb['aliases'] else 'result_type{}'
            body.append('ailo::detail::ffi_store(result, ' + fallback + ');')
        body += ['}', '}']
        definitions.append('\n'.join(body))
        start_h, start_c = len(H), len(C)
        public = 'struct ' + name + ' *'
        aliases = sorted(set(cb['aliases'])) or [name.removeprefix('ailo_')]
        for alias in aliases:
            root = 'ailo_' + alias
            if alias not in ALIASES:
                H.append('typedef ' + public + ' ' + root + ';')
            H.append('typedef ' + name + '_fn ' + root + '_fn;')
            emit(root + '_new', [(root + '_fn', 'function'), ('void *', 'user_data')],
                 'std::unique_ptr<' + name + '> result(new ' + name + ');\n'
                 'result->function = function; result->user = user_data; auto & self = *result;\n'
                 'self.core.prepare(' + ffi_description(ret) + ', {' + ', '.join(ffi_description(t) for t in args) + '}, ' + name + '_dispatch, &self);\n'
                 + ('' if is_void else 'if (self.core.cif.rtype->size != sizeof(' + ret.native + ')) throw std::runtime_error("Callback ABI result size differs");\n')
                 + 'static_assert(sizeof(self.target) == sizeof(self.core.entry), "Unsupported native code pointer ABI");\n'
                 'std::memcpy(&self.target, &self.core.entry, sizeof(self.target)); *output = result.release();',
                 'Own an independent typed closure. Keep it and callback user state alive until all native users stop.', public, ['function'])
            emit(root + '_from_address', [('void *', 'address')],
                 'if (address) { ' + native + ' target; static_assert(sizeof(target) == sizeof(address), "Unsupported code pointer ABI"); std::memcpy(&target, &address, sizeof(target)); *output = ' + name + '_view(target); }',
                 'Own a callable view of a native procedure address; the procedure owner stays borrowed.', public)
            emit(root + '_free', [(public, 'self')], 'delete self;',
                 'Release an owned closure/view after native users stop; NULL is accepted. Never free a borrowed callback argument.')
            params = [(public, 'self')] + [(t.public(input=True), 'argument_' + str(i)) for i, t in enumerate(args)]
            call = 'self->target(' + ', '.join(t.to_native('argument_' + str(i)) for i, t in enumerate(args)) + ')'
            emit(root + '_call', params, (call + ';' if is_void else result_body(ret, call)),
                 'Call a borrowed/native or owned callback using its native preconditions; by-value record copies are owned, native pointer ownership is unchanged.',
                 None if is_void else ret.public(), ['self'] + ['argument_' + str(i) for i, t in enumerate(args) if t.required('')])
            emit(root + '_clear_error', [(public, 'self')], 'self->core.error.store(AILO_OK);',
                 'Clear the sticky callback exception status after inspecting it.', validate=['self'])
            H.append('ailo_status ' + root + '_error(' + public + 'self, ailo_error *error);')
            C.append('extern "C" __attribute__((visibility("default"))) ailo_status ' + root + '_error(' + public + 'self, ailo_error *error) { if (!self) return ailo_error_set(error, AILO_INVALID_ARGUMENT, "Callback is required"); const auto status = self->core.error.load(); return ailo_error_set(error, status, status == AILO_OK ? "" : "Native callback raised an exception or returned an invalid result"); }')
            EXPORTS.append(root + '_error')
        declarations.extend(H[start_h:]); del H[start_h:]
        definitions.extend(C[start_c:]); del C[start_c:]
    return declarations, definitions


def allocation(name, native, public):
    emit(name + '_array_new', [('size_t', 'count')],
         'if (count > static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max()) / sizeof(' + native + ')) throw ailo::detail::native_range();\n'
         'if (count) *output = reinterpret_cast<' + public + ' *>(new ' + native + '[count]());',
         'Allocate zero-initialized native-backed elements; caller owns the result and count.', public + ' *')
    emit(name + '_array_free', [(public + ' *', 'self')],
         'delete[] reinterpret_cast<' + native + ' *>(self);',
         'Release only a facade element allocation or by-value result; NULL is accepted.')
    emit(name + '_array_at', [(public + ' *', 'self'), ('size_t', 'count'), ('size_t', 'index')],
         'if (index >= count) throw ailo::detail::native_range();\n'
         '*output = reinterpret_cast<' + public + ' *>(reinterpret_cast<' + native + ' *>(self) + index);',
         'Borrow an element using the actual caller-declared extent; never free the element.', public + ' *', ['self'])


def fields(record):
    nested = iter(record['nested'])
    for f in record['fields']:
        if '(unnamed ' in f['type'] or '(anonymous ' in f['type']:
            child = next(nested)
            prefix = f['name'] + '.' if f['name'] else ''
            for item in child['fields']:
                yield prefix + item['name'], item['type']
        else:
            yield f['name'], f['type']


def generate():
    for element in ['int64_t', 'uint64_t', 'bool']:
        STORAGES[element] = {'name': 'ailo_storage_' + element, 'element': element}
    for enum in DATA['enums']:
        values = ['AILO_' + v['name'] + ' = ' + str(v['value']) for v in enum['values']]
        H.append('enum ailo_' + enum['name'] + ' {\n' + ',\n'.join(values) + '\n};')
        for value in enum['values']:
            C.append('static_assert(static_cast<int>(' + value['name'] + ') == ' + str(value['value']) + ', "Native enum drift");')
        COVERAGE['enums'].append(enum['name'])

    for alias in DATA['typedefs']:
        name = alias['name']
        if name == 'ggml_guid':
            H.append('typedef ailo_u8 ailo_ggml_guid[16];')
            COVERAGE['typedefs'].append(name)
            continue
        try:
            t = Type(alias['type'])
            if t.kind == 'callback':
                t.callback['aliases'].append(name)
            public = t.public()
            if not t.depth and t.kind == 'record':
                public = t.public_base
            H.append('typedef ' + public + ' ailo_' + name + ';')
            COVERAGE['typedefs'].append(name)
        except Gap as e:
            GAPS.append({'kind': 'typedef', 'name': name, 'reason': str(e)})

    for record in DATA['records']:
        name = record['name']
        native = name if name == 'ggml_bf16_t' else 'struct ' + name
        public = 'struct ailo_' + name
        allocation('ailo_' + name, native, public)
        COVERAGE['records'].append(name)
        for field, text in fields(record):
            identity = name + '.' + field
            try:
                match = re.fullmatch(r'(.+)\[(\d+)\]', text)
                extent = int(match[2]) if match else None
                t = Type(match[1] if match else text)
                if t.kind == 'callback':
                    t.callback['aliases'].append(name + '_' + field.replace('.', '_') + '_callback')
                method = 'ailo_' + name + '_' + field.replace('.', '_')
                params = [('const ' + public + ' *', 'self')]
                expression = 'reinterpret_cast<const ' + native + ' *>(self)->' + field
                if extent is not None:
                    params.append(('size_t', 'index'))
                    expression += '[index]'
                guard = ('if (index >= ' + str(extent) + ') throw ailo::detail::native_range();\n') if extent else ''
                emit(method + '_get', params, guard + result_body(t, expression),
                     'Read the native field with explicit conversion; record-value copies are owned, pointer views borrowed.', t.public(), ['self'])
                params[0] = (public + ' *', 'self')
                params.append((t.public(input=True), 'value'))
                dst = expression.replace('reinterpret_cast<const ', 'reinterpret_cast<')
                body = dst + ' = ' + t.to_native('value') + ';'
                if t.kind == 'map' and not t.depth:
                    body = t.base + ' copy = ' + t.to_native('value') + ';\n' + dst + '.swap(copy);'
                emit(method + '_set', params, guard + body,
                     'Mutate the native field; referenced pointer owners must remain alive.',
                     validate=['self'] + (['value'] if t.required('value') else []))
                COVERAGE['fields'].append(identity)
            except Gap as e:
                GAPS.append({'kind': 'field', 'name': identity, 'reason': str(e)})

    for function in DATA['functions']:
        name = function['name']
        if name in MANUAL_FUNCTIONS:
            COVERAGE['functions'].append(name)
            continue
        try:
            if function['variadic']:
                raise Gap('variadic adapter pending')
            ret = Type(function['type'].split('(', 1)[0].strip())
            native_args = []
            params = []
            required = []
            before, after, prelude = [], [], ''
            for index, arg in enumerate(function['parameters']):
                t = Type(arg['type'])
                param = 'argument_' + str(index)
                params.append((t.public(input=True), param))
                if t.kind == 'callback' and t.depth == 2:
                    before.append(t.base + ' callback_' + str(index) + ' = nullptr;')
                    native_args.append('&callback_' + str(index))
                    after.append('*' + param + ' = ' + t.name + '_view(callback_' + str(index) + ');')
                    required.append(param)
                    prelude += 'if (' + param + ') *' + param + ' = nullptr;\n'
                else:
                    native_args.append(t.to_native(param))
                if t.required(param):
                    required.append(param)
            call = name + '(' + ', '.join(native_args) + ')'
            body = call + ';' if ret.base == 'void' and not ret.depth else result_body(ret, call)
            body = '\n'.join(before + [body] + after)
            if name == 'llama_backend_init':
                body = 'ailo::detail::backend_acquire();'
            elif name == 'llama_backend_free':
                body = 'ailo::detail::backend_release();'
            # Optional backend declarations are installed even when disabled.
            backend = BACKEND_FUNCTIONS.get(name)
            if backend:
                condition = 'AILO_NATIVE_' + backend.upper()
                body = '#if defined(' + condition + ')\n' + body + '\n#else\nthrow ailo::detail::native_unsupported();\n#endif'
                # Keep controllable generated code warning-clean in disabled branches.
                body = ''.join('(void)' + p + ';\n' for _, p in params) + body
            emit('ailo_' + name, params, body,
                 'Invoke the native API with its preconditions; native results and ownership are preserved separately from bridge status.',
                 None if ret.base == 'void' and not ret.depth else ret.public(), required, prelude)
            COVERAGE['functions'].append(name)
        except Gap as e:
            GAPS.append({'kind': 'function', 'name': name, 'reason': str(e)})

    # Callback signatures also expose native-backed pointer/word-pair arrays.
    discovered = set()
    while len(discovered) < len(CALLBACKS):
        for key, cb in list(CALLBACKS.items()):
            if key not in discovered:
                discovered.add(key)
                Type(cb['return'])
                for arg in cb['arguments']:
                    Type(arg)
    # Field/functor processing discovers all native element storage types.
    handled = set()
    while len(handled) < len(STORAGES):
        for key, storage in list(STORAGES.items()):
            if key in handled:
                continue
            handled.add(key)
            t = Type(key)
            name = storage['name']
            public = 'struct ' + name
            native_alias = name + '_element'
            C.append('using ' + native_alias + ' = ' + key + ';')
            allocation(name, native_alias, public)
            params = [('const ' + public + ' *', 'self'), ('size_t', 'count'), ('size_t', 'index')]
            expression = 'reinterpret_cast<const ' + native_alias + ' *>(self)[index]'
            guard = 'if (index >= count) throw ailo::detail::native_range();\n'
            emit(name + '_get', params, guard + result_body(t, expression),
                 'Read a native-backed element; actual extent and owner lifetime are caller obligations.', t.public(), ['self'])
            params[0] = (public + ' *', 'self')
            params.append((t.public(input=True), 'value'))
            emit(name + '_set', params,
                 guard + 'reinterpret_cast<' + native_alias + ' *>(self)[index] = ' + t.to_native('value') + ';',
                 'Write an explicitly converted native element within the caller-declared extent.',
                 validate=['self'] + (['value'] if t.required('value') else []))

    allocation('ailo_backend_memory_map', SPECIAL_MAP, MAP_PUBLIC)
    emit('ailo_backend_memory_map_size', [('const ' + MAP_PUBLIC + ' *', 'self')],
         '*output = reinterpret_cast<const ' + SPECIAL_MAP + ' *>(self)->size();',
         'Read the number of device memory entries.', 'size_t', ['self'])
    emit('ailo_backend_memory_map_set', [(MAP_PUBLIC + ' *', 'self'), ('struct ailo_ggml_backend_device *', 'device'), ('size_t', 'bytes')],
         '(*reinterpret_cast<' + SPECIAL_MAP + ' *>(self))[reinterpret_cast<ggml_backend_dev_t>(device)] = bytes;',
         'Insert or replace a device memory entry; the device remains borrowed.', validate=['self'])
    emit('ailo_backend_memory_map_get', [('const ' + MAP_PUBLIC + ' *', 'self'), ('size_t', 'index'), ('struct ailo_ggml_backend_device **', 'device')],
         'const auto & map = *reinterpret_cast<const ' + SPECIAL_MAP + ' *>(self);\n'
         'if (index >= map.size()) throw ailo::detail::native_range();\n'
         'auto entry = map.begin(); std::advance(entry, index);\n'
         '*device = reinterpret_cast<struct ailo_ggml_backend_device *>(entry->first);\n*output = entry->second;',
         'Borrow the indexed device and read its byte count; map ordering follows native device keys.', 'size_t', ['self', 'device'],
         prelude='if (device) *device = nullptr;\n')
    for macro in DATA['macros']:
        name, value = macro['name'], macro['value']
        if name in NONCAPABILITY_MACROS:
            continue
        if name in MACRO_ADAPTERS:
            COVERAGE['macros'].append(name)
        elif not macro['parameters'] and re.fullmatch(r'(?:-?\d+(?:\.\d+)?[uUfFlL]*|0x[0-9A-Fa-f]+[uUlL]*|"[^"\n]*")', value):
            H.append('#define AILO_' + name + ' ' + value)
            COVERAGE['macros'].append(name)
        elif not macro['parameters'] and re.fullmatch(r'(?:GGML|GGUF|LLAMA|MTMD)_[A-Z0-9_]+', value):
            H.append('#define AILO_' + name + ' AILO_' + value)
            COVERAGE['macros'].append(name)
        else:
            GAPS.append({'kind': 'macro', 'name': name, 'reason': 'macro classification/adapter pending'})


def outputs():
    generate()
    callback_h, callback_c = generate_callbacks()
    preamble = ('/* Generated from the pinned upstream public API inventory.\n'
                ' * Regenerate with scripts/generate_native.py; do not edit manually.\n'
                ' * See docs/facade-bindings.md for ownership and bridge semantics. */\n')
    forward = ['struct ailo_' + name + ';' for name in sorted(TAGS)]
    forward += ['struct ' + s['name'] + ';' for s in STORAGES.values()]
    forward.append(MAP_PUBLIC + ';')
    forward += ['struct ' + cb['name'] + ';' for cb in CALLBACKS.values()]
    enum_count = len(DATA['enums'])
    header = (preamble + '#ifndef AILO_NATIVE_H\n#define AILO_NATIVE_H\n'
              '#include "ailo/base.h"\n#include <stddef.h>\n#include <stdio.h>\n'
              '#include "ailo/native_variadic.h"\n'
              '#include "ailo/native_macros.h"\n'
              '#ifdef __cplusplus\nextern "C" {\n#endif\n'
              + '\n'.join(forward + H[:enum_count] + callback_h + H[enum_count:]) + '\n#ifdef __cplusplus\n}\n#endif\n#endif\n')
    includes = ['#include "' + h + '"' for h in DATA['headers']]
    # Deprecations are deliberately part of the complete public facade.
    cpp = (preamble + '#include "ailo/native.h"\n#include "ailo_native_internal.hpp"\n#include "ailo_backend_internal.hpp"\n#include "ailo_callback_internal.hpp"\n'
           '#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wdeprecated-declarations"\n'
           + '\n'.join(includes + callback_c + C) + '\n#pragma GCC diagnostic pop\n')
    manifest = {'inventory_sha256': hashlib.sha256((ROOT / 'docs/upstream-api.json').read_bytes()).hexdigest(),
                'implemented': COVERAGE, 'gaps': GAPS, 'exports': sorted(EXPORTS),
                'manual_function_exports': sorted('ailo_' + n for n in MANUAL_FUNCTIONS),
                'noncapability_macros': {n: 'Native header guard; project headers have their own guards' for n in sorted(NONCAPABILITY_MACROS)},
                'constants': {'GGML_TENSOR_SIZE': 'AILO_GGML_TENSOR_SIZE / ailo_ggml_tensor_native_size'},
                'backend_conditions': BACKEND_FUNCTIONS}
    for constant in DATA.get('constants', []):
        if constant['name'] not in manifest['constants']:
            GAPS.append({'kind': 'constant', 'name': constant['name'], 'reason': 'constant adapter pending'})
    tests = record_tests(preamble, includes)
    return {'include/ailo/native.h': header, 'src/ailo_native.cpp': cpp,
            'tests/native_records.cpp': tests,
            'docs/facade-coverage.json': json.dumps(manifest, indent=2) + '\n',
            'cmake/ailo-native.exports': '\n'.join(sorted(EXPORTS)) + '\n'}


def record_tests(preamble, includes):
    lines = [preamble, '#include "ailo/native.h"', '#include "ailo_integer_bridge.hpp"',
             '#include <cstdio>', '#include <limits>', '#include <cstring>', '#include <type_traits>'] + includes
    lines += ['static int callback_tag = 0;',
              'template <typename Signature, int Tag> struct field_callback;',
              'template <int Tag, typename R, typename... Args> struct field_callback<R (*)(Args...), Tag> { static R call(Args...) { callback_tag = Tag; if constexpr (!std::is_void_v<R>) return R{}; } };',
              '#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\\n", __FILE__, __LINE__, #x); return 1; } } while (0)',
              'int main() {', 'ailo_error error = {};', 'alignas(64) unsigned char address[64] = {};',
              'size_t tested = 0;']
    callback_index = 0
    for record in DATA['records']:
        name = record['name']
        native = name if name == 'ggml_bf16_t' else 'struct ' + name
        root = 'ailo_' + name
        lines += ['{', 'struct ' + root + ' * self = nullptr;',
                  'CHECK(' + root + '_array_new(2, &self, &error) == AILO_OK && self);',
                  'auto native = reinterpret_cast<' + native + ' *>(self);', '(void)native;']
        for field, text in fields(record):
            identity = name + '.' + field
            if identity not in COVERAGE['fields']:
                continue
            array = re.fullmatch(r'(.+)\[(\d+)\]', text)
            t = Type(array[1] if array else text)
            method = root + '_' + field.replace('.', '_')
            index = (int(array[2]) - 1) if array else None
            args = 'self, ' + (str(index) + ', ' if array else '')
            actual = 'native->' + field + ('[' + str(index) + ']' if array else '')
            lines.append('{')
            if t.kind == 'map' and not t.depth:
                lines += ['struct ailo_backend_memory_map * value = nullptr, * result = nullptr;',
                          'CHECK(ailo_backend_memory_map_array_new(1, &value, &error) == AILO_OK);',
                          'CHECK(ailo_backend_memory_map_set(value, nullptr, 1234, &error) == AILO_OK);',
                          'CHECK(' + method + '_set(' + args + 'value, &error) == AILO_OK);',
                          'CHECK(' + method + '_get(' + args + '&result, &error) == AILO_OK);',
                          'size_t count = 0; CHECK(ailo_backend_memory_map_size(result, &count, &error) == AILO_OK && count == 1);',
                          'CHECK(' + actual + '.at(nullptr) == 1234);',
                          'CHECK(ailo_backend_memory_map_array_free(result, &error) == AILO_OK);',
                          'CHECK(ailo_backend_memory_map_array_free(value, &error) == AILO_OK);']
            elif t.kind == 'record' and not t.depth:
                child = 'ailo_' + t.name
                lines += ['struct ' + child + ' * value = nullptr, * result = nullptr;',
                          'CHECK(' + child + '_array_new(1, &value, &error) == AILO_OK);']
                if t.name == 'ggml_backend_dev_caps':
                    lines += ['CHECK(ailo_ggml_backend_dev_caps_events_set(value, 1, &error) == AILO_OK);']
                    member, expected = 'events', 'true'
                elif t.name == 'mtmd_helper_video_init_params':
                    lines += ['CHECK(ailo_mtmd_helper_video_init_params_fps_target_set(value, 29.5f, &error) == AILO_OK);']
                    member, expected = 'fps_target', '29.5f'
                else:
                    raise Gap('record test needs value semantics for ' + t.name)
                lines += ['CHECK(' + method + '_set(' + args + 'value, &error) == AILO_OK);',
                          'CHECK(' + method + '_get(' + args + '&result, &error) == AILO_OK && result != value);',
                          'CHECK(' + actual + '.' + member + ' == ' + expected + ');',
                          'CHECK(reinterpret_cast<' + t.base + ' *>(result)->' + member + ' == ' + expected + ');',
                          'CHECK(' + child + '_array_free(result, &error) == AILO_OK);',
                          'CHECK(' + child + '_array_free(value, &error) == AILO_OK);']
            elif t.kind == 'callback':
                callback_index += 1
                cbroot = 'ailo_' + sorted(t.callback['aliases'])[0]
                lines += [t.public() + ' value = nullptr;', t.public() + ' result = nullptr;',
                          'auto procedure = &field_callback<decltype(' + actual + '), ' + str(callback_index) + '>::call;',
                          'void * entry = nullptr; static_assert(sizeof(entry) == sizeof(procedure), "Native callback code pointer ABI");',
                          'std::memcpy(&entry, &procedure, sizeof(entry));',
                          'CHECK(' + cbroot + '_from_address(entry, &value, &error) == AILO_OK);',
                          'CHECK(' + method + '_set(' + args + 'value, &error) == AILO_OK);',
                          'CHECK(' + actual + ' == procedure);',
                          'CHECK(' + method + '_get(' + args + '&result, &error) == AILO_OK && result);']
                call_args = ['result']
                for i, argument in enumerate(t.callback['arguments']):
                    argument_type = Type(argument)
                    lines += ['using argument_type_' + str(i) + ' = ' + argument_type.public(input=True) + ';',
                              'argument_type_' + str(i) + ' argument_' + str(i) + '{};']
                    call_args.append('argument_' + str(i))
                ret = Type(t.callback['return'])
                if ret.native != 'void':
                    lines += ['using callback_result_type = ' + ret.public() + ';',
                              'callback_result_type callback_result{};']
                    call_args.append('&callback_result')
                lines += ['callback_tag = 0;',
                          'CHECK(' + cbroot + '_call(' + ', '.join(call_args + ['&error']) + ') == AILO_OK);',
                          'CHECK(callback_tag == ' + str(callback_index) + ');']
                if ret.kind == 'record' and not ret.depth:
                    lines.append('CHECK(ailo_' + ret.name + '_array_free(callback_result, &error) == AILO_OK);')
                lines += ['CHECK(' + method + '_set(' + args + 'nullptr, &error) == AILO_OK && !' + actual + ');',
                          'CHECK(' + cbroot + '_free(value, &error) == AILO_OK);',
                          'CHECK(' + cbroot + '_free(result, &error) == AILO_OK);']
            else:
                public = t.public()
                if t.depth:
                    sample = 'reinterpret_cast<' + public + '>(address)'
                elif t.base == 'int64_t':
                    sample = 'ailo_i64{0x80000000U, 17}'
                elif t.base == 'uint64_t':
                    sample = 'ailo_u64{0xf234abcdU, 0xabcdef12U}'
                elif t.kind == 'enum':
                    sample = 'AILO_' + ENUMS[t.name]['values'][0]['name']
                elif t.base == 'float':
                    sample = '0.25f'
                elif t.base == 'double':
                    sample = '0.5'
                elif t.base == 'bool':
                    sample = '17'
                else:
                    sample = '97'
                lines += [public + ' value = ' + sample + ';', public + ' result = {};',
                          'CHECK(' + method + '_set(' + args + 'value, &error) == AILO_OK);',
                          'CHECK(' + method + '_get(' + args + '&result, &error) == AILO_OK);']
                if t.base == 'bool' and not t.depth:
                    lines += ['CHECK(result == 1 && ' + actual + ');']
                elif t.base in {'int64_t', 'uint64_t'} and not t.depth:
                    lines += ['CHECK(result.hi == value.hi && result.lo == value.lo);',
                              'CHECK(' + actual + ' == ' + t.to_native('value') + ');']
                else:
                    lines += ['CHECK(result == value);', 'CHECK(' + actual + ' == ' + t.to_native('value') + ');']
            if array:
                lines += ['CHECK(' + method + '_get(self, ' + array[2] + ', ' + ('&result' if t.public() != 'void' else 'nullptr') + ', &error) == AILO_OUT_OF_RANGE);']
            lines += ['++tested;', '}']
        lines += ['struct ' + root + ' * element = nullptr;',
                  'CHECK(' + root + '_array_at(self, 2, 1, &element, &error) == AILO_OK);',
                  'CHECK(reinterpret_cast<' + native + ' *>(element) == native + 1);',
                  'CHECK(' + root + '_array_at(self, 2, 2, &element, &error) == AILO_OUT_OF_RANGE && !element);',
                  'CHECK(' + root + '_array_new(std::numeric_limits<size_t>::max(), &element, &error) == AILO_OUT_OF_RANGE && !element);',
                  'CHECK(' + root + '_array_free(self, &error) == AILO_OK);',
                  'CHECK(' + root + '_array_free(nullptr, &error) == AILO_OK);', '}']
    lines += ['CHECK(tested == ' + str(len(COVERAGE['fields'])) + ');',
              'std::printf("Native record fields verified: %zu\\n", tested);', 'return 0;', '}']
    return '\n'.join(lines) + '\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--require-complete', action='store_true')
    args = parser.parse_args()
    result = outputs()
    work = ROOT / 'build/generator'
    work.mkdir(parents=True, exist_ok=True)
    for relative, content in result.items():
        path = ROOT / relative
        if path.suffix in {'.h', '.cpp'}:
            content = subprocess.check_output(['clang-format', '--assume-filename=' + str(path)], input=content, text=True)
        if args.check:
            if not path.exists() or path.read_text() != content:
                raise SystemExit('Generated facade differs: ' + relative)
        elif not path.exists() or path.read_text() != content:
            path.parent.mkdir(parents=True, exist_ok=True)
            temporary = work / path.name
            temporary.write_text(content)
            temporary.replace(path)
    print('Functions:', len(COVERAGE['functions']), '/', len(DATA['functions']),
          'fields:', len(COVERAGE['fields']), 'explicit gaps:', len(GAPS))
    if args.require_complete and GAPS:
        raise SystemExit('Complete facade gate failed: unsupported declarations remain in docs/facade-coverage.json')
