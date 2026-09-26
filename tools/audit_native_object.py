#!/usr/bin/env python3
"""Read-only evidence report for original 3DS/library.xml and before/after map XML.

This is NOT a 3DS exporter, a collision serializer, or a compatibility test.
All reported collision deltas are counts; positional ownership needs separate proof.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET

CHUNK_NAMES={0x4D4D:'main',0x3D3D:'editor',0x4000:'object',0x4100:'triangle_mesh',
             0x4110:'vertices',0x4120:'faces',0x4140:'uv',0x4160:'local_axis',0xAFFF:'material',
             0xA000:'material_name',0xA300:'texture_filename',0xB000:'keyframer'}
CONTAINERS={0x4D4D,0x3D3D,0x4100,0xAFFF,0xA200,0xB000,0xB002,0xB010}

def read_3ds(path:Path):
    data=path.read_bytes()
    if len(data)<6 or struct.unpack_from('<HI',data)[0]!=0x4D4D:
        raise ValueError('Not a 3DS main chunk')
    records=[]
    def walk(start,end,depth=0):
        if depth>24: raise ValueError('3DS chunk nesting exceeds limit')
        at=start
        while at<end:
            if at+6>end: raise ValueError('Truncated 3DS chunk header')
            kind,length=struct.unpack_from('<HI',data,at)
            if length<6 or at+length>end: raise ValueError('Invalid 3DS chunk length')
            payload=at+6
            entry={'id':f'{kind:04x}','kind':CHUNK_NAMES.get(kind,'unknown'),'offset':at,'length':length}
            if kind in (0x4000,0xA000,0xA300):
                stop=data.find(b'\0',payload,at+length)
                if stop<0: raise ValueError('Unterminated 3DS string')
                entry['name']=data[payload:stop].decode('latin-1')
                if kind==0x4000: payload=stop+1
            if kind in (0x4110,0x4120,0x4140):
                if payload+2>at+length: raise ValueError('Truncated geometry count')
                entry['count']=struct.unpack_from('<H',data,payload)[0]
            records.append(entry)
            if kind in CONTAINERS: walk(payload,at+length,depth+1)
            at+=length
    walk(0,len(data))
    return {'file':str(path),'bytes':len(data),'chunks':records,
            'object_names':[c['name'] for c in records if c['id']=='4000'],
            'helper_candidates':[c['name'] for c in records if c['id']=='4000' and c['name'].lower().startswith(('box','plane','tri','occl'))]}

def library_report(path:Path):
    root=ET.parse(path).getroot()
    if root.tag!='library': raise ValueError('Expected <library> root')
    props=[]
    for group in root.findall('prop-group'):
        for prop in group.findall('prop'):
            mesh=prop.find('mesh')
            props.append({'group':group.get('name'),'name':prop.get('name'),
                          'mesh':mesh.get('file') if mesh is not None else None,
                          'textures':[dict(x.attrib) for x in mesh.findall('texture')] if mesh is not None else []})
    return {'file':str(path),'library':root.get('name'),'props':props}

def map_report(path:Path):
    root=ET.parse(path).getroot()
    geometry=root.find('static-geometry'); collision=root.find('collision-geometry')
    return {'file':str(path),'root_tag':root.tag,
            'props':len(geometry.findall('prop')) if geometry is not None else 0,
            'collisions':{tag:len(collision.findall(tag)) if collision is not None else 0 for tag in
                          ('collision-plane','collision-box','collision-triangle')}}

def report(library:Path|None,model:Path|None,before:Path|None,after:Path|None):
    out={'warning':'Evidence inventory only; no collision-to-prop ownership or native export is inferred.'}
    if library: out['library']=library_report(library)
    if model: out['model']=read_3ds(model)
    if before or after:
        if not(before and after): raise ValueError('Both before and after maps are required')
        a,b=map_report(before),map_report(after)
        out['map_comparison']={'before':a,'after':b,'prop_delta':b['props']-a['props'],
                               'collision_deltas':{k:b['collisions'][k]-a['collisions'][k] for k in a['collisions']}}
    return out

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--library-xml',type=Path);p.add_argument('--model-3ds',type=Path)
    p.add_argument('--map-before',type=Path);p.add_argument('--map-after',type=Path)
    p.add_argument('--out',type=Path,help='Optional JSON report; never overwrites an existing file')
    args=p.parse_args()
    if not any((args.library_xml,args.model_3ds,args.map_before,args.map_after)):p.error('Provide source evidence')
    result=report(args.library_xml,args.model_3ds,args.map_before,args.map_after)
    encoded=json.dumps(result,indent=2,ensure_ascii=False)+'\n'
    if args.out:
        with args.out.open('x',encoding='utf-8') as f:f.write(encoded)
    else:print(encoded,end='')
if __name__=='__main__':main()
