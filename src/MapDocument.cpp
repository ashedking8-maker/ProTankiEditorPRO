#include "MapDocument.h"
#include "Logger.h"
#include "VerifiedCollisionTemplates.h"
#include "NativeCollisionImport.h"
#include <pugixml.hpp>
#include <windows.h>
#include <algorithm>
#include <array>
#include <utility>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
float number(pugi::xml_node n, const char* child, float fallback = 0.0f) {
    auto c = n.child(child);
    return c ? c.text().as_float(fallback) : fallback;
}
DirectX::XMFLOAT3 vec3(pugi::xml_node n) {
    return { number(n, "x"), number(n, "y"), number(n, "z") };
}
size_t childCount(pugi::xml_node n, const char* name) {
    size_t count = 0;
    for (auto ignored : n.children(name)) { (void)ignored; ++count; }
    return count;
}
std::string fixed(float value, int decimals) {
    const float threshold = decimals == 3 ? 0.0005f : 0.0000005f;
    if (std::fabs(value) < threshold) value = 0.0f;
    std::ostringstream out;
    out.setf(std::ios::fixed, std::ios::floatfield);
    out << std::setprecision(decimals) << value;
    return out.str();
}
void setExisting(pugi::xml_node parent, const char* child, float value, int decimals) {
    auto n = parent.child(child);
    if (n) n.text().set(fixed(value, decimals).c_str());
}
void setOrAppend(pugi::xml_node parent, const char* child, float value, int decimals) {
    auto n = parent.child(child);
    if (!n) n = parent.append_child(child);
    n.text().set(fixed(value, decimals).c_str());
}
void normalizeLegacySelfClosing(std::string& xml) {
    size_t pos = 0;
    while ((pos = xml.find(" />", pos)) != std::string::npos) xml.erase(pos, 1);
    while (!xml.empty() && (xml.back() == '\n' || xml.back() == '\r')) xml.pop_back();
}
}

void MapDocument::Clear() {
    path_.clear(); version_.clear(); sourceXml_=std::make_shared<const std::string>(); props_.clear(); propTransformDirty_.clear();
    specialBoxes_.clear(); lights_.clear(); ctfFlags_.clear(); spawns_.clear(); bonuses_.clear(); controlPoints_.clear();
    collisionPlanes_.clear(); collisionBoxes_.clear(); collisionTriangles_.clear(); stats_ = {}; dirty_ = false;
    flagsDirty_=spawnsDirty_=pointsDirty_=bonusesDirty_=zonesDirty_=collisionDirty_=lightsDirty_=false;
}

void MapDocument::CreateBlank(const std::string& version, bool markDirty) {
    Clear();
    version_ = version.empty() ? "1.0.Light" : version;
    // Keep all canonical legacy sections present so editing and serialization use the same pipeline.
    sourceXml_ = std::make_shared<const std::string>("<map version=\"" + version_ + "\">\n"
        "  <static-geometry/>\n  <collision-geometry/>\n  <spawn-points/>\n"
        "  <ctf-flags/>\n  <dom-keypoints/>\n  <bonus-regions/>\n"
        "  <special-geometry/>\n  <lights/>\n  <way-points/>\n</map>");
    dirty_ = markDirty; // Startup workspace is clean; explicit File > New map remains dirty.
}

bool MapDocument::Load(const std::filesystem::path& file, std::string& error) {
    Clear();
    Log::Info("Map load begin: " + Log::PathUtf8(file));

    std::ifstream input(file, std::ios::binary);
    if (!input) { error = "Could not open map file."; Log::Error(error); return false; }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    auto loaded=buffer.str();
    if (loaded.empty()) { error = "Map file is empty."; Log::Error(error); Clear(); return false; }

    sourceXml_=std::make_shared<const std::string>(std::move(loaded));
    pugi::xml_document doc;
    const auto result = doc.load_buffer(sourceXml_->data(), sourceXml_->size(), pugi::parse_full, pugi::encoding_utf8);
    if (!result) {
        error = std::string("XML parse error: ") + result.description();
        Log::Error(error);
        Clear();
        return false;
    }
    auto map = doc.child("map");
    if (!map) { error = "Root <map> element is missing."; Log::Error(error); Clear(); return false; }

    path_ = file;
    version_ = map.attribute("version").as_string();

    auto geometry = map.child("static-geometry");
    props_.reserve(childCount(geometry, "prop"));
    int sourceIndex = 0;
    for (auto p : geometry.children("prop")) {
        PropInstance i;
        i.library = p.attribute("library-name").as_string();
        i.group = p.attribute("group-name").as_string();
        i.name = p.attribute("name").as_string();
        i.texture = p.child("texture-name").text().as_string();
        i.position = vec3(p.child("position"));
        i.rotation = vec3(p.child("rotation"));
        if (const auto native = p.child("with_collision")) {
            const std::string raw = native.text().as_string();
            if (raw == "0" || raw == "1") i.nativeWithCollision = raw == "1" ? 1 : 0;
            else {i.hasUncopyableMetadata=true;i.hasInvalidNativeMetadata=true;}
        }
        if (const auto free = p.attribute("free")) {
            const std::string raw = free.value();
            if (raw == "true" || raw == "1") i.nativeFree = 1;
            else if (raw == "false" || raw == "0") i.nativeFree = 0;
            else {i.hasUncopyableMetadata=true;i.hasInvalidNativeMetadata=true;}
        }
        // Ambiguous duplicate native keys cannot be safely cloned by selecting
        // the first node and hoping that the engine does the same.
        if(childCount(p,"with_collision")>1 || childCount(p,"position")>1 ||
           childCount(p,"rotation")>1 || childCount(p,"texture-name")>1) {
            i.hasUncopyableMetadata=true;
            i.hasInvalidNativeMetadata=true;
        }
        for (const auto attr:p.attributes()) {
            const std::string name=attr.name();
            if(name!="library-name"&&name!="group-name"&&name!="name"&&name!="free")
                i.hasUncopyableMetadata=true;
        }
        for (const auto child:p.children()) {
            if(child.type()!=pugi::node_element)continue;
            const std::string name=child.name();
            if(name!="rotation"&&name!="position"&&name!="texture-name"&&name!="with_collision")
                i.hasUncopyableMetadata=true;
        }
        std::ostringstream originalNode;
        p.print(originalNode,"",pugi::format_raw,pugi::encoding_utf8);
        i.originalPropXml=std::make_shared<const std::string>(originalNode.str());
        i.legacySourceIndex = sourceIndex++;
        props_.push_back(std::move(i));
    }
    propTransformDirty_.assign(props_.size(), false);

    stats_.props = props_.size();
    auto collision = map.child("collision-geometry");
    stats_.collisionPlanes = childCount(collision, "collision-plane");
    stats_.collisionBoxes = childCount(collision, "collision-box");
    stats_.collisionTriangles = childCount(collision, "collision-triangle");
    collisionPlanes_.reserve(stats_.collisionPlanes);
    int collisionIndex=0;
    for (auto node : collision.children("collision-plane")) {
        CollisionPlane p;p.legacySourceIndex=collisionIndex++;
        p.position = vec3(node.child("position")); p.rotation = vec3(node.child("rotation"));
        p.width = number(node,"width"); p.length = number(node,"length");
        collisionPlanes_.push_back(p);
    }
    collisionBoxes_.reserve(stats_.collisionBoxes);
    collisionIndex=0;
    for (auto node : collision.children("collision-box")) {
        CollisionBox b;b.legacySourceIndex=collisionIndex++;
        b.position = vec3(node.child("position")); b.rotation = vec3(node.child("rotation"));
        b.size = vec3(node.child("size")); collisionBoxes_.push_back(b);
    }
    collisionTriangles_.reserve(stats_.collisionTriangles);
    collisionIndex=0;
    for (auto node : collision.children("collision-triangle")) {
        CollisionTriangle t;t.legacySourceIndex=collisionIndex++;
        t.v0 = vec3(node.child("v0")); t.v1 = vec3(node.child("v1")); t.v2 = vec3(node.child("v2"));
        // Preserve local vertices and the separate authored transform. The
        // preview uses these; SaveLegacy still keeps the original XML bytes.
        t.position = vec3(node.child("position")); t.rotation = vec3(node.child("rotation"));
        collisionTriangles_.push_back(t);
    }
    stats_.spawnPoints = childCount(map.child("spawn-points"), "spawn-point");
    stats_.bonusRegions = childCount(map.child("bonus-regions"), "bonus-region");
    stats_.specialBoxes = childCount(map.child("special-geometry"), "special-box");
    stats_.lights = childCount(map.child("lights"),"light");
    lights_.reserve(stats_.lights);
    int lightIndex=0;
    for(auto light:map.child("lights").children("light")) {
        LightMarker marker;
        marker.legacySourceIndex=lightIndex++;
        marker.type=light.attribute("type").as_string("omni");
        marker.color=light.attribute("color").as_uint(0xFFFA9Du);
        marker.intensity=light.attribute("intensity").as_float(1.0f);
        marker.attenuationBegin=light.attribute("attenuationBegin").as_float(0.1f);
        marker.attenuationEnd=light.attribute("attenuationEnd").as_float(20.f);
        marker.position=vec3(light.child("position"));
        marker.rotationZ=number(light.child("rotation"),"z");
        lights_.push_back(std::move(marker));
    }
    stats_.wayPoints = childCount(map.child("way-points"), "way-point");
    stats_.dominationPoints = childCount(map.child("dom-keypoints"), "dom-keypoint");
    auto flags = map.child("ctf-flags");
    stats_.ctfFlags = childCount(flags, "flag-red") + childCount(flags, "flag-blue");

    specialBoxes_.reserve(stats_.specialBoxes);
    int functionalIndex=0;
    for (auto box : map.child("special-geometry").children("special-box")) {
        SpecialBox s;
        s.legacySourceIndex=functionalIndex++;
        s.min = {number(box, "minX"), number(box, "minY"), number(box, "minZ")};
        s.max = {number(box, "maxX"), number(box, "maxY"), number(box, "maxZ")};
        s.action = box.child("action").text().as_string();
        s.free = box.attribute("free").as_bool(false);
        specialBoxes_.push_back(std::move(s));
    }

    functionalIndex=0;
    for (auto flag : flags.children()) {
        if (std::string(flag.name()) != "flag-red" && std::string(flag.name()) != "flag-blue") continue;
        CtfFlagMarker f;
        f.legacySourceIndex=functionalIndex++;
        const std::string nodeName = flag.name();
        if (nodeName.find("red") != std::string::npos) f.team = "red";
        else if (nodeName.find("blue") != std::string::npos) f.team = "blue";
        else f.team = nodeName;
        f.position = vec3(flag);
        ctfFlags_.push_back(std::move(f));
    }

    functionalIndex=0;
    for (auto spawn : map.child("spawn-points").children("spawn-point")) {
        SpawnMarker marker;
        marker.legacySourceIndex=functionalIndex++;
        marker.position = vec3(spawn.child("position"));
        marker.rotationZ = number(spawn.child("rotation"), "z");
        marker.type = spawn.attribute("type").as_string();
        marker.team = spawn.attribute("team").as_string();
        spawns_.push_back(std::move(marker));
    }
    functionalIndex=0;
    for (auto region : map.child("bonus-regions").children("bonus-region")) {
        BonusRegionMarker marker;
        marker.legacySourceIndex=functionalIndex++;
        marker.min = vec3(region.child("min"));
        marker.max = vec3(region.child("max"));
        marker.name = region.attribute("name").as_string();
        marker.bonusType = region.child("bonus-type").text().as_string();
        if(const auto attr=region.attribute("free"))marker.free=attr.as_bool();
        if(const auto attr=region.attribute("parachute"))marker.parachute=attr.as_bool();
        for (auto mode : region.children("game-mode")) marker.modes.emplace_back(mode.text().as_string());
        bonuses_.push_back(std::move(marker));
    }
    functionalIndex=0;
    for (auto point : map.child("dom-keypoints").children("dom-keypoint")) {
        ControlPointMarker marker;
        marker.legacySourceIndex=functionalIndex++;
        marker.position = vec3(point.child("position"));
        marker.name = point.attribute("name").as_string();
        marker.distance = point.attribute("distance").as_float();
        marker.free = point.attribute("free").as_bool();
        controlPoints_.push_back(std::move(marker));
    }

    dirty_ = false;
    BindVerifiedCollisionOwners();
    Log::Info("Map load complete. version=" + version_ + " props=" + std::to_string(stats_.props) +
        " collisionPlanes=" + std::to_string(stats_.collisionPlanes) + " collisionBoxes=" + std::to_string(stats_.collisionBoxes) +
        " collisionTriangles=" + std::to_string(stats_.collisionTriangles) + " spawns=" + std::to_string(stats_.spawnPoints) +
        " bonuses=" + std::to_string(stats_.bonusRegions) + " specialBoxes=" + std::to_string(stats_.specialBoxes) +
        " ctfFlags=" + std::to_string(stats_.ctfFlags));
    error.clear();
    return true;
}

