#pragma once
#include <DirectXMath.h>
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace NativeCollisionImport { struct Result; }

struct PropInstance {
    std::string library;
    std::string group;
    std::string name;
    std::string texture;
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 rotation{};
    // Original per-instance XML metadata. -1 means that the field is absent or
    // not a recognized boolean value. This is deliberately NOT derived from
    // the presence of collision-plane/triangle nodes: the legacy map stores
    // those separately and the game's interpretation needs independent proof.
    // Explicit values survive Ctrl+C/V; original source nodes are copied in
    // full (including unsupported extensions) on an ordinary map save.
    int nativeWithCollision{-1}; // <with_collision>0/1</with_collision>
    int nativeFree{-1};          // prop free="true"/"false"
    bool hasUncopyableMetadata{}; // opaque source fields require deliberate copy approval
    bool hasInvalidNativeMetadata{}; // malformed known game flags cannot be copied even with approval
    bool allowOpaqueMetadataCopy{}; // transient per-copy opt-in, never serialized as a game flag
    // Full-map native clipboard clones keep the complete original XML subtree
    // even when the editor cannot interpret one of its legacy fields. This is
    // an in-memory provenance marker only; it is never serialized as a game flag.
    bool losslessNativeClone{};
    bool collisionOwnershipUnresolved{}; // Runtime-only: source helpers could not be matched to XML.
    int nativeCloneBatchId{-1};
    std::shared_ptr<const std::string> originalPropXml; // complete detached source <prop> subtree
    int legacySourceIndex{-1}; // original <static-geometry>/<prop> index, -1 for newly created props
};

struct CollisionPlane {
    int legacySourceIndex{-1}; int authoredOwnerIndex{-1}; int nativeCloneBatchId{-1}; bool transformDirty{};
    DirectX::XMFLOAT3 position{}; DirectX::XMFLOAT3 rotation{}; float width{}; float length{};
    std::shared_ptr<const std::string> originalXml; // complete source collision node for lossless cloning
};
struct CollisionBox {
    int legacySourceIndex{-1}; int authoredOwnerIndex{-1}; int nativeCloneBatchId{-1}; bool transformDirty{};
    DirectX::XMFLOAT3 position{}; DirectX::XMFLOAT3 rotation{}; DirectX::XMFLOAT3 size{};
    std::shared_ptr<const std::string> originalXml;
};
struct CollisionTriangle {
    int legacySourceIndex{-1};
    int authoredOwnerIndex{-1};
    int nativeCloneBatchId{-1};
    bool transformDirty{};
    DirectX::XMFLOAT3 v0{}, v1{}, v2{};
    DirectX::XMFLOAT3 position{}, rotation{};
    std::shared_ptr<const std::string> originalXml;
};

struct SpecialBox {
    int legacySourceIndex{-1};
    DirectX::XMFLOAT3 min{};
    DirectX::XMFLOAT3 max{};
    std::string action;
    bool free{};
};

struct CtfFlagMarker {
    int legacySourceIndex{-1};
    std::string team;
    DirectX::XMFLOAT3 position{};
};

struct SpawnMarker {
    int legacySourceIndex{-1};
    DirectX::XMFLOAT3 position{};
    float rotationZ{};
    std::string type;
    std::string team;
};

struct BonusRegionMarker {
    int legacySourceIndex{-1};
    DirectX::XMFLOAT3 min{}, max{};
    std::string name;
    std::string bonusType;
    std::vector<std::string> modes;
    bool free{true};
    bool parachute{true};
};

struct ControlPointMarker {
    int legacySourceIndex{-1};
    DirectX::XMFLOAT3 position{};
    std::string name;
    float distance{};
    bool free{};
};

// Native <lights><light> records. Existing unknown attributes/child nodes are
// preserved from source XML when editing; new records use only observed fields.
struct LightMarker {
    int legacySourceIndex{-1};
    std::string type{"omni"};
    DirectX::XMFLOAT3 position{};
    float rotationZ{};
    unsigned int color{0xFFFA9Du}; // Native XML: packed decimal RGB
    float intensity{1.0f};
    float attenuationBegin{0.1f}, attenuationEnd{20.0f};
    // Detached native <light> subtree. Existing lights already preserve unknown
    // XML in-place through legacySourceIndex; this copy lets Ctrl+C/V of a
    // supported light retain unknown attributes/children as well.
    std::shared_ptr<const std::string> originalXml;
};

