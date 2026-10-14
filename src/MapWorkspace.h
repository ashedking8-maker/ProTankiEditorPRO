#pragma once
#include "MapDocument.h"
#include "EditHistory.h"
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <optional>

// Only the active document is exposed to the renderer/UI. Inactive XML and undo
// histories stay in RAM; switching moves them, it never reloads or serializes.
class MapWorkspace {
public:
    using Id=std::uint64_t;
    struct Tab {Id id;std::filesystem::path path;std::string title;bool dirty;};
    MapWorkspace(){entries_.push_back({nextId_++});}
    Id ActiveId() const {return entries_[active_].id;}
    size_t Size() const {return entries_.size();}
    static bool IsUntouchedEmpty(const MapDocument& doc,const EditHistory& history) {
        const auto& s=doc.Stats();
        return !doc.Dirty() && !history.CanUndo() && !history.CanRedo() &&
            s.props==0 && s.collisionPlanes==0 && s.collisionBoxes==0 && s.collisionTriangles==0 &&
            s.spawnPoints==0 && s.bonusRegions==0 && s.specialBoxes==0 && s.lights==0 &&
            s.wayPoints==0 && s.dominationPoints==0 && s.ctfFlags==0;
    }
    void PruneInactiveEmpty(std::optional<Id> keep={}) {
        for(size_t i=entries_.size();i-->0;)if(i!=active_ && entries_[i].id!=keep && IsUntouchedEmpty(entries_[i].document,entries_[i].history)) {
            entries_.erase(entries_.begin()+static_cast<std::ptrdiff_t>(i));
            if(i<active_)--active_;
        }
    }
    Id Open(MapDocument document,MapDocument& current,EditHistory& history) {
        PruneInactiveEmpty();
        if(IsUntouchedEmpty(current,history)) {
            current=std::move(document);history.Clear();return ActiveId();
        }
        return Add(std::move(document),current,history);
    }
    static bool SamePath(const std::filesystem::path& a,const std::filesystem::path& b) {
        if(a.empty()||b.empty())return false;
        std::error_code ec;
        if(std::filesystem::equivalent(a,b,ec)&&!ec)return true;
        const auto normalize=[](const auto& path) {
            std::error_code error;auto p=std::filesystem::weakly_canonical(path,error);
            if(error)p=path.lexically_normal();
            auto result=p.generic_wstring();
#ifdef _WIN32
            std::transform(result.begin(),result.end(),result.begin(),[](wchar_t c){return std::towlower(c);});
#endif
            return result;
        };
        return normalize(a)==normalize(b);
    }
    std::vector<Tab> Tabs(const MapDocument& current) const {
        std::vector<Tab> result;
        for(size_t i=0;i<entries_.size();++i) {
            const auto& doc=i==active_?current:entries_[i].document;
            const auto utf8=doc.Path().stem().u8string();
            std::string name=utf8.empty()?"NewProject":std::string(utf8.begin(),utf8.end());
            result.push_back({entries_[i].id,doc.Path(),std::move(name),doc.Dirty()});
        }
        return result;
    }
    std::optional<Id> FindPath(const std::filesystem::path& path,const MapDocument& current) const {
        for(const auto& tab:Tabs(current))if(SamePath(path,tab.path))return tab.id;
        return {};
    }
    bool Activate(Id id,MapDocument& current,EditHistory& history) {
        const auto it=std::find_if(entries_.begin(),entries_.end(),[&](const auto& entry){return entry.id==id;});
        if(it==entries_.end())return false;
        const size_t next=static_cast<size_t>(it-entries_.begin());
        if(next==active_)return true;
        entries_[active_].document=std::move(current);
        entries_[active_].history=std::move(history);
        active_=next;
        current=std::move(entries_[active_].document);
        history=std::move(entries_[active_].history);
        return true;
    }
    Id Add(MapDocument document,MapDocument& current,EditHistory& history) {
        const auto id=nextId_++;entries_.push_back({id,std::move(document),{}});
        Activate(id,current,history);return id;
    }
    // Caller has already resolved Save / Discard / Cancel for the active tab.
    void CloseActive(MapDocument& current,EditHistory& history) {
        if(entries_.size()==1) {
            current.CreateBlank("1.0.Light",false);history.Clear();entries_[0]=Entry{nextId_++};return;
        }
        entries_.erase(entries_.begin()+static_cast<std::ptrdiff_t>(active_));
        active_=std::min(active_,entries_.size()-1);
        current=std::move(entries_[active_].document);history=std::move(entries_[active_].history);
    }
private:
    struct Entry {Id id;MapDocument document;EditHistory history;};
    std::vector<Entry> entries_;
    size_t active_{};
    Id nextId_{1};
};