void MapDocument::BindVerifiedCollisionOwners() {
    // Native XML has no explicit prop->collider pointer. Bind ONLY a fully
    // validated 6-plane/10-triangle template. Never claim partial ownership.
    constexpr float tolerance=0.02f;
    auto same=[&](const DirectX::XMFLOAT3& a,const DirectX::XMFLOAT3& b) {
        return std::fabs(a.x-b.x)<tolerance && std::fabs(a.y-b.y)<tolerance &&
            std::fabs(a.z-b.z)<tolerance;
    };
    // Different Euler angles can share an origin; never bind them as one prop.
    auto sameAngle=[](float a,float b) {
        return std::fabs(std::atan2(std::sin(a-b),std::cos(a-b)))<0.001f;
    };
    auto sameRotation=[&](const DirectX::XMFLOAT3& actual,
                          const DirectX::XMFLOAT3& reference,float yaw) {
        return sameAngle(actual.x,reference.x)&&sameAngle(actual.y,reference.y)&&
            sameAngle(actual.z,reference.z+yaw);
    };
    for(size_t i=0;i<props_.size();++i) {
        const auto& p=props_[i];
        if(!VerifiedCollisionTemplates::Available(p.library,p.group,p.name))continue;
        const float cs=std::cos(p.rotation.z),sn=std::sin(p.rotation.z);
        auto place=[&](const DirectX::XMFLOAT3& offset) {
            return DirectX::XMFLOAT3{p.position.x+cs*offset.x-sn*offset.y,
                p.position.y+sn*offset.x+cs*offset.y,p.position.z+offset.z};
        };
        std::vector<size_t> planeIndices,triangleIndices;
        bool unambiguous=true;
        for(const auto& templ:VerifiedCollisionTemplates::BeachWallEnd2Planes) {
            const auto expected=place(templ.offset);
            size_t match=collisionPlanes_.size();
            for(size_t j=0;j<collisionPlanes_.size();++j) {
                const auto& c=collisionPlanes_[j];
                if(c.authoredOwnerIndex>=0 || !same(c.position,expected) ||
                    !sameRotation(c.rotation,templ.rotation,p.rotation.z) ||
                    std::fabs(c.width-templ.width)>tolerance ||
                    std::fabs(c.length-templ.length)>tolerance)continue;
                if(match!=collisionPlanes_.size()) {unambiguous=false;break;}
                match=j;
            }
            if(!unambiguous || match==collisionPlanes_.size()) {unambiguous=false;break;}
            planeIndices.push_back(match);
        }
        if(!unambiguous)continue;
        for(const auto& templ:VerifiedCollisionTemplates::BeachWallEnd2Triangles) {
            const auto expected=place(templ.offset);
            size_t match=collisionTriangles_.size();
            for(size_t j=0;j<collisionTriangles_.size();++j) {
                const auto& c=collisionTriangles_[j];
                if(c.authoredOwnerIndex>=0 || !same(c.position,expected) ||
                    !sameRotation(c.rotation,templ.rotation,p.rotation.z) ||
                    !same(c.v0,templ.v0)||!same(c.v1,templ.v1)||!same(c.v2,templ.v2))continue;
                if(match!=collisionTriangles_.size()) {unambiguous=false;break;}
                match=j;
            }
            if(!unambiguous || match==collisionTriangles_.size()) {unambiguous=false;break;}
            triangleIndices.push_back(match);
        }
        if(!unambiguous)continue;
        for(auto at:planeIndices)collisionPlanes_[at].authoredOwnerIndex=static_cast<int>(i);
        for(auto at:triangleIndices)collisionTriangles_[at].authoredOwnerIndex=static_cast<int>(i);
        Log::Debug("Verified original collision ownership: prop="+std::to_string(i)+" planes=6 triangles=10");
    }
}

bool MapDocument::SetPropTransform(size_t index, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& rotation) {
    if (index >= props_.size()) return false;
    auto& p = props_[index];
    const bool changed = p.position.x != position.x || p.position.y != position.y || p.position.z != position.z ||
        p.rotation.x != rotation.x || p.rotation.y != rotation.y || p.rotation.z != rotation.z;
    if (!changed) return true;
    // Match only colliders with the exact OLD prop origin and no second prop at
    // that location. The source format has no explicit prop/collider relation:
    // never guess a match on proximity or a triangle's zero local origin.
    const auto old=p.position, oldRotation=p.rotation;
    const auto same=[&](const DirectX::XMFLOAT3& value) {
        return std::fabs(value.x-old.x)<=0.1f && std::fabs(value.y-old.y)<=0.1f &&
               std::fabs(value.z-old.z)<=0.1f;
    };
    // Author-owned primitives are explicitly bound in memory; never infer their
    // ownership from a matching centre or discard their individual offsets.
    const float yawDelta=rotation.z-oldRotation.z;
    const float cs=std::cos(yawDelta),sn=std::sin(yawDelta);
    auto moveAuthored=[&](auto& c) {
        if(c.authoredOwnerIndex!=static_cast<int>(index))return;
        const float dx=c.position.x-old.x,dy=c.position.y-old.y;
        c.position={position.x+cs*dx-sn*dy,position.y+sn*dx+cs*dy,position.z+c.position.z-old.z};
        c.rotation.z+=yawDelta;c.transformDirty=true;collisionDirty_=true;
    };
    for(auto& c:collisionPlanes_)moveAuthored(c);
    for(auto& c:collisionBoxes_)moveAuthored(c);
    for(auto& c:collisionTriangles_)moveAuthored(c);
    bool sharedOrigin=false;
    for(size_t i=0;i<props_.size();++i)if(i!=index && same(props_[i].position)) {
        sharedOrigin=true;break;
    }
    if(!sharedOrigin) {
        auto moveCollider=[&](auto& c) {
            if(c.authoredOwnerIndex>=0 || !same(c.position))return;
            c.position.x+=position.x-old.x;
            c.position.y+=position.y-old.y;
            c.position.z+=position.z-old.z;
            c.rotation.z+=rotation.z-oldRotation.z;
            c.transformDirty=true;
            collisionDirty_=true;
        };
        for(auto& c:collisionPlanes_)moveCollider(c);
        for(auto& c:collisionBoxes_)moveCollider(c);
    }
    p.position = position;
    p.rotation = rotation;
    propTransformDirty_[index] = true;
    dirty_ = true;
    {
        std::ostringstream msg;
        msg << "Prop transform changed index=" << index
            << " pos=(" << position.x << "," << position.y << "," << position.z << ")"
            << " rot=(" << rotation.x << "," << rotation.y << "," << rotation.z << ")";
        Log::Debug(msg.str());
    }
    return true;
}

