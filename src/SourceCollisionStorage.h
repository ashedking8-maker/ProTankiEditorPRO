#pragma once
#include "SourceCollisionCatalog.h"
#include <pugixml.hpp>
#include <chrono>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace SourceCollisionStorage {
using namespace SourceCollisionCatalog;
inline void Put(pugi::xml_node n,V v){n.append_attribute("x").set_value(v.x);n.append_attribute("y").set_value(v.y);n.append_attribute("z").set_value(v.z);}
inline bool Get(pugi::xml_node n,V& v){if(!n||!n.attribute("x")||!n.attribute("y")||!n.attribute("z"))return false;v={n.attribute("x").as_float(NAN),n.attribute("y").as_float(NAN),n.attribute("z").as_float(NAN)};return Finite(v);}
inline bool Save(const std::filesystem::path& path,std::string& error) {
    error.clear();if(path.empty()){error="Missing settings directory.";return false;}
    pugi::xml_document d;auto root=d.append_child("source-collision-catalog");root.append_attribute("version")=2;
    {std::lock_guard lock(Mutex());for(const auto& e:Entries()) {
        auto n=root.append_child("mesh");n.append_attribute("identity")=e.identity.c_str();n.append_attribute("conflicted")=e.conflicted;
        for(auto v:e.mesh.vertices)Put(n.append_child("v"),v);
        for(auto f:e.mesh.faces){auto face=n.append_child("f");face.append_attribute("a")=f[0];face.append_attribute("b")=f[1];face.append_attribute("c")=f[2];}
        for(auto p:e.collision.planes){auto q=n.append_child("plane");Put(q.append_child("position"),p.offset);Put(q.append_child("rotation"),p.rotation);q.append_attribute("width").set_value(p.width);q.append_attribute("length").set_value(p.length);}
        for(auto t:e.collision.triangles){auto q=n.append_child("triangle");Put(q.append_child("position"),t.offset);Put(q.append_child("rotation"),t.rotation);Put(q.append_child("v0"),t.v0);Put(q.append_child("v1"),t.v1);Put(q.append_child("v2"),t.v2);}
    }}
    std::ostringstream buffer;d.save(buffer,"",pugi::format_raw);const auto bytes=buffer.str();
    if(bytes.size()>32u*1024*1024){error="Collision catalog exceeds 32 MB.";return false;}
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec){error=ec.message();return false;}
    auto temp=path;temp+=std::string(".tmp-")+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    {std::ofstream file(temp,std::ios::binary|std::ios::trunc);file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));file.flush();if(!file){error="Cannot write collision catalog.";file.close();std::filesystem::remove(temp,ec);return false;}}
#ifdef _WIN32
    const bool ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::filesystem::rename(temp,path,ec);const bool ok=!ec;
#endif
    if(!ok){error="Cannot replace collision catalog.";std::filesystem::remove(temp,ec);}return ok;
}
inline bool Load(const std::filesystem::path& path,std::string& error,bool merge=false) {
    error.clear();std::error_code ec;if(path.empty()||!std::filesystem::exists(path,ec))return !ec;
    if(std::filesystem::file_size(path,ec)>32u*1024*1024||ec){error="Invalid collision catalog size.";return false;}
    std::ifstream in(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});
    pugi::xml_document d;auto fail=[&]{error="Invalid collision catalog; previous in-memory data retained.";return false;};
    if(!d.load_buffer(bytes.data(),bytes.size()))return fail();auto root=d.child("source-collision-catalog");if(root.attribute("version").as_int()!=2)return fail();
    std::vector<Entry> loaded;
    for(auto n:root.children("mesh")) {
        if(loaded.size()>=4096)return fail();Entry e;e.identity=n.attribute("identity").as_string();if(e.identity.empty()||e.identity.size()>4096)return fail();e.conflicted=n.attribute("conflicted").as_bool();
        for(auto v:n.children("v")){V p;if(e.mesh.vertices.size()>=16384||!Get(v,p))return fail();e.mesh.vertices.push_back(p);}
        for(auto f:n.children("f")){if(e.mesh.faces.size()>=4096)return fail();std::array<uint16_t,3> indices{};int i=0;for(const char* name:{"a","b","c"}){const int value=f.attribute(name).as_int(-1);if(value<0||static_cast<size_t>(value)>=e.mesh.vertices.size())return fail();indices[i++]=static_cast<uint16_t>(value);}e.mesh.faces.push_back(indices);}
        for(auto q:n.children("plane")){VerifiedCollisionTemplates::Plane p;if(!Get(q.child("position"),p.offset)||!Get(q.child("rotation"),p.rotation))return fail();p.width=q.attribute("width").as_float(NAN);p.length=q.attribute("length").as_float(NAN);if(!std::isfinite(p.width)||!std::isfinite(p.length)||p.width<=.01f||p.length<=.01f||p.width>1.e7f||p.length>1.e7f)return fail();e.collision.planes.push_back(p);if(e.collision.planes.size()>4096)return fail();}
        for(auto q:n.children("triangle")){VerifiedCollisionTemplates::Triangle t;if(!Get(q.child("position"),t.offset)||!Get(q.child("rotation"),t.rotation)||!Get(q.child("v0"),t.v0)||!Get(q.child("v1"),t.v1)||!Get(q.child("v2"),t.v2)||Norm(Cross(Sub(t.v1,t.v0),Sub(t.v2,t.v0)))<.01f)return fail();e.collision.triangles.push_back(t);if(e.collision.triangles.size()>4096)return fail();}
        if(e.mesh.faces.empty()||!e.collision.Valid())return fail();
        for(const auto& prev:loaded)if(prev.identity==e.identity&&SameMesh(prev.mesh,e.mesh))return fail();loaded.push_back(std::move(e));
    }
    if(merge) {
        for(auto& entry:loaded) {
            const bool conflicted=entry.conflicted;
            Register(entry.mesh,std::move(entry.collision),entry.identity);
            if(conflicted){std::lock_guard lock(Mutex());for(auto& existing:Entries())if(existing.identity==entry.identity&&SameMesh(existing.mesh,entry.mesh))existing.conflicted=true;}
        }
    } else {std::lock_guard lock(Mutex());Entries()=std::move(loaded);}
    return true;
}
}
