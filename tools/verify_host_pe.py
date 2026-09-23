#!/usr/bin/env python3
"""Verify linked PE/VERSIONINFO bytes, independently of the generated RC file.

Format references: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
and https://learn.microsoft.com/en-us/windows/win32/menurc/vs-versioninfo .
This is deliberately a bounded reader for our development DLLs, not a PE loader.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def require(condition, message):
    if not condition:
        raise ValueError(message)


class Reader:
    def __init__(self, data):
        self.data = data

    def take(self, offset, size):
        require(offset >= 0 and size >= 0 and offset + size <= len(self.data),
                'truncated or out-of-bounds PE/resource data')
        return self.data[offset:offset + size]

    def unpack(self, fmt, offset):
        return struct.unpack(fmt, self.take(offset, struct.calcsize(fmt)))

    def u16(self, offset):
        return self.unpack('<H', offset)[0]

    def u32(self, offset):
        return self.unpack('<I', offset)[0]


def version_block(data, offset=0, limit=None, depth=0):
    reader = Reader(data)
    limit = len(data) if limit is None else limit
    require(depth < 8, 'VERSIONINFO nesting too deep')
    size, length, kind = reader.unpack('<HHH', offset)
    end = offset + size
    require(size >= 8 and end <= limit and kind in (0, 1), 'invalid VERSIONINFO block')
    cursor = offset + 6
    key_start = cursor
    while cursor + 2 <= end and reader.u16(cursor):
        cursor += 2
    require(cursor + 2 <= end, 'unterminated VERSIONINFO key')
    key = reader.take(key_start, cursor - key_start).decode('utf-16le')
    cursor = (cursor + 2 + 3) & ~3
    value_bytes = length * (2 if kind == 1 else 1)
    require(cursor + value_bytes <= end, 'VERSIONINFO value exceeds block')
    value = reader.take(cursor, value_bytes)
    cursor = (cursor + value_bytes + 3) & ~3
    children = {}
    while cursor < end:
        require(end - cursor >= 8, 'trailing VERSIONINFO garbage')
        child, next_offset = version_block(data, cursor, end, depth + 1)
        require(child['key'] not in children, 'duplicate VERSIONINFO key')
        children[child['key']] = child
        cursor = (next_offset + 3) & ~3
    return {'key': key, 'kind': kind, 'value': value, 'children': children}, end


def inspect(data):
    reader = Reader(data)
    require(reader.take(0, 2) == b'MZ', 'missing DOS signature')
    pe = reader.u32(0x3c)
    require(reader.take(pe, 4) == b'PE\0\0', 'missing PE signature')
    machine, count, stamp = reader.unpack('<HHI', pe + 4)
    optional_size, flags = reader.unpack('<HH', pe + 20)
    optional = pe + 24
    magic = reader.u16(optional)
    require(magic in (0x10b, 0x20b), 'unsupported optional header')
    directory = optional + (96 if magic == 0x10b else 112)
    require(optional_size >= directory - optional + 24, 'resource directory header missing')
    require(reader.u32(directory - 4) >= 3, 'resource data directory missing')
    resource_rva, resource_size = reader.unpack('<II', directory + 16)
    require(0 < count <= 96 and resource_size > 0, 'missing sections/resources')
    sections = []
    for index in range(count):
        section = optional + optional_size + index * 40
        virtual_size, rva, raw_size, raw = reader.unpack('<IIII', section + 8)
        sections.append((rva, raw_size, raw))

    def file_offset(rva, size):
        matches = [raw + rva - start for start, length, raw in sections
                   if start <= rva and rva + size <= start + length]
        require(len(matches) == 1, 'RVA outside a unique file-backed section')
        reader.take(matches[0], size)
        return matches[0]

    resource = Reader(reader.take(file_offset(resource_rva, resource_size), resource_size))

    def entries(offset):
        named, ids = resource.unpack('<HH', offset + 12)
        require(named + ids <= 64, 'resource directory too large')
        return [resource.unpack('<II', offset + 16 + i * 8) for i in range(named + ids)]

    root = dict(entries(0))
    require(16 in root and root[16] & 0x80000000, 'RT_VERSION resource missing')
    ids = entries(root[16] & 0x7fffffff)
    require(len(ids) == 1 and ids[0][0] == 1 and ids[0][1] & 0x80000000,
            'expected exactly VERSIONINFO resource ID 1')
    languages = entries(ids[0][1] & 0x7fffffff)
    require(len(languages) == 1 and not languages[0][1] & 0x80000000,
            'expected one VERSIONINFO language leaf')
    rva, length, codepage, reserved = resource.unpack('<IIII', languages[0][1])
    require(reserved == 0 and 0 < length <= 65535, 'invalid VERSIONINFO data entry')
    block, _ = version_block(reader.take(file_offset(rva, length), length))
    require(block['key'] == 'VS_VERSION_INFO' and block['kind'] == 0,
            'invalid VS_VERSION_INFO root')
    require(len(block['value']) == 52, 'invalid VS_FIXEDFILEINFO length')
    fixed = struct.unpack('<13I', block['value'])
    children = block['children']
    require(set(children) == {'StringFileInfo', 'VarFileInfo'}, 'unexpected VERSIONINFO tree')
    tables = children['StringFileInfo']['children']
    require(set(tables) == {'040904b0'}, 'unexpected version string locale/codepage')
    strings = {}
    for name, item in tables['040904b0']['children'].items():
        require(item['kind'] == 1 and not item['children'] and item['value'].endswith(b'\0\0'),
                'invalid version string')
        strings[name] = item['value'][:-2].decode('utf-16le')
    translation = children['VarFileInfo']['children']
    require(set(translation) == {'Translation'}, 'missing version translation')
    require(translation['Translation']['value'] == struct.pack('<HH', 0x409, 1200),
            'unexpected version translation')
    return dict(machine=machine, optional_magic=magic, characteristics=flags,
                timestamp=stamp, os_version=list(reader.unpack('<HH', optional + 40)),
                subsystem_version=list(reader.unpack('<HH', optional + 48)),
                subsystem=reader.u16(optional + 68), fixed=list(fixed), strings=strings,
                resource_language=languages[0][0], resource_codepage=codepage)


def verify(data, configuration):
    actual = inspect(data)
    target = configuration['target']
    machine, magic = {'x86': (0x14c, 0x10b), 'x64': (0x8664, 0x20b)}[target['arch']]
    require((actual['machine'], actual['optional_magic']) == (machine, magic),
            'PE architecture does not match target')
    require(actual['characteristics'] & 0x2002 == 0x2002, 'image must be executable DLL')
    require(actual['timestamp'] == 0, 'PE timestamp is not reproducible')
    require(actual['os_version'] == configuration['subsystem'] and
            actual['subsystem_version'] == configuration['subsystem'], 'PE OS/subsystem version mismatch')
    require(actual['subsystem'] in (2, 3), 'invalid Windows subsystem')
    version = configuration['version']
    parts = [int(p) for p in version.split('.')]
    ms, ls = (parts[0] << 16) | parts[1], (parts[2] << 16) | parts[3]
    fixed = [0xfeef04bd, 0x10000, ms, ls, ms, ls, 0x3f, 2,
             4 if target['os'] == 'winme' else 0x40004, 2, 0, 0, 0]
    require(actual['fixed'] == fixed, 'VS_FIXEDFILEINFO mismatch')
    expected = {'FileDescription': 'IEWebkit development document host',
                'FileVersion': version, 'ProductName': 'IEWebkit Host Development',
                'ProductVersion': version, 'OriginalFilename': 'iewebkit-host.dll',
                'Variant': target['artifact_id'], 'InternetExplorer': target['ie'],
                'OperatingSystem': target['os'], 'ContentArchitecture': target['arch'],
                'SecurityMode': target['mode'], 'Certification': 'Unverified development component'}
    require(actual['strings'] == expected, 'linked VERSIONINFO strings do not match selected target')
    return dict(schema_version=1, passed=True, engine_included=False, guest_verified=False,
                release_eligible=False, artifact_sha256=hashlib.sha256(data).hexdigest(), pe=actual)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('artifact_directory', type=Path)
    args = parser.parse_args()
    metadata = json.loads((args.artifact_directory / 'build.json').read_text())
    result = verify((args.artifact_directory / 'iewebkit-host.dll').read_bytes(), metadata['configuration'])
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