size_t MapDocument::ExactCollidersAt(const DirectX::XMFLOAT3& at) const {
    auto same=[&](const DirectX::XMFLOAT3& other) {
        constexpr float tolerance=0.10f;
        return std::fabs(other.x-at.x)<=tolerance && std::fabs(other.y-at.y)<=tolerance &&
               std::fabs(other.z-at.z)<=tolerance;
    };
    size_t count=0;
    for(const auto& c:collisionPlanes_)if(same(c.position))++count;
    for(const auto& c:collisionBoxes_)if(same(c.position))++count;
    // A triangle's origin is often (0,0,0) while its vertices are elsewhere.
    // Do not guess associations from its origin.
    return count;
}

bool MapDocument::DeleteCollider(ColliderKind kind,size_t index) {
    switch(kind) {
    case ColliderKind::Plane:
        if(index>=collisionPlanes_.size()||collisionPlanes_[index].authoredOwnerIndex>=0)return false;
        collisionPlanes_.erase(collisionPlanes_.begin()+static_cast<std::ptrdiff_t>(index));
        stats_.collisionPlanes=collisionPlanes_.size();break;
    case ColliderKind::Box:
        if(index>=collisionBoxes_.size()||collisionBoxes_[index].authoredOwnerIndex>=0)return false;
        collisionBoxes_.erase(collisionBoxes_.begin()+static_cast<std::ptrdiff_t>(index));
        stats_.collisionBoxes=collisionBoxes_.size();break;
    case ColliderKind::Triangle:
        if(index>=collisionTriangles_.size()||collisionTriangles_[index].authoredOwnerIndex>=0)return false;
        collisionTriangles_.erase(collisionTriangles_.begin()+static_cast<std::ptrdiff_t>(index));
        stats_.collisionTriangles=collisionTriangles_.size();break;
    }
    collisionDirty_=dirty_=true;
    return true;
}

bool MapDocument::DeletePropWithCollision(size_t index,std::string& reason) {
    return DeletePropsWithCollision({index},reason);
}

bool MapDocument::DeletePropsWithCollision(const std::vector<size_t>& requested,std::string& reason) {
    std::vector<size_t> indices=requested;
    std::sort(indices.begin(),indices.end());
    indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
    if(indices.empty()){reason="Deletion skipped: nothing selected.";Log::Warning(reason);return false;}
    for(const size_t i:indices)if(i>=props_.size()) {
        reason="Deletion blocked: invalid prop index "+std::to_string(i)+".";
        Log::Warning(reason);return false;
    }
    Log::Info("Delete request: selected="+std::to_string(indices.size())+
        " totalProps="+std::to_string(props_.size())+
        " planes="+std::to_string(collisionPlanes_.size())+
        " boxes="+std::to_string(collisionBoxes_.size())+
        " triangles="+std::to_string(collisionTriangles_.size()));
    const bool allProps=indices.size()==props_.size();
    if(allProps) {
        // Explicitly deleting ALL static props means the static collision section
        // must also be emptied: otherwise invisible geometry would remain.
        const size_t removed=collisionPlanes_.size()+collisionBoxes_.size()+collisionTriangles_.size();
        props_.clear();propTransformDirty_.clear();
        collisionPlanes_.clear();collisionBoxes_.clear();collisionTriangles_.clear();
        stats_.props=stats_.collisionPlanes=stats_.collisionBoxes=stats_.collisionTriangles=0;
        collisionDirty_=dirty_=true;
        reason="Removed all "+std::to_string(indices.size())+" static props and all "+
            std::to_string(removed)+" static collision primitives. Functional map elements are preserved.";
        Log::Info(reason);return true;
    }
    auto same=[&](const DirectX::XMFLOAT3& a,const DirectX::XMFLOAT3& b) {
        return std::fabs(a.x-b.x)<=0.1f && std::fabs(a.y-b.y)<=0.1f && std::fabs(a.z-b.z)<=0.1f;
    };
    std::vector<size_t> planes,boxes;
    size_t sharedOrigins=0;
    // A shared collider remains while another visual prop at that position
    // survives. This allows deletion of one co-located decorative prop without
    // deleting the physical surface of a remaining prop.
    for(const size_t i:indices) {
        const auto& p=props_[i];
        if(p.legacySourceIndex<0)continue; // Author-owned colliders are removed by DeleteProp.
        const size_t count=ExactCollidersAt(p.position);
        bool hasUnselectedProp=false;
        for(size_t j=0;j<props_.size();++j) if(j!=i &&
            same(props_[j].position,p.position) &&
            !std::binary_search(indices.begin(),indices.end(),j)) {
            hasUnselectedProp=true;
            Log::Info("Delete shared origin: prop="+std::to_string(i)+
                " survivingProp="+std::to_string(j)+
                " collidersPreserved="+std::to_string(count));
            break;
        }
        Log::Info("Delete preflight prop index="+std::to_string(i)+" sourceIndex="+
            std::to_string(p.legacySourceIndex)+" identity="+p.library+"/"+p.group+"/"+p.name+
            " exactPlaneOrBoxCount="+std::to_string(count));
        if(hasUnselectedProp) {
            if(count) ++sharedOrigins;
            continue;
        }
        for(size_t j=0;j<collisionPlanes_.size();++j)if(collisionPlanes_[j].authoredOwnerIndex<0 && same(collisionPlanes_[j].position,p.position))planes.push_back(j);
        for(size_t j=0;j<collisionBoxes_.size();++j)if(collisionBoxes_[j].authoredOwnerIndex<0 && same(collisionBoxes_[j].position,p.position))boxes.push_back(j);
        // Triangle ownership cannot safely be inferred from a matching origin.
    }
    std::sort(planes.begin(),planes.end());planes.erase(std::unique(planes.begin(),planes.end()),planes.end());
    std::sort(boxes.begin(),boxes.end());boxes.erase(std::unique(boxes.begin(),boxes.end()),boxes.end());
    // A partial edit cannot safely erase triangles without explicit ownership.
    // Do not silently claim a complete collision removal for a matching prop.
    for(auto it=planes.rbegin();it!=planes.rend();++it)DeleteCollider(ColliderKind::Plane,*it);
    for(auto it=boxes.rbegin();it!=boxes.rend();++it)DeleteCollider(ColliderKind::Box,*it);
    const size_t authoredRemoved=collisionPlanes_.size()+collisionBoxes_.size()+collisionTriangles_.size();
    for(auto it=indices.rbegin();it!=indices.rend();++it)DeleteProp(*it);
    const size_t authoredAfter=collisionPlanes_.size()+collisionBoxes_.size()+collisionTriangles_.size();
    reason="Removed "+std::to_string(indices.size())+" prop(s), "+std::to_string(planes.size())+
        " planes and "+std::to_string(boxes.size())+" boxes by exact origin; "+
        std::to_string(authoredRemoved-authoredAfter)+" explicitly owned native primitives. " +
        (sharedOrigins?"Preserved shared collision at "+std::to_string(sharedOrigins)+" origin(s). ":"")+
        (collisionTriangles_.empty()?"No triangles remain.":"Unrelated or unverified legacy triangles were preserved; inspect Geometry before exporting.");
    Log::Info(reason);
    return true;
}

size_t MapDocument::AddProp(PropInstance prop) {
    prop.legacySourceIndex = -1;
    props_.push_back(std::move(prop));
    propTransformDirty_.push_back(true);
    stats_.props = props_.size();
    dirty_ = true;
    const size_t index = props_.size() - 1;
    Log::Info("Prop added index=" + std::to_string(index) + " identity=" + props_[index].library + "/" + props_[index].group + "/" + props_[index].name);
    return index;
}

bool MapDocument::HasNativeCollisionForProp(size_t index) const {
    if(index>=props_.size())return false;
    const int owner=static_cast<int>(index);
    auto owned=[&](const auto& list) {
        return std::any_of(list.begin(),list.end(),[&](const auto& c){return c.authoredOwnerIndex==owner;});
    };
    return owned(collisionPlanes_)||owned(collisionBoxes_)||owned(collisionTriangles_);
}

bool MapDocument::AddImportedCollisionForProp(size_t index,const NativeCollisionImport::Result& source) {
    if(index>=props_.size()||!source.Valid()||HasNativeCollisionForProp(index))return false;
    const auto& p=props_[index];
    // Never overwrite/duplicate a loaded map's unbound original colliders.
    if(p.legacySourceIndex>=0 || std::fabs(p.rotation.x)>1.e-5f || std::fabs(p.rotation.y)>1.e-5f)return false;
    // Identical co-located props have indistinguishable XML collision owners
    // after round trip; require the user to move one before collision authoring.
    for(size_t i=0;i<props_.size();++i)if(i!=index) {
        const auto& q=props_[i];
        if(q.library==p.library&&q.group==p.group&&q.name==p.name&&
           std::fabs(q.position.x-p.position.x)<.01f&&
           std::fabs(q.position.y-p.position.y)<.01f&&
           std::fabs(q.position.z-p.position.z)<.01f&&
           std::fabs(std::atan2(std::sin(q.rotation.z-p.rotation.z),
                   std::cos(q.rotation.z-p.rotation.z)))<.001f)return false;
    }
    const float cs=std::cos(p.rotation.z),sn=std::sin(p.rotation.z);
    const auto place=[&](DirectX::XMFLOAT3 o) {
        return DirectX::XMFLOAT3{p.position.x+cs*o.x-sn*o.y,
                                 p.position.y+sn*o.x+cs*o.y,p.position.z+o.z};
    };
    for(const auto& shape:source.planes) {
        CollisionPlane c;c.authoredOwnerIndex=static_cast<int>(index);c.transformDirty=true;
        c.position=place(shape.offset);c.rotation=shape.rotation;c.rotation.z+=p.rotation.z;
        c.width=shape.width;c.length=shape.length;collisionPlanes_.push_back(c);
    }
    for(const auto& shape:source.boxes) {
        CollisionBox c;c.authoredOwnerIndex=static_cast<int>(index);c.transformDirty=true;
        c.position=place(shape.offset);c.rotation=shape.rotation;c.rotation.z+=p.rotation.z;
        c.size=shape.size;collisionBoxes_.push_back(c);
    }
    for(const auto& shape:source.triangles) {
        CollisionTriangle c;c.authoredOwnerIndex=static_cast<int>(index);c.transformDirty=true;
        c.position=place(shape.offset);c.rotation=shape.rotation;c.rotation.z+=p.rotation.z;
        c.v0=shape.v0;c.v1=shape.v1;c.v2=shape.v2;collisionTriangles_.push_back(c);
    }
    stats_.collisionPlanes=collisionPlanes_.size();stats_.collisionBoxes=collisionBoxes_.size();
    stats_.collisionTriangles=collisionTriangles_.size();
    collisionDirty_=dirty_=true;
    Log::Info("3DS native helper collision authored: "+p.library+"/"+p.group+"/"+p.name+
        " planes="+std::to_string(source.planes.size())+
        " boxes="+std::to_string(source.boxes.size())+
        " triangles="+std::to_string(source.triangles.size())+
        " owner="+std::to_string(index)+" (native game validation still required)");
    return true;
}

