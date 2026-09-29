#!/usr/bin/env python3
"""Run scalar native-format regressions on Linux without the Windows/D3D SDK.

The only shim is DirectX::XMFLOAT3 (three floats). No importer, parser, serializer
or math function is mocked. Windows CI should use the normal CMake/CTest suite.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
TESTS = (
    ('gameplay_selection_regression.cpp', ()),
    ('native_3ds_writer_regression.cpp', ()),
    ('native_visual_metadata_regression.cpp', ()),
    ('native_terrain_delta_regression.cpp', ('tests/fixtures/native_helpers',)),
    ('native_swf_compatibility_regression.cpp', ('tests/fixtures',)),
)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='g++')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='ptpro-native-tests-') as work:
        work = Path(work)
        shim = work / 'shim'
        shim.mkdir()
        (shim / 'DirectXMath.h').write_text(
            '#pragma once\nnamespace DirectX { struct XMFLOAT3 { float x{},y{},z{}; }; }\n', encoding='utf-8')
        for name, fixture_args in TESTS:
            output = work / Path(name).stem
            subprocess.run([args.compiler, '-std=c++20', '-O2', '-DNDEBUG', '-Wall', '-Wextra',
                            '-I', str(shim), '-I', str(ROOT / 'src'), '-I', str(ROOT / 'tests'),
                            str(ROOT / 'tests' / name), '-o', str(output)], check=True)
            subprocess.run([str(output), *[str(ROOT / p) for p in fixture_args]], check=True)
    print(f'PASS: {len(TESTS)} scalar C++ regression programs; Windows/Assimp/XML tests are separate.')

if __name__ == '__main__':
    main()
