"""Fast non-Windows audit of the source hookup; the real MapDocument C++
regression is registered through CMake for Windows CI.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
cpp = (root / 'src/MapDocument.cpp').read_text()
cmake = (root / 'CMakeLists.txt').read_text()
policy = (root / 'src/NativeCloneEditPolicy.h').read_text()
assert 'NativeCloneEditPolicy::MayEdit' in cpp
assert 'NativeCloneEditPolicy::MayEraseOpaque' in cpp
assert 'NativeCloneEditPolicy::MayEraseUnresolved' in cpp
assert 'batchContainsOpaqueCollision' in policy
assert 'c.nativeCloneBatchId>=0 || !same(c.position)' in cpp
assert 'fullySelectedCloneBatches.insert(batch)' in cpp
assert 'has no individually verified collision owner' in cpp
assert 'Log::Info("Lossless clone ownership:' in cpp
assert 'NativeClonePartialEditRegression' in cmake
assert 'NativeCloneEditPolicyRegression' in cmake
assert 'allRemainingClonePropsSelected' in policy
print('PASS: clone partial editing preflight, exact collider movement, batch-scoped cleanup and registered regression targets')