bool MapDocument::BindImportedCollisionForProp(size_t index,const NativeCollisionImport::Result& source) {
    if(index>=props_.size()||!source.Valid()||HasNativeCollisionForProp(index))return false;
    const auto& p=props_[index];
    if(std::fabs(p.rotation.x)>1.e-5f||std::fabs(p.rotation.y)>1.e-5f)return false;
    const int owner=static_cast<int>(index);
    const float cs=std::cos(p.rotation.z),sn=std::sin(p.rotation.z);
    const auto place=[&](DirectX::XMFLOAT3 o) {
        return DirectX::XMFLOAT3{p.position.x+cs*o.x-sn*o.y,
            p.position.y+sn*o.x+cs*o.y,p.position.z+o.z};
    };
    const auto eq=[](DirectX::XMFLOAT3 a,DirectX::XMFLOAT3 b) {
        return std::fabs(a.x-b.x)<.09f&&std::fabs(a.y-b.y)<.09f&&std::fabs(a.z-b.z)<.09f;
    };
    // A native plane/triangle may have many equivalent Euler decompositions and
    // local vertex orders. Compare actual transformed WORLD geometry instead of
    // serialized rotations. This also binds original GTanks-authored instances.
    const auto rotate=[](DirectX::XMFLOAT3 v,DirectX::XMFLOAT3 r) {
        const float cx=std::cos(r.x),sx=std::sin(r.x),
                    cy=std::cos(r.y),sy=std::sin(r.y),
                    cz=std::cos(r.z),sz=std::sin(r.z);
        const DirectX::XMFLOAT3 x{v.x,cx*v.y-sx*v.z,sx*v.y+cx*v.z};
        const DirectX::XMFLOAT3 y{cy*x.x+sy*x.z,x.y,-sy*x.x+cy*x.z};
        return DirectX::XMFLOAT3{cz*y.x-sz*y.y,sz*y.x+cz*y.y,y.z};
    };
    const auto plus=[](DirectX::XMFLOAT3 a,DirectX::XMFLOAT3 b) {
        return DirectX::XMFLOAT3{a.x+b.x,a.y+b.y,a.z+b.z};
    };
    const auto normal=[&](const std::array<DirectX::XMFLOAT3,3>& v) {
        const auto u=NativeCollisionImport::Sub(v[1],v[0]);
        const auto w=NativeCollisionImport::Sub(v[2],v[0]);
        return NativeCollisionImport::Unit(NativeCollisionImport::Cross(u,w));
    };
    const auto sameWinding=[&](const std::array<DirectX::XMFLOAT3,3>& a,
                                const std::array<DirectX::XMFLOAT3,3>& b) {
        return NativeCollisionImport::Dot(normal(a),normal(b))>.999f;
    };
    const auto unorderedEqual=[&](const auto& a,const auto& b) {
        std::array<bool,4> used{};
        for(const auto& vertex:a) {
            bool found=false;
            for(size_t k=0;k<b.size();++k)if(!used[k]&&eq(vertex,b[k])) {
                used[k]=true;found=true;break;
            }
            if(!found)return false;
        }
        return true;
    };
    const auto planeCorners=[&](DirectX::XMFLOAT3 center,DirectX::XMFLOAT3 rot,float width,float length) {
        std::array<DirectX::XMFLOAT3,4> out{};
        for(int i=0;i<4;++i) {
            const DirectX::XMFLOAT3 local{((i&1)?1.f:-1.f)*width*.5f,
                ((i&2)?1.f:-1.f)*length*.5f,0.f};
            out[static_cast<size_t>(i)]=plus(center,rotate(local,rot));
        }
        return out;
    };
    const auto triangleVerts=[&](DirectX::XMFLOAT3 center,DirectX::XMFLOAT3 rot,
                                 DirectX::XMFLOAT3 a,DirectX::XMFLOAT3 b,DirectX::XMFLOAT3 c) {
        return std::array<DirectX::XMFLOAT3,3>{plus(center,rotate(a,rot)),
            plus(center,rotate(b,rot)),plus(center,rotate(c,rot))};
    };
    // Identically placed props cannot be distinguished by an XML collider owner.
    for(size_t j=0;j<props_.size();++j)if(j!=index) {
        const auto& q=props_[j];
        if(q.library==p.library&&q.group==p.group&&q.name==p.name&&eq(q.position,p.position)&&
            std::fabs(std::atan2(std::sin(q.rotation.z-p.rotation.z),
                                 std::cos(q.rotation.z-p.rotation.z)))<.002f)return false;
    }
    std::vector<size_t> planes,boxes,triangles;
    for(const auto& expected:source.planes) {
        const auto center=place(expected.offset);
        auto rotation=expected.rotation;rotation.z+=p.rotation.z;
        const auto want=planeCorners(center,rotation,expected.width,expected.length);
        const auto wantNormal=rotate({0.f,0.f,1.f},rotation);
        size_t found=collisionPlanes_.size();
        for(size_t j=0;j<collisionPlanes_.size();++j) {
            const auto& c=collisionPlanes_[j];
            if(c.authoredOwnerIndex>=0||!eq(c.position,center)||
               NativeCollisionImport::Dot(wantNormal,rotate({0.f,0.f,1.f},c.rotation))<.999f||
               !unorderedEqual(want,planeCorners(c.position,c.rotation,c.width,c.length)))continue;
            if(found!=collisionPlanes_.size())return false; // Ambiguous duplicate.
            found=j;
        }
        if(found==collisionPlanes_.size()||std::find(planes.begin(),planes.end(),found)!=planes.end())return false;
        planes.push_back(found);
    }
    for(const auto& expected:source.boxes) {
        const auto center=place(expected.offset);
        const float yaw=expected.rotation.z+p.rotation.z;
        size_t found=collisionBoxes_.size();
        for(size_t j=0;j<collisionBoxes_.size();++j) {
            const auto& c=collisionBoxes_[j];
            if(c.authoredOwnerIndex>=0||!eq(c.position,center)||!eq(c.size,expected.size)||
               std::fabs(std::atan2(std::sin(c.rotation.z-yaw),std::cos(c.rotation.z-yaw)))>.002f||
               std::fabs(c.rotation.x-expected.rotation.x)>.002f||
               std::fabs(c.rotation.y-expected.rotation.y)>.002f)continue;
            if(found!=collisionBoxes_.size())return false;
            found=j;
        }
        if(found==collisionBoxes_.size()||std::find(boxes.begin(),boxes.end(),found)!=boxes.end())return false;
        boxes.push_back(found);
    }
    for(const auto& expected:source.triangles) {
        const auto center=place(expected.offset);
        auto rotation=expected.rotation;rotation.z+=p.rotation.z;
        const auto want=triangleVerts(center,rotation,expected.v0,expected.v1,expected.v2);
        size_t found=collisionTriangles_.size();
        for(size_t j=0;j<collisionTriangles_.size();++j) {
            const auto& c=collisionTriangles_[j];
            if(c.authoredOwnerIndex>=0||!eq(c.position,center))continue;
            const auto actual=triangleVerts(c.position,c.rotation,c.v0,c.v1,c.v2);
            if(!unorderedEqual(want,actual)||!sameWinding(want,actual))continue;
            if(found!=collisionTriangles_.size())return false;
            found=j;
        }
        if(found==collisionTriangles_.size()||std::find(triangles.begin(),triangles.end(),found)!=triangles.end())return false;
        triangles.push_back(found);
    }
    for(const auto i:planes)collisionPlanes_[i].authoredOwnerIndex=owner;
    for(const auto i:boxes)collisionBoxes_[i].authoredOwnerIndex=owner;
    for(const auto i:triangles)collisionTriangles_[i].authoredOwnerIndex=owner;
    return true;
}

bool MapDocument::HasVerifiedCollisionForProp(size_t index) const {
    if(index>=props_.size())return false;
    const int owner=static_cast<int>(index);
    const auto planeCount=std::count_if(collisionPlanes_.begin(),collisionPlanes_.end(),
        [owner](const auto& c){return c.authoredOwnerIndex==owner;});
    const auto triangleCount=std::count_if(collisionTriangles_.begin(),collisionTriangles_.end(),
        [owner](const auto& c){return c.authoredOwnerIndex==owner;});
    return planeCount==6 && triangleCount==10;
}

