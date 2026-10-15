#pragma once
#include "SourceCollisionCatalog.h"
#include "MapDocument.h"
#include "AssetRegistry.h"
#include <pugixml.hpp>
#include <set>
#include <tuple>
namespace SourceCollisionLearning {
using namespace SourceCollisionCatalog;
struct Shape {bool plane{};size_t index{};V center;};
struct Index {
    std::map<std::array<int,3>,std::vector<Shape>> cells;
    static std::array<int,3> Key(V p){return {int(std::floor(p.x/1000)),int(std::floor(p.y/1000)),int(std::floor(p.z/1000))};}
    explicit Index(const MapDocument& map) {
        for(size_t i=0;i<map.CollisionPlanes().size();++i){const auto& p=map.CollisionPlanes()[i];if(Finite(p.position))cells[Key(p.position)].push_back({true,i,p.position});}
        for(size_t i=0;i<map.CollisionTriangles().size();++i){const auto& t=map.CollisionTriangles()[i];if(Finite(t.position))cells[Key(t.position)].push_back({false,i,t.position});}
    }
    std::vector<Shape> Query(V lo,V hi) const {
        const auto a=Key(lo),b=Key(hi);std::vector<Shape> out;
        if(double(b[0]-a[0]+1)*(b[1]-a[1]+1)*(b[2]-a[2]+1)>100000)return out;
        for(int x=a[0];x<=b[0];++x)for(int y=a[1];y<=b[1];++y)for(int z=a[2];z<=b[2];++z){auto it=cells.find({x,y,z});if(it!=cells.end())out.insert(out.end(),it->second.begin(),it->second.end());}
        return out;
    }
};
inline bool PlainXml(const std::shared_ptr<const std::string>& xml,bool plane) {
    if(!xml)return false; // Only original source data is eligible for learning.
    pugi::xml_document d;if(!d.load_string(xml->c_str()))return false;auto n=d.document_element();
    if(std::string(n.name())!=(plane?"collision-plane":"collision-triangle"))return false;
    for(auto a:n.attributes())if(std::string(a.name())!="id")return false;
    std::set<std::string> seen;
    for(auto child:n.children()) {
        const std::string name=child.name();if(!seen.insert(name).second||child.first_attribute())return false;
        if(name=="position"||name=="rotation"||(!plane&&(name=="v0"||name=="v1"||name=="v2"))) {
            std::set<std::string> axes;
            for(auto axis:child.children())if(!axes.insert(axis.name()).second||
                (std::string(axis.name())!="x"&&std::string(axis.name())!="y"&&std::string(axis.name())!="z")||axis.first_attribute()||axis.first_child().type()!=pugi::node_pcdata||axis.first_child().next_sibling())return false;
            if(axes.size()!=3)return false;
        } else if(!plane||(name!="width"&&name!="length")||child.first_child().type()!=pugi::node_pcdata||child.first_child().next_sibling())return false;
    }
    return seen.size()==(plane?4u:5u);
}
// Area sums alone are insufficient: overlapping visible triangles could hide
// an uncovered region. Reject positive-area overlap inside a source plane.
inline double OverlapArea(const CollisionPolygon& a,const CollisionPolygon& b,V origin,V x,V y) {
    struct P {double x,y;};
    const auto project=[&](V v){v=Sub(v,origin);return P{Dot(v,x),Dot(v,y)};};
    std::vector<P> polygon;for(auto v:a)polygon.push_back(project(v));
    for(size_t edge=0;edge<b.size()&&!polygon.empty();++edge) {
        const auto p=project(b[edge]),q=project(b[(edge+1)%b.size()]);
        const auto distance=[&](P r){return (q.x-p.x)*(r.y-p.y)-(q.y-p.y)*(r.x-p.x);};
        std::vector<P> clipped;auto previous=polygon.back();double dp=distance(previous);
        for(auto current:polygon) {
            const double dc=distance(current);const bool ip=dp>=-1.e-7,ic=dc>=-1.e-7;
            if(ip!=ic&&std::fabs(dp-dc)>1.e-12){const double t=dp/(dp-dc);clipped.push_back({previous.x+t*(current.x-previous.x),previous.y+t*(current.y-previous.y)});}
            if(ic)clipped.push_back(current);previous=current;dp=dc;
        }
        polygon=std::move(clipped);
    }
    if(polygon.size()<3)return 0;double area=0;const auto origin2=polygon.front();
    for(size_t i=1;i+1<polygon.size();++i)area+=(polygon[i].x-origin2.x)*(polygon[i+1].y-origin2.y)-(polygon[i].y-origin2.y)*(polygon[i+1].x-origin2.x);
    return std::fabs(area)*.5;
}
inline Result Infer(const MapDocument& map,size_t propIndex,const Mesh& mesh,const Index& index) {
    Result out;out.error="Source collision does not completely and uniquely cover the mesh.";
    const auto& p=map.Props()[propIndex];
    if(p.nativeWithCollision==0||p.hasInvalidNativeMetadata||p.hasUncopyableMetadata||
       std::fabs(p.rotation.x)>1.e-5f||std::fabs(p.rotation.y)>1.e-5f)return out;
    const auto world=[&](V v){return Add(p.position,Native3DSScene::Rotate(v,{0,0,p.rotation.z}));};
    const auto local=[&](V v){return Native3DSScene::Rotate(Sub(v,p.position),{0,0,-p.rotation.z});};
    V lo{1.e7f,1.e7f,1.e7f},hi{-1.e7f,-1.e7f,-1.e7f};
    std::vector<CollisionPolygon> faces;std::vector<float> areas;std::vector<V> normals;
    for(auto v:mesh.vertices){v=world(v);if(!Finite(v))return out;lo={std::min(lo.x,v.x-.1f),std::min(lo.y,v.y-.1f),std::min(lo.z,v.z-.1f)};hi={std::max(hi.x,v.x+.1f),std::max(hi.y,v.y+.1f),std::max(hi.z,v.z+.1f)};}
    // Boxes cannot be reconstructed from visible triangle coverage safely.
    for(const auto& b:map.CollisionBoxes()) {
        const auto v=b.position;
        if(v.x>=lo.x&&v.x<=hi.x&&v.y>=lo.y&&v.y<=hi.y&&v.z>=lo.z&&v.z<=hi.z)return out;
    }
    for(auto f:mesh.faces) {
        if(f[0]>=mesh.vertices.size()||f[1]>=mesh.vertices.size()||f[2]>=mesh.vertices.size())return out;
        CollisionPolygon tri{mesh.vertices[f[0]],mesh.vertices[f[1]],mesh.vertices[f[2]]};
        auto n=Cross(Sub(tri[1],tri[0]),Sub(tri[2],tri[0]));const float area=Norm(n)*.5f;
        if(area<.01f)return out;faces.push_back(std::move(tri));areas.push_back(area);normals.push_back(Unit(n));
    }
    std::vector<unsigned> covered(faces.size());Result found;
    for(const auto& candidate:index.Query(lo,hi)) {
        CollisionPolygon corners;VerifiedCollisionTemplates::Plane plane{};VerifiedCollisionTemplates::Triangle triangle{};
        bool plain=false;
        if(candidate.plane) {
            const auto& c=map.CollisionPlanes()[candidate.index];
            if(c.authoredOwnerIndex>=0&&c.authoredOwnerIndex!=static_cast<int>(propIndex))continue;
            if(!std::isfinite(c.width)||!std::isfinite(c.length)||c.width<=0||c.length<=0)continue;
            plane={local(c.position),{c.rotation.x,c.rotation.y,c.rotation.z-p.rotation.z},c.width,c.length};
            corners=Corners(plane);plain=PlainXml(c.originalXml,true);
        } else {
            const auto& c=map.CollisionTriangles()[candidate.index];
            if(c.authoredOwnerIndex>=0&&c.authoredOwnerIndex!=static_cast<int>(propIndex))continue;
            triangle={local(c.position),{c.rotation.x,c.rotation.y,c.rotation.z-p.rotation.z},c.v0,c.v1,c.v2};
            corners=Corners(triangle);plain=PlainXml(c.originalXml,false);
        }
        if(std::any_of(corners.begin(),corners.end(),[](V v){return !Finite(v);}))continue;
        auto n=Cross(Sub(corners[1],corners[0]),Sub(corners[2],corners[0]));if(Norm(n)<.01f)continue;n=Unit(n);
        std::vector<size_t> hits;double area=0;
        V x{},y{};float width=0,length=0;
        if(candidate.plane){x=Sub(corners[1],corners[0]);y=Sub(corners[3],corners[0]);width=Norm(x);length=Norm(y);if(width<.01f||length<.01f)continue;x=Unit(x);y=Unit(y);}
        for(size_t i=0;i<faces.size();++i) {
            if(Dot(n,normals[i])<.999f)continue;
            bool match=!candidate.plane&&SamePolygon(corners,faces[i]);
            if(candidate.plane) {
                match=true;
                for(V v:faces[i]){v=Sub(v,corners[0]);const float a=Dot(v,x),b=Dot(v,y);if(std::fabs(Dot(v,n))>.09f||a<-.09f||a>width+.09f||b<-.09f||b>length+.09f){match=false;break;}}
            }
            if(match){hits.push_back(i);area+=areas[i];}
        }
        if(hits.empty())continue;
        if(candidate.plane&&std::fabs(area-double(width)*length)>std::max(1.,double(width)*length*1.e-5))continue;
        if(candidate.plane)for(size_t a=0;a<hits.size();++a)for(size_t b=a+1;b<hits.size();++b)
            if(OverlapArea(faces[hits[a]],faces[hits[b]],corners[0],x,y)>
               std::max(.0001,double(std::min(areas[hits[a]],areas[hits[b]]))*1.e-7))return out;
        if(!plain)return out; // Never strip unknown collision metadata.
        for(auto i:hits)if(++covered[i]!=1)return out;
        if(candidate.plane)found.planes.push_back(plane);else found.triangles.push_back(triangle);
    }
    for(size_t i=0;i<faces.size();++i)if(!covered[i]) {
        // Coincident opposite internal faces may be absent in the original physics.
        // They must occur as an exact isolated pair; no exterior hole is accepted.
        CollisionPolygon reverse=faces[i];std::swap(reverse[0],reverse[1]);size_t partners=0;
        for(size_t j=0;j<faces.size();++j)if(j!=i&&!covered[j]&&SamePolygon(reverse,faces[j]))++partners;
        if(partners!=1)return out;
    }
    if(!found.Valid())return out;
    found.error.clear();return found;
}
struct Stats {size_t learned{},checked{},rejected{};bool changed{};};
// At least two spatially distinct original instances must independently agree.
inline Stats Learn(MapDocument& map,const AssetRegistry& assets) {
    Stats stats;const auto revision=Revision();Index index(map);
    std::map<std::tuple<std::string,std::string,std::string>,std::vector<size_t>> groups;
    for(size_t i=0;i<map.Props().size();++i){const auto& p=map.Props()[i];groups[{p.library,p.group,p.name}].push_back(i);}
    for(const auto& [key,instances]:groups) {
        if(instances.size()<2)continue;
        const auto* asset=assets.Find(std::get<0>(key),std::get<1>(key),std::get<2>(key));
        if(!asset||asset->mesh.empty())continue;
        auto native=NativeCollisionImport::Read(asset->mesh,asset->meshObject);
        if(native.error!=NoNativeHelpersError)continue;
        Mesh mesh;if(!Capture(asset->mesh,asset->meshObject,mesh))continue;++stats.checked;
        Result reference;size_t witnesses=0;std::vector<V> positions;bool conflict=false;
        // Every spatially distinct instance in the loaded source must agree.
        for(size_t i:instances) {
            const auto position=map.Props()[i].position;
            if(std::any_of(positions.begin(),positions.end(),[&](V v){return Norm(Sub(v,position))<1.f;}))continue;
            auto candidate=Infer(map,i,mesh,index);
            if(!candidate.Valid()){conflict=true;break;}
            if(witnesses&&!Equivalent(reference,candidate)){conflict=true;break;}
            if(!witnesses)reference=std::move(candidate);
            ++witnesses;positions.push_back(position);
        }
        if(conflict||witnesses<2){++stats.rejected;continue;}
        if(Register(std::move(mesh),std::move(reference),Identity(asset->library,asset->group,asset->name)))++stats.learned;
    }
    stats.changed=Revision()!=revision;return stats;
}
}