struct MapStats {
    size_t props{};
    size_t collisionPlanes{};
    size_t collisionBoxes{};
    size_t collisionTriangles{};
    size_t spawnPoints{};
    size_t bonusRegions{};
    size_t specialBoxes{};
    size_t lights{};
    size_t wayPoints{};
    size_t dominationPoints{};
    size_t ctfFlags{};
};

class MapDocument {
public:
    bool Load(const std::filesystem::path& file, std::string& error);
    void CreateBlank(const std::string& version = "1.0.Light", bool markDirty = true);
    bool SaveLegacy(std::string& error);
    bool SaveLegacyAs(const std::filesystem::path& file, std::string& error);
    void Clear();

    bool SetPropTransform(size_t index, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& rotation);
    size_t AddProp(PropInstance prop);
    void SetCollisionOwnershipUnresolved(size_t index,bool unresolved) {
        if(index<props_.size())props_[index].collisionOwnershipUnresolved=unresolved;
    }
    // Copies actual XML primitives. A subset requires verified owners; a full
    // static-map selection also carries opaque unowned collision records.
    bool CopyNativeCollisionForProps(const std::vector<size_t>& indices,
        std::vector<CollisionPlane>& planes,std::vector<CollisionBox>& boxes,
        std::vector<CollisionTriangle>& triangles) const;

    // Lossless full-static-map clone path used by Ctrl+C/V when every static
    // prop is selected. The collision bundle is copied from the source map as
    // opaque native data instead of being reconstructed from 3DS helpers.
    // Unknown XML attributes/children are retained and only transform/ID data
    // required for the new instance is rewritten. Collider owner indices in
    // the bundle are source PROP SLOTS (0..props.size-1) and are remapped here.
    bool AppendLosslessNativeStaticClone(std::vector<PropInstance> props,
        std::vector<CollisionPlane> planes, std::vector<CollisionBox> boxes,
        std::vector<CollisionTriangle> triangles, std::vector<int>& inserted,
        std::string& error);
    // Exactly one verified original native template is supported in this phase.
    // Returns false for unsupported models; the UI must disclose visual-only placement.
    bool AddVerifiedCollisionForProp(size_t index);
    bool HasVerifiedCollisionForProp(size_t index) const;
    bool HasNativeCollisionForProp(size_t index) const;
    // Map-verified helperless Fogtown floors. The UI validates each original
    // 3DS mesh against its certified source-map footprint before calling these.
    bool HasVerifiedGroundSurfaceForProp(size_t index) const;
    bool AddVerifiedGroundSurfaceForProp(size_t index);
    // Uses one spatial pass, not O(props * collision planes), on large maps.
    // permitted[] is true only after verifying the matching external source 3DS.
    bool RepairVerifiedGroundSurfaces(const std::array<bool,6>& permitted,
                                      size_t& added,size_t& unresolved);
    // Generic 3DS plane/triangle helper path. Never manufactures geometry from
    // the visible mesh; refuses existing legacy props without a safe binding.
    // A copied original source map may contain two EXACT identical visual props
    // AND two matching native colliders (Esplanade WTile 1). Permit the second
    // authored set only when a higher-level source-multiplicity proof passed,
    // and only against an already authored prop from THIS placement transaction.
    bool AddImportedCollisionForProp(size_t index,const NativeCollisionImport::Result& source,
                                    bool sourceVerifiedCoincident=false,size_t stagedStart=static_cast<size_t>(-1));
    // Re-bind ONLY complete exact sets following Save/Load. No XML writes.
    bool BindImportedCollisionForProp(size_t index,const NativeCollisionImport::Result& source);
    bool DeleteProp(size_t index);
    // Strict deletion: refuse rather than silently leave an unverified physical wall.
    // Legacy XML has no explicit prop->collider reference: only a unique exact
    // center can be matched. Non-matching cases require manual collision review.
    bool DeletePropWithCollision(size_t index, std::string& reason);
    // Validate a group against the ORIGINAL document and delete atomically.
    // Returns false without changing the map when ownership is ambiguous.
    bool DeletePropsWithCollision(const std::vector<size_t>& indices, std::string& reason);
    enum class ColliderKind { Plane, Box, Triangle };
    bool DeleteCollider(ColliderKind kind, size_t index);
    size_t ExactCollidersAt(const DirectX::XMFLOAT3& position) const;
    // Functional objects are native legacy elements, not static props.
    bool SetFlagPosition(size_t index, const DirectX::XMFLOAT3& position);
    bool AddFlag(const std::string& team, const DirectX::XMFLOAT3& position);
    bool DeleteFlag(size_t index);
    bool SetSpawn(size_t index, const SpawnMarker& value);
    size_t AddSpawn(SpawnMarker value);
    bool DeleteSpawn(size_t index);
    bool SetControlPoint(size_t index, const ControlPointMarker& value);
    size_t AddControlPoint(ControlPointMarker value);
    bool DeleteControlPoint(size_t index);
    bool SetBonusRegion(size_t index, const BonusRegionMarker& value);
    size_t AddBonusRegion(BonusRegionMarker value);
    bool DeleteBonusRegion(size_t index);
    bool SetSpecialBox(size_t index, const SpecialBox& value);
    size_t AddSpecialBox(SpecialBox value);
    bool DeleteSpecialBox(size_t index);
    bool SetLight(size_t index, const LightMarker& value);
    size_t AddLight(LightMarker value);
    bool DeleteLight(size_t index);