bool MapDocument::AddVerifiedCollisionForProp(size_t index) {
    if(index>=props_.size())return false;
    const auto& p=props_[index];
    if(!VerifiedCollisionTemplates::Available(p.library,p.group,p.name) ||
        HasVerifiedCollisionForProp(index))return false;
    // An older map can contain a partial or nearby ORIGINAL collider set. When
    // explicitly repairing a loaded (source-indexed) prop, never duplicate it
    // if any native primitive has the same verified position AND shape.
    if(p.legacySourceIndex>=0) {
        const float cs=std::cos(p.rotation.z),sn=std::sin(p.rotation.z);
        auto place=[&](const DirectX::XMFLOAT3& v) {
            return DirectX::XMFLOAT3{p.position.x+cs*v.x-sn*v.y,
                p.position.y+sn*v.x+cs*v.y,p.position.z+v.z};
        };
        // Windows headers can define `near` as an empty legacy macro.
        // Use an unambiguous identifier so this check compiles under MSVC.
        auto nearVec3=[](const DirectX::XMFLOAT3& a,const DirectX::XMFLOAT3& b) {
            return std::fabs(a.x-b.x)<0.1f&&std::fabs(a.y-b.y)<0.1f&&std::fabs(a.z-b.z)<0.1f;
        };
        auto sameYaw=[](float a,float b) {
            return std::fabs(std::atan2(std::sin(a-b),std::cos(a-b)))<0.001f;
        };
        for(const auto& t:VerifiedCollisionTemplates::BeachWallEnd2Planes) {
            const auto expected=place(t.offset);
            for(const auto& c:collisionPlanes_)
                if(nearVec3(c.position,expected) && std::fabs(c.width-t.width)<.1f &&
                   std::fabs(c.length-t.length)<.1f &&
                   sameYaw(c.rotation.z,t.rotation.z+p.rotation.z)) {
                    Log::Warning("Verified wall collision repair refused: existing matching plane could be shared/partial.");
                    return false;
                }
        }
        for(const auto& t:VerifiedCollisionTemplates::BeachWallEnd2Triangles) {
            const auto expected=place(t.offset);
            for(const auto& c:collisionTriangles_)
                if(nearVec3(c.position,expected) && nearVec3(c.v0,t.v0) && nearVec3(c.v1,t.v1) &&
                   nearVec3(c.v2,t.v2) && sameYaw(c.rotation.z,t.rotation.z+p.rotation.z)) {
                    Log::Warning("Verified wall collision repair refused: existing matching triangle could be shared/partial.");
                    return false;
                }
        }
    }
    // Source template is for a zero-yaw instance. Native Z-up X/Y offsets
    // rotate around the placed prop origin, never around the map origin.
    const float cs=std::cos(p.rotation.z),sn=std::sin(p.rotation.z);
    auto place=[&](DirectX::XMFLOAT3 offset) {
        return DirectX::XMFLOAT3{p.position.x+cs*offset.x-sn*offset.y,
            p.position.y+sn*offset.x+cs*offset.y,p.position.z+offset.z};
    };
    for(const auto& source:VerifiedCollisionTemplates::BeachWallEnd2Planes) {
        CollisionPlane v;v.authoredOwnerIndex=static_cast<int>(index);
        v.position=place(source.offset);v.rotation=source.rotation;
        v.rotation.z+=p.rotation.z;v.width=source.width;v.length=source.length;
        v.transformDirty=true;collisionPlanes_.push_back(v);
    }
    for(const auto& source:VerifiedCollisionTemplates::BeachWallEnd2Triangles) {
        CollisionTriangle v;v.authoredOwnerIndex=static_cast<int>(index);
        v.position=place(source.offset);v.rotation=source.rotation;v.rotation.z+=p.rotation.z;
        v.v0=source.v0;v.v1=source.v1;v.v2=source.v2;v.transformDirty=true;
        collisionTriangles_.push_back(v);
    }
    stats_.collisionPlanes=collisionPlanes_.size();stats_.collisionTriangles=collisionTriangles_.size();
    collisionDirty_=dirty_=true;
    Log::Info("Verified native collision authored: Beach/sidewalls/Wall End 2 planes=6 triangles=10 owner="+std::to_string(index));
    return true;
}

bool MapDocument::DeleteProp(size_t index) {
    if (index >= props_.size()) return false;
    // Explicit author ownership survives overlaps and protects unrelated native
    // colliders. Direct DeleteProp and group delete both use this same cleanup.
    auto eraseAuthored=[&](auto& items) {
        const auto oldSize=items.size();
        items.erase(std::remove_if(items.begin(),items.end(),[&](const auto& c) {
            return c.authoredOwnerIndex==static_cast<int>(index);
        }),items.end());
        for(auto& c:items)if(c.authoredOwnerIndex>static_cast<int>(index))--c.authoredOwnerIndex;
        if(items.size()!=oldSize)collisionDirty_=true;
    };
    eraseAuthored(collisionPlanes_);eraseAuthored(collisionBoxes_);eraseAuthored(collisionTriangles_);
    stats_.collisionPlanes=collisionPlanes_.size();stats_.collisionBoxes=collisionBoxes_.size();
    stats_.collisionTriangles=collisionTriangles_.size();
    Log::Info("Prop deleted index=" + std::to_string(index) + " identity=" + props_[index].library + "/" + props_[index].group + "/" + props_[index].name);
    props_.erase(props_.begin() + static_cast<std::ptrdiff_t>(index));
    propTransformDirty_.erase(propTransformDirty_.begin() + static_cast<std::ptrdiff_t>(index));
    stats_.props = props_.size();
    dirty_ = true;
    return true;
}


bool MapDocument::SetFlagPosition(size_t index, const DirectX::XMFLOAT3& pos) {
    if(index>=ctfFlags_.size()) return false;
    ctfFlags_[index].position=pos; flagsDirty_=dirty_=true; return true;
}
bool MapDocument::AddFlag(const std::string& team, const DirectX::XMFLOAT3& pos) {
    if(team!="red" && team!="blue") return false;
    for(const auto& flag:ctfFlags_) if(flag.team==team) return false; // one native flag per team
    CtfFlagMarker f; f.team=team; f.position=pos; ctfFlags_.push_back(f);
    stats_.ctfFlags=ctfFlags_.size(); flagsDirty_=dirty_=true; return true;
}
bool MapDocument::DeleteFlag(size_t index) {
    if(index>=ctfFlags_.size()) return false;
    ctfFlags_.erase(ctfFlags_.begin()+static_cast<std::ptrdiff_t>(index));
    stats_.ctfFlags=ctfFlags_.size(); flagsDirty_=dirty_=true; return true;
}
bool MapDocument::SetSpawn(size_t index, const SpawnMarker& value) {
    if(index>=spawns_.size()) return false;
    const int old=spawns_[index].legacySourceIndex; spawns_[index]=value; spawns_[index].legacySourceIndex=old;
    spawnsDirty_=dirty_=true; return true;
}
size_t MapDocument::AddSpawn(SpawnMarker value) {
    value.legacySourceIndex=-1; spawns_.push_back(std::move(value));
    stats_.spawnPoints=spawns_.size(); spawnsDirty_=dirty_=true; return spawns_.size()-1;
}
bool MapDocument::DeleteSpawn(size_t index) {
    if(index>=spawns_.size()) return false;
    spawns_.erase(spawns_.begin()+static_cast<std::ptrdiff_t>(index));
    stats_.spawnPoints=spawns_.size(); spawnsDirty_=dirty_=true; return true;
}
bool MapDocument::SetControlPoint(size_t index, const ControlPointMarker& value) {
    if(index>=controlPoints_.size()) return false;
    const int old=controlPoints_[index].legacySourceIndex; controlPoints_[index]=value; controlPoints_[index].legacySourceIndex=old;
    pointsDirty_=dirty_=true; return true;
}
size_t MapDocument::AddControlPoint(ControlPointMarker value) {
    value.legacySourceIndex=-1; controlPoints_.push_back(std::move(value));
    stats_.dominationPoints=controlPoints_.size(); pointsDirty_=dirty_=true; return controlPoints_.size()-1;
}
bool MapDocument::DeleteControlPoint(size_t index) {
    if(index>=controlPoints_.size()) return false;
    controlPoints_.erase(controlPoints_.begin()+static_cast<std::ptrdiff_t>(index));
    stats_.dominationPoints=controlPoints_.size(); pointsDirty_=dirty_=true; return true;
}
bool MapDocument::SetBonusRegion(size_t index, const BonusRegionMarker& value) {
    if(index>=bonuses_.size()) return false;
    const int old=bonuses_[index].legacySourceIndex; bonuses_[index]=value; bonuses_[index].legacySourceIndex=old;
    bonusesDirty_=dirty_=true; return true;
}
size_t MapDocument::AddBonusRegion(BonusRegionMarker value) {
    value.legacySourceIndex=-1; bonuses_.push_back(std::move(value));
    stats_.bonusRegions=bonuses_.size(); bonusesDirty_=dirty_=true; return bonuses_.size()-1;
}
bool MapDocument::DeleteBonusRegion(size_t index) {
    if(index>=bonuses_.size()) return false;
    bonuses_.erase(bonuses_.begin()+static_cast<std::ptrdiff_t>(index));
    stats_.bonusRegions=bonuses_.size(); bonusesDirty_=dirty_=true; return true;
}
bool MapDocument::SetSpecialBox(size_t index, const SpecialBox& value) {
    if(index>=specialBoxes_.size()) return false;
    const int old=specialBoxes_[index].legacySourceIndex; specialBoxes_[index]=value; specialBoxes_[index].legacySourceIndex=old;
    zonesDirty_=dirty_=true; return true;
}
size_t MapDocument::AddSpecialBox(SpecialBox value) {
    value.legacySourceIndex=-1; specialBoxes_.push_back(std::move(value));
    stats_.specialBoxes=specialBoxes_.size(); zonesDirty_=dirty_=true; return specialBoxes_.size()-1;
}
bool MapDocument::DeleteSpecialBox(size_t index) {
    if(index>=specialBoxes_.size()) return false;
    specialBoxes_.erase(specialBoxes_.begin()+static_cast<std::ptrdiff_t>(index));
    stats_.specialBoxes=specialBoxes_.size(); zonesDirty_=dirty_=true; return true;
}

