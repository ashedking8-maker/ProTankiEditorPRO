from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def test_lossless_full_map_clipboard_bypasses_3ds_reinterpretation():
    ui=(ROOT/'src/EditorUi.cpp').read_text(encoding='utf-8')
    hdr=(ROOT/'src/EditorUi.h').read_text(encoding='utf-8')
    doc=(ROOT/'src/MapDocument.cpp').read_text(encoding='utf-8')
    mh=(ROOT/'src/MapDocument.h').read_text(encoding='utf-8')
    assert 'clipboardHasNativeStaticBundle_' in hdr
    assert 'clipboardCollisionPlanes_=map.CollisionPlanes()' in ui
    assert 'clipboardCollisionBoxes_=map.CollisionBoxes()' in ui
    assert 'clipboardCollisionTriangles_=map.CollisionTriangles()' in ui
    assert 'AppendLosslessNativeStaticClone' in ui
    assert '3DS reinterpretation bypassed' in ui
    assert 'losslessNativeClone' in mh
    assert 'originalXml' in mh
    assert 'Lossless collision clone contains invalid source' in doc
    assert 'opaque collision data cannot be orphaned' in doc

def test_tunnel2_source_anchor_is_verified_and_fixture_is_present():
    native=(ROOT/'src/NativeCollisionImport.h').read_text(encoding='utf-8')
    assert 'stem=="tunnel_2"' in native
    assert 'nodes[0].name=="Box02"' in native
    assert (ROOT/'tests/fixtures/esplanade/tunnel_2.3ds').is_file()

def test_lossless_clone_regression_is_registered():
    cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
    assert 'LosslessNativeCloneRegression' in cmake
    assert 'LosslessFullStaticMapClone' in cmake
    assert (ROOT/'tests/lossless_native_clone_regression.cpp').is_file()
    assert (ROOT/'tests/fixtures/lossless_clone/source.xml').is_file()
