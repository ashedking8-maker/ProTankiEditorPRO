import importlib.util
import struct
import tempfile
from pathlib import Path
spec=importlib.util.spec_from_file_location('audit_native_object',Path(__file__).resolve().parents[1]/'tools/audit_native_object.py')
a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
def chunk(k,data):return struct.pack('<HI',k,len(data)+6)+data
with tempfile.TemporaryDirectory() as temp:
    root=Path(temp)
    model=root/'sample.3ds'
    model.write_bytes(chunk(0x4D4D,chunk(0x3D3D,chunk(0x4000,b'Box01\0'+chunk(0x4100,chunk(0x4110,b'\x00\x00'))))))
    rep=a.read_3ds(model)
    assert rep['object_names']==['Box01'] and rep['helper_candidates']==['Box01']
    model.write_bytes(model.read_bytes()[:-1])
    try:a.read_3ds(model);assert False,'corrupt 3ds accepted'
    except ValueError:pass
    lib=root/'library.xml'
    lib.write_text('<library name="demo"><prop-group name="default"><prop name="Test"><mesh file="sample.3ds"><texture name="a" diffuse-map="t.jpg"/></mesh></prop></prop-group></library>')
    assert a.library_report(lib)['props'][0]['textures'][0]['diffuse-map']=='t.jpg'
    before=root/'before.xml';after=root/'after.xml'
    before.write_text('<map><static-geometry><prop/></static-geometry><collision-geometry><collision-box/><collision-plane/></collision-geometry></map>')
    after.write_text('<map><static-geometry><prop/><prop/></static-geometry><collision-geometry><collision-box/><collision-plane/><collision-triangle/></collision-geometry></map>')
    delta=a.report(lib,None,before,after)['map_comparison']
    assert delta['prop_delta']==1 and delta['collision_deltas']['collision-triangle']==1
print('PASS: native object evidence auditor (3DS chunks, malformed chunks, library XML, map deltas)')