bool MapDocument::SetLight(size_t index,const LightMarker& value) {
    if(index>=lights_.size() || value.type!="omni" || value.color>0xFFFFFFu ||
       !std::isfinite(value.intensity) || value.intensity<0 ||
       !std::isfinite(value.attenuationBegin) || value.attenuationBegin<0 ||
       !std::isfinite(value.attenuationEnd) || value.attenuationEnd<=value.attenuationBegin ||
       !std::isfinite(value.rotationZ) ||
       !std::isfinite(value.position.x)||!std::isfinite(value.position.y)||!std::isfinite(value.position.z))return false;
    const int original=lights_[index].legacySourceIndex;
    lights_[index]=value;lights_[index].legacySourceIndex=original;
    lightsDirty_=dirty_=true;return true;
}
size_t MapDocument::AddLight(LightMarker value) {
    // All supplied native map examples are omni; no invented light types.
    if(value.type!="omni" || value.color>0xFFFFFFu ||
       !std::isfinite(value.intensity)||value.intensity<0 ||
       !std::isfinite(value.attenuationBegin)||value.attenuationBegin<0 ||
       !std::isfinite(value.attenuationEnd)||value.attenuationEnd<=value.attenuationBegin ||
       !std::isfinite(value.position.x)||!std::isfinite(value.position.y)||!std::isfinite(value.position.z)||
       !std::isfinite(value.rotationZ))return lights_.size();
    value.legacySourceIndex=-1;lights_.push_back(std::move(value));
    stats_.lights=lights_.size();lightsDirty_=dirty_=true;return lights_.size()-1;
}
bool MapDocument::DeleteLight(size_t index) {
    if(index>=lights_.size())return false;
    lights_.erase(lights_.begin()+static_cast<std::ptrdiff_t>(index));
    stats_.lights=lights_.size();lightsDirty_=dirty_=true;return true;
}