    const std::filesystem::path& Path() const { return path_; }
    const std::vector<PropInstance>& Props() const { return props_; }
    const std::vector<SpecialBox>& SpecialBoxes() const { return specialBoxes_; }
    const std::vector<LightMarker>& Lights() const { return lights_; }
    const std::vector<CtfFlagMarker>& CtfFlags() const { return ctfFlags_; }
    const std::vector<SpawnMarker>& Spawns() const { return spawns_; }
    const std::vector<BonusRegionMarker>& Bonuses() const { return bonuses_; }
    const std::vector<ControlPointMarker>& ControlPoints() const { return controlPoints_; }
    // Read-only collision data for the native preview. These are parsed from the
    // original XML, never regenerated by the editor's serializer.
    const std::vector<CollisionPlane>& CollisionPlanes() const { return collisionPlanes_; }
    const std::vector<CollisionBox>& CollisionBoxes() const { return collisionBoxes_; }
    const std::vector<CollisionTriangle>& CollisionTriangles() const { return collisionTriangles_; }
    const MapStats& Stats() const { return stats_; }
    const std::string& Version() const { return version_; }
    bool Dirty() const { return dirty_; }

private:
    bool SerializeLegacy(std::string& xml, std::string& error) const;
    void BindVerifiedCollisionOwners();
    void BindVerifiedGroundOwners();

    std::filesystem::path path_;
    std::string version_;
    // Immutable master XML is shared across Undo snapshots: a large 230 MB map
    // must not copy its raw source on every structural edit. Save installs a NEW
    // buffer, so older snapshots keep their own original source unchanged.
    std::shared_ptr<const std::string> sourceXml_{std::make_shared<const std::string>()};
    std::vector<PropInstance> props_;
    std::vector<bool> propTransformDirty_;
    std::vector<SpecialBox> specialBoxes_;
    std::vector<LightMarker> lights_;
    std::vector<CtfFlagMarker> ctfFlags_;
    std::vector<SpawnMarker> spawns_;
    std::vector<BonusRegionMarker> bonuses_;
    std::vector<ControlPointMarker> controlPoints_;
    std::vector<CollisionPlane> collisionPlanes_;
    std::vector<CollisionBox> collisionBoxes_;
    std::vector<CollisionTriangle> collisionTriangles_;
    MapStats stats_{};
    bool dirty_{};
    int nextNativeCloneBatchId_{1};
    bool flagsDirty_{}, spawnsDirty_{}, pointsDirty_{}, bonusesDirty_{}, zonesDirty_{}, collisionDirty_{}, lightsDirty_{};
};