bool MapDocument::SerializeLegacy(std::string& xml, std::string& error) const {
    if (sourceXml_->empty()) { error = "No legacy map document is loaded."; return false; }
    if (!dirty_) { xml = *sourceXml_; error.clear(); return true; }

    pugi::xml_document doc;
    const auto result = doc.load_buffer(sourceXml_->data(), sourceXml_->size(), pugi::parse_full, pugi::encoding_utf8);
    if (!result) { error = std::string("Legacy master XML could not be reparsed: ") + result.description(); return false; }
    auto map = doc.child("map");
    auto geometry = map.child("static-geometry");
    if (!map || !geometry) { error = "Legacy master is missing <map> or <static-geometry>."; return false; }

    // Edit retained props IN PLACE. Older versions rebuilt the whole prop list
    // after unrelated children; that needlessly reordered extension nodes in
    // original maps. Keep unknown siblings, attributes and child fields intact.
    std::vector<pugi::xml_node> originalNodes;
    for (auto node : geometry.children("prop")) originalNodes.push_back(node);
    const size_t originalCount = originalNodes.size();
    std::vector<bool> retained(originalCount,false);
    int previousSourceIndex=-1;
    for (const auto& p:props_) if (p.legacySourceIndex >= 0) {
        const size_t source=static_cast<size_t>(p.legacySourceIndex);
        if(source>=originalCount || retained[source] || p.legacySourceIndex<=previousSourceIndex) {
            error="Compatibility guard stopped export: stale, repeated or reordered source prop.";
            return false;
        }
        retained[source]=true;
        previousSourceIndex=p.legacySourceIndex;
    }

    for (size_t propIndex = 0; propIndex < props_.size(); ++propIndex) {
        const auto& p = props_[propIndex];
        pugi::xml_node node;
        if (p.legacySourceIndex >= 0 && static_cast<size_t>(p.legacySourceIndex) < originalCount) {
            node=originalNodes[static_cast<size_t>(p.legacySourceIndex)];
        } else if (p.legacySourceIndex < 0) {
            if(p.hasInvalidNativeMetadata) {
                error="Compatibility guard stopped export: malformed or repeated native prop fields cannot be duplicated.";
                return false;
            }
            if(p.hasUncopyableMetadata && !p.allowOpaqueMetadataCopy) {
                error="Compatibility guard stopped export: copied prop has unknown source metadata; enable explicit opaque XML copy or keep the original node.";
                return false;
            }
            if(p.nativeWithCollision==1 && !HasNativeCollisionForProp(propIndex)) {
                error="Compatibility guard stopped export: newly copied prop has native with_collision=1 but no owned collision primitives.";
                return false;
            }
            if(p.originalPropXml && !p.originalPropXml->empty()) {
                // A source-instance copy is a complete detached native subtree;
                // never synthesize it from a partial C++ struct. Verify the
                // identity before reusing it (clipboard may outlive a reload).
                pugi::xml_document clone;
                const auto parsed=clone.load_buffer(p.originalPropXml->data(),
                    p.originalPropXml->size(),pugi::parse_full,pugi::encoding_utf8);
                const auto source=clone.child("prop");
                if(!parsed || !source || std::string(source.attribute("library-name").value())!=p.library ||
                    std::string(source.attribute("group-name").value())!=p.group ||
                    std::string(source.attribute("name").value())!=p.name) {
                    error="Compatibility guard stopped export: copied original prop XML is invalid or its identity changed.";
                    return false;
                }
                node=geometry.append_copy(source);
                if(!node) {error="Could not duplicate the complete native prop XML.";return false;}
                // Preserve all opaque children and attributes, but allow the
                // typed per-instance fields to reflect intentional edits.
                auto texture=node.child("texture-name");
                if(!texture)texture=node.append_child("texture-name");
                if(std::string(texture.text().as_string())!=p.texture)texture.text().set(p.texture.c_str());
                if(p.nativeWithCollision >= 0) {
                    auto flag=node.child("with_collision");
                    if(!flag)flag=node.append_child("with_collision");
                    const std::string desired=p.nativeWithCollision==1?"1":"0";
                    if(std::string(flag.text().as_string())!=desired)flag.text().set(desired.c_str());
                }
                if(p.nativeFree >= 0) {
                    auto free=node.attribute("free");
                    if(!free)free=node.append_attribute("free");
                    if(free.as_bool(false)!=(p.nativeFree==1))free.set_value(p.nativeFree==1);
                }
            } else {
                // Brand-new placement: the native library definition stays in
                // library.xml; a map instance only references its identity.
                // Do not invent extra gameplay flags from a library template.
                if(p.hasUncopyableMetadata) {
                    error="Compatibility guard stopped export: opaque metadata has no original source prop XML.";
                    return false;
                }
                node=geometry.append_child("prop");
                node.append_attribute("library-name").set_value(p.library.c_str());
                node.append_attribute("group-name").set_value(p.group.c_str());
                node.append_attribute("name").set_value(p.name.c_str());
                if(p.nativeFree >= 0)node.append_attribute("free").set_value(p.nativeFree==1);
                node.append_child("rotation").append_child("z").text().set(fixed(p.rotation.z,6).c_str());
                node.append_child("texture-name").text().set(p.texture.c_str());
                auto position=node.append_child("position");
                position.append_child("x").text().set(fixed(p.position.x,3).c_str());
                position.append_child("y").text().set(fixed(p.position.y,3).c_str());
                position.append_child("z").text().set(fixed(p.position.z,3).c_str());
                if(p.nativeWithCollision >= 0)
                    node.append_child("with_collision").text().set(p.nativeWithCollision==1?"1":"0");
            }
        }

        else {
            error = "Compatibility guard stopped export: stale original prop index.";
            return false;
        }

        if (propTransformDirty_[propIndex] || p.legacySourceIndex < 0) {
            auto rotation = node.child("rotation");
            if (!rotation) rotation = node.prepend_child("rotation");
            // Preserve the original editor convention: static props normally store only rotation/z.
            if (rotation.child("x")) setExisting(rotation, "x", p.rotation.x, 6);
            if (rotation.child("y")) setExisting(rotation, "y", p.rotation.y, 6);
            setOrAppend(rotation, "z", p.rotation.z, 6);

            auto position = node.child("position");
            if (!position) position = node.append_child("position");
            setOrAppend(position, "x", p.position.x, 3);
            setOrAppend(position, "y", p.position.y, 3);
            setOrAppend(position, "z", p.position.z, 3);
        }
    }
    for(size_t i=0;i<originalCount;++i) if(!retained[i])geometry.remove_child(originalNodes[i]);

    if(collisionDirty_) {
        auto collision=map.child("collision-geometry");
        if(!collision) {error="Legacy collision section is missing.";return false;}
        // Edit/remove native nodes IN PLACE. Appending every retained collider
        // after the extension nodes silently changes the original XML order.
        // Preserve the order, attributes and unknown siblings in original maps.
        const auto countNodes=[&](const char* tag){return childCount(collision,tag);};
        const size_t sourcePlanes=countNodes("collision-plane");
        const size_t sourceBoxes=countNodes("collision-box");
        const size_t sourceTriangles=countNodes("collision-triangle");
        std::vector<const CollisionPlane*> planes(sourcePlanes,nullptr);
        std::vector<const CollisionBox*> boxes(sourceBoxes,nullptr);
        std::vector<const CollisionTriangle*> triangles(sourceTriangles,nullptr);
        for(const auto& p:collisionPlanes_) {
            if(p.legacySourceIndex<0)continue; // New verified native primitive; append after existing nodes.
            if(p.legacySourceIndex<0 || static_cast<size_t>(p.legacySourceIndex)>=sourcePlanes ||
                planes[static_cast<size_t>(p.legacySourceIndex)]) {
                error="Collision plane source index is invalid or duplicated: export stopped.";return false;
            }
            planes[static_cast<size_t>(p.legacySourceIndex)]=&p;
        }
        for(const auto& b:collisionBoxes_) {
            if(b.legacySourceIndex<0)continue; // New verified native primitive; append after existing nodes.
            if(b.legacySourceIndex<0 || static_cast<size_t>(b.legacySourceIndex)>=sourceBoxes ||
                boxes[static_cast<size_t>(b.legacySourceIndex)]) {
                error="Collision box source index is invalid or duplicated: export stopped.";return false;
            }
            boxes[static_cast<size_t>(b.legacySourceIndex)]=&b;
        }
        for(const auto& t:collisionTriangles_) {
            if(t.legacySourceIndex<0)continue; // New verified native primitive; append after existing nodes.
            if(t.legacySourceIndex<0 || static_cast<size_t>(t.legacySourceIndex)>=sourceTriangles ||
                triangles[static_cast<size_t>(t.legacySourceIndex)]) {
                error="Collision triangle source index is invalid or duplicated: export stopped.";return false;
            }
            triangles[static_cast<size_t>(t.legacySourceIndex)]=&t;
        }
        size_t planeIndex=0,boxIndex=0,triangleIndex=0;
        for(auto node=collision.first_child();node;) {
            auto next=node.next_sibling();
            const std::string tag=node.name();
            if(tag=="collision-plane") {
                const auto* p=planes[planeIndex++];
                if(!p) collision.remove_child(node);
                else if(p->transformDirty) {
                    auto pos=node.child("position");if(!pos)pos=node.append_child("position");
                    setOrAppend(pos,"x",p->position.x,3);
                    setOrAppend(pos,"y",p->position.y,3);
                    setOrAppend(pos,"z",p->position.z,3);
                    auto rot=node.child("rotation");if(!rot)rot=node.append_child("rotation");
                    setOrAppend(rot,"z",p->rotation.z,6);
                }
            } else if(tag=="collision-box") {
                const auto* b=boxes[boxIndex++];
                if(!b) collision.remove_child(node);
                else if(b->transformDirty) {
                    auto pos=node.child("position");if(!pos)pos=node.append_child("position");
                    setOrAppend(pos,"x",b->position.x,3);
                    setOrAppend(pos,"y",b->position.y,3);
                    setOrAppend(pos,"z",b->position.z,3);
                    auto rot=node.child("rotation");if(!rot)rot=node.append_child("rotation");
                    setOrAppend(rot,"z",b->rotation.z,6);
                }
            } else if(tag=="collision-triangle") {
                const auto* t=triangles[triangleIndex++];
                if(!t)collision.remove_child(node);
                else if(t->transformDirty) {
                    auto pos=node.child("position");if(!pos)pos=node.append_child("position");
                    setOrAppend(pos,"x",t->position.x,3);
                    setOrAppend(pos,"y",t->position.y,3);
                    setOrAppend(pos,"z",t->position.z,3);
                    auto rot=node.child("rotation");if(!rot)rot=node.append_child("rotation");
                    setOrAppend(rot,"z",t->rotation.z,6);
                }
            }
            node=next;
        }
        auto putVec=[](pugi::xml_node n,const DirectX::XMFLOAT3& v,int precision) {
            setOrAppend(n,"x",v.x,precision);setOrAppend(n,"y",v.y,precision);setOrAppend(n,"z",v.z,precision);
        };
        for(const auto& p:collisionPlanes_)if(p.legacySourceIndex<0) {
            auto n=collision.append_child("collision-plane");n.append_attribute("id").set_value(0);
            setOrAppend(n,"width",p.width,3);setOrAppend(n,"length",p.length,3);
            putVec(n.append_child("position"),p.position,3);
            putVec(n.append_child("rotation"),p.rotation,6);
        }
        for(const auto& b:collisionBoxes_)if(b.legacySourceIndex<0) {
            auto n=collision.append_child("collision-box");n.append_attribute("id").set_value(0);
            putVec(n.append_child("size"),b.size,3);
            putVec(n.append_child("position"),b.position,3);
            putVec(n.append_child("rotation"),b.rotation,6);
        }
        for(const auto& t:collisionTriangles_)if(t.legacySourceIndex<0) {
            auto n=collision.append_child("collision-triangle");n.append_attribute("id").set_value(0);
            putVec(n.append_child("v0"),t.v0,3);putVec(n.append_child("v1"),t.v1,3);
            putVec(n.append_child("v2"),t.v2,3);
            putVec(n.append_child("position"),t.position,3);
            putVec(n.append_child("rotation"),t.rotation,6);
        }
    }

    // Copy original functional nodes before rebuilding only sections that were actually edited.
    // Existing node contents/extra attributes remain intact when their legacy source index survives.
    auto originalChildren=[](pugi::xml_node parent, const char* tag, pugi::xml_document& copied) {
        auto root=copied.append_child("original");
        for(auto node:parent.children(tag)) root.append_copy(node);
        return root;
    };
    auto originalAt=[](pugi::xml_node root, const char* tag, int index) -> pugi::xml_node {
        if(index<0) return {};
        int current=0;
        for(auto child:root.children(tag)) if(current++==index) return child;
        return {};
    };
    auto setVec=[](pugi::xml_node node, const DirectX::XMFLOAT3& v) {
        setOrAppend(node,"x",v.x,3); setOrAppend(node,"y",v.y,3); setOrAppend(node,"z",v.z,3);
    };
    if(flagsDirty_) {
        auto section=map.child("ctf-flags"); if(!section) section=map.append_child("ctf-flags");
        pugi::xml_document originalsFlags;
        auto old=originalsFlags.append_child("original");
        for(auto n:section.children()) old.append_copy(n);
        while(section.child("flag-red")) section.remove_child(section.child("flag-red"));
        while(section.child("flag-blue")) section.remove_child(section.child("flag-blue"));
        for(const auto& f:ctfFlags_) {
            const char* tag=f.team=="red"?"flag-red":"flag-blue";
            pugi::xml_node n=originalAt(old,tag,f.legacySourceIndex);
            if(!n) n=old.child(tag);
            auto dst=n?section.append_copy(n):section.append_child(tag);
            setVec(dst,f.position);
        }
    }
    if(spawnsDirty_) {
        auto section=map.child("spawn-points"); if(!section) section=map.append_child("spawn-points");
        pugi::xml_document oldDoc; auto old=originalChildren(section,"spawn-point",oldDoc);
        while(section.child("spawn-point")) section.remove_child(section.child("spawn-point"));
        for(const auto& spawn:spawns_) {
            auto original=originalAt(old,"spawn-point",spawn.legacySourceIndex);
            auto dst=original?section.append_copy(original):section.append_child("spawn-point");
            if(!spawn.type.empty()) { auto a=dst.attribute("type"); if(!a) a=dst.append_attribute("type"); a.set_value(spawn.type.c_str()); }
            if(!spawn.team.empty()) { auto a=dst.attribute("team"); if(!a) a=dst.append_attribute("team"); a.set_value(spawn.team.c_str()); }
            auto position=dst.child("position"); if(!position) position=dst.append_child("position"); setVec(position,spawn.position);
            auto rotation=dst.child("rotation"); if(!rotation) rotation=dst.prepend_child("rotation");
            setOrAppend(rotation,"z",spawn.rotationZ,6);
        }
    }
    if(pointsDirty_) {
        auto section=map.child("dom-keypoints"); if(!section) section=map.append_child("dom-keypoints");
        pugi::xml_document oldDoc; auto old=originalChildren(section,"dom-keypoint",oldDoc);
        while(section.child("dom-keypoint")) section.remove_child(section.child("dom-keypoint"));
        for(const auto& point:controlPoints_) {
            auto original=originalAt(old,"dom-keypoint",point.legacySourceIndex);
            auto dst=original?section.append_copy(original):section.append_child("dom-keypoint");
            auto name=dst.attribute("name"); if(!name) name=dst.append_attribute("name"); name.set_value(point.name.c_str());
            auto distance=dst.attribute("distance"); if(!distance) distance=dst.append_attribute("distance"); distance.set_value(fixed(point.distance,3).c_str());
            auto free=dst.attribute("free"); if(point.free || free) { if(!free) free=dst.append_attribute("free"); free.set_value(point.free); }
            auto position=dst.child("position"); if(!position) position=dst.append_child("position"); setVec(position,point.position);
        }
    }
    if(bonusesDirty_) {
        auto section=map.child("bonus-regions"); if(!section) section=map.append_child("bonus-regions");
        pugi::xml_document oldDoc; auto old=originalChildren(section,"bonus-region",oldDoc);
        while(section.child("bonus-region")) section.remove_child(section.child("bonus-region"));
        for(const auto& region:bonuses_) {
            auto original=originalAt(old,"bonus-region",region.legacySourceIndex);
            auto dst=original?section.append_copy(original):section.append_child("bonus-region");
            // Preserve attribute absence for existing legacy records unless user
            // changes its value; new volumes explicitly record both native flags.
            for(const auto [key,value]: {std::pair<const char*,bool>{"parachute",region.parachute},
                                         std::pair<const char*,bool>{"free",region.free}}) {
                auto attr=dst.attribute(key);
                if(attr || !original || !value) {
                    if(!attr)attr=dst.append_attribute(key);
                    attr.set_value(value);
                }
            }
            auto name=dst.attribute("name"); if(!name) name=dst.append_attribute("name"); name.set_value(region.name.c_str());
            auto min=dst.child("min"); if(!min) min=dst.append_child("min"); setVec(min,region.min);
            auto max=dst.child("max"); if(!max) max=dst.append_child("max"); setVec(max,region.max);
            // Original bonus regions carry a position equal to their minimum and a Z rotation.
            auto position=dst.child("position"); if(!position) position=dst.append_child("position"); setVec(position,region.min);
            if(!original) {auto rotation=dst.prepend_child("rotation");setOrAppend(rotation,"z",0.0f,3);}
            auto type=dst.child("bonus-type"); if(!type) type=dst.append_child("bonus-type"); type.text().set(region.bonusType.c_str());
            // Update mode nodes when edited, without altering untouched legacy XML.
            std::vector<std::string> savedModes;
            for(auto mode:dst.children("game-mode")) savedModes.emplace_back(mode.text().as_string());
            if(savedModes!=region.modes) {
                while(dst.child("game-mode")) dst.remove_child(dst.child("game-mode"));
                for(const auto& mode:region.modes)dst.append_child("game-mode").text().set(mode.c_str());
            }
        }
    }
    if(lightsDirty_) {
        auto section=map.child("lights");if(!section)section=map.append_child("lights");
        const size_t sourceCount=childCount(section,"light");
        std::vector<const LightMarker*> indexed(sourceCount,nullptr);
        for(const auto& light:lights_) if(light.legacySourceIndex>=0) {
            const size_t i=static_cast<size_t>(light.legacySourceIndex);
            if(i>=sourceCount || indexed[i]) {
                error="Invalid or duplicated native light source index: export stopped.";return false;
            }
            indexed[i]=&light;
        }
        auto updateNative=[&](pugi::xml_node dst,const LightMarker& light) {
            auto setAttribute=[&](const char* name,const char* value) {
                auto a=dst.attribute(name);if(!a)a=dst.append_attribute(name);a.set_value(value);
            };
            setAttribute("type",light.type.c_str());
            setAttribute("color",std::to_string(light.color).c_str());
            setAttribute("intensity",fixed(light.intensity,6).c_str());
            setAttribute("attenuationBegin",fixed(light.attenuationBegin,6).c_str());
            setAttribute("attenuationEnd",fixed(light.attenuationEnd,6).c_str());
            auto rot=dst.child("rotation");if(!rot)rot=dst.append_child("rotation");
            setOrAppend(rot,"z",light.rotationZ,6);
            auto pos=dst.child("position");if(!pos)pos=dst.append_child("position");
            setVec(pos,light.position);
        };
        // Change the existing nodes IN PLACE, retaining mixed ordering, unknown
        // siblings and the original metadata. Do not recreate a light from an
        // approximate nearest static prop: there is no native ownership link.
        size_t index=0;
        for(auto node=section.first_child();node;) {
            auto next=node.next_sibling();
            if(std::string(node.name())=="light") {
                const LightMarker* item=indexed[index++];
                if(!item)section.remove_child(node);
                else if(item->type=="omni")updateNative(node,*item);
                // Unverified native types remain byte-equivalent at node level.
            }
            node=next;
        }
        for(const auto& light:lights_) if(light.legacySourceIndex<0) {
            if(light.type!="omni") {error="Unsupported light type cannot be authored.";return false;}
            auto node=section.append_child("light");updateNative(node,light);
        }
    }
    if(zonesDirty_) {
        auto section=map.child("special-geometry"); if(!section) section=map.append_child("special-geometry");
        pugi::xml_document oldDoc; auto old=originalChildren(section,"special-box",oldDoc);
        while(section.child("special-box")) section.remove_child(section.child("special-box"));
        for(const auto& box:specialBoxes_) {
            auto original=originalAt(old,"special-box",box.legacySourceIndex);
            auto dst=original?section.append_copy(original):section.append_child("special-box");
            if(!original) dst.append_attribute("type").set_value("special-geometry");
            setOrAppend(dst,"minX",box.min.x,3); setOrAppend(dst,"minY",box.min.y,3); setOrAppend(dst,"minZ",box.min.z,3);
            setOrAppend(dst,"maxX",box.max.x,3); setOrAppend(dst,"maxY",box.max.y,3); setOrAppend(dst,"maxZ",box.max.z,3);
            auto action=dst.child("action"); if(!action) action=dst.append_child("action"); action.text().set(box.action.c_str());
            if(box.free || dst.attribute("free")) { auto free=dst.attribute("free"); if(!free) free=dst.append_attribute("free"); free.set_value(box.free); }
        }
    }

    std::ostringstream out;
    doc.save(out, "  ", pugi::format_indent | pugi::format_no_declaration, pugi::encoding_utf8);
    xml = out.str();
    normalizeLegacySelfClosing(xml);

    pugi::xml_document verify;
    const auto verified = verify.load_buffer(xml.data(), xml.size(), pugi::parse_full, pugi::encoding_utf8);
    auto verifyMap = verify.child("map");
    if (!verified || !verifyMap || childCount(verifyMap.child("static-geometry"), "prop") != props_.size()) {
        error = "Compatibility guard stopped export: serialized legacy map failed structural validation.";
        return false;
    }
    const auto collision = verifyMap.child("collision-geometry");
    const bool preserved =
        std::string(verifyMap.attribute("version").as_string()) == version_ &&
        childCount(collision, "collision-plane") == stats_.collisionPlanes &&
        childCount(collision, "collision-box") == stats_.collisionBoxes &&
        childCount(collision, "collision-triangle") == stats_.collisionTriangles &&
        childCount(verifyMap.child("spawn-points"), "spawn-point") == stats_.spawnPoints &&
        childCount(verifyMap.child("bonus-regions"), "bonus-region") == stats_.bonusRegions &&
        childCount(verifyMap.child("special-geometry"), "special-box") == stats_.specialBoxes &&
        childCount(verifyMap.child("lights"), "light") == stats_.lights &&
        childCount(verifyMap.child("way-points"), "way-point") == stats_.wayPoints &&
        childCount(verifyMap.child("dom-keypoints"), "dom-keypoint") == stats_.dominationPoints &&
        childCount(verifyMap.child("ctf-flags"), "flag-red") + childCount(verifyMap.child("ctf-flags"), "flag-blue") == stats_.ctfFlags;
    if (!preserved) {
        error = "Compatibility guard stopped export: a legacy gameplay/collision section changed unexpectedly.";
        return false;
    }

    error.clear();
    return true;
}

bool MapDocument::SaveLegacy(std::string& error) {
    if (path_.empty()) { error = "No output path is associated with this map."; return false; }
    return SaveLegacyAs(path_, error);
}

bool MapDocument::SaveLegacyAs(const std::filesystem::path& file, std::string& error) {
    Log::Info("Legacy save begin: " + Log::PathUtf8(file));
    std::string xml;
    if (!SerializeLegacy(xml, error)) return false;

    std::error_code ec;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) { error = "Could not create the destination folder: " + ec.message(); return false; }

    auto temp = file;
    temp += L".gtanks-next.tmp";
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output) { error = "Could not create temporary map file."; return false; }
        output.write(xml.data(), static_cast<std::streamsize>(xml.size()));
        output.flush();
        if (!output) { error = "Could not write complete map data."; output.close(); std::filesystem::remove(temp, ec); return false; }
    }

    // The first overwrite of an existing map keeps its original bytes. Never
    // overwrite a pre-existing backup on subsequent saves or repeat edits.
    ec.clear();
    const bool targetExists=std::filesystem::exists(file,ec);
    if(ec) {
        std::filesystem::remove(temp,ec);
        error="Could not check existing map before save.";return false;
    }
    if(targetExists) {
        auto backup=file; backup+=L".original.bak";
        ec.clear();const bool backupExists=std::filesystem::exists(backup,ec);
        if(ec) {
            std::filesystem::remove(temp,ec);
            error="Could not check original map backup.";return false;
        }
        if(!backupExists) {
            std::filesystem::copy_file(file,backup,std::filesystem::copy_options::none,ec);
            if(ec) {
                std::error_code ignored;std::filesystem::remove(temp,ignored);
                error="Could not create original map backup: "+ec.message();return false;
            }
            Log::Info("Original map backup: "+Log::PathUtf8(backup));
        }
    }

    if (!MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD code = GetLastError();
        std::filesystem::remove(temp, ec);
        error = "Could not replace the destination map file (Windows error " + std::to_string(code) + ").";
        return false;
    }

    path_ = file;
    sourceXml_ = std::make_shared<const std::string>(std::move(xml));
    pugi::xml_document refreshed;
    if(!refreshed.load_buffer(sourceXml_->data(),sourceXml_->size(),pugi::parse_full,pugi::encoding_utf8)) {
        error="Internal error: written map could not be reparsed for snapshot refresh.";
        return false;
    }
    auto savedProp=refreshed.child("map").child("static-geometry").child("prop");
    for (size_t i = 0; i < props_.size(); ++i) {
        if(!savedProp) {error="Internal error: saved prop snapshot count differs.";return false;}
        props_[i].legacySourceIndex = static_cast<int>(i);
        std::ostringstream raw;
        savedProp.print(raw,"",pugi::format_raw,pugi::encoding_utf8);
        props_[i].originalPropXml=std::make_shared<const std::string>(raw.str());
        props_[i].allowOpaqueMetadataCopy=false; // approval is per operation, not a permanent game property
        savedProp=savedProp.next_sibling("prop");
    }
    std::fill(propTransformDirty_.begin(), propTransformDirty_.end(), false);
    for(size_t i=0;i<ctfFlags_.size();++i) ctfFlags_[i].legacySourceIndex=static_cast<int>(i);
    for(size_t i=0;i<spawns_.size();++i) spawns_[i].legacySourceIndex=static_cast<int>(i);
    for(size_t i=0;i<controlPoints_.size();++i) controlPoints_[i].legacySourceIndex=static_cast<int>(i);
    for(size_t i=0;i<bonuses_.size();++i) bonuses_[i].legacySourceIndex=static_cast<int>(i);
    for(size_t i=0;i<specialBoxes_.size();++i) specialBoxes_[i].legacySourceIndex=static_cast<int>(i);
    for(size_t i=0;i<lights_.size();++i) lights_[i].legacySourceIndex=static_cast<int>(i);
    for(size_t i=0;i<collisionPlanes_.size();++i)collisionPlanes_[i].legacySourceIndex=static_cast<int>(i);
    for(size_t i=0;i<collisionBoxes_.size();++i)collisionBoxes_[i].legacySourceIndex=static_cast<int>(i);
    for(size_t i=0;i<collisionTriangles_.size();++i)collisionTriangles_[i].legacySourceIndex=static_cast<int>(i);
    for(auto& item:collisionPlanes_)item.transformDirty=false;
    for(auto& item:collisionBoxes_)item.transformDirty=false;
    for(auto& item:collisionTriangles_)item.transformDirty=false;
    flagsDirty_=spawnsDirty_=pointsDirty_=bonusesDirty_=zonesDirty_=collisionDirty_=lightsDirty_=false;
    dirty_ = false;
    Log::Info("Legacy save complete: " + Log::PathUtf8(path_));
    error.clear();
    return true;
}
