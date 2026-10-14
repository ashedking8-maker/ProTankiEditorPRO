#pragma once
#include "MapWorkspace.h"
#include <imgui.h>
#include <imgui_internal.h>

// One native docking tab per map. No second content-area tab bar.
class MapTabsUi {
public:
    void SetDockId(ImGuiID id) {dockId_=id;}
    static std::string WindowName(const MapWorkspace::Tab& tab) {
        return tab.title+"###MapDocument_"+std::to_string(tab.id);
    }
    // Always leaves the active map window begun, even when its contents are
    // hidden. The caller must End() it. Document activation is deferred by App.
    bool Begin(const std::vector<MapWorkspace::Tab>& tabs,MapWorkspace::Id active,
               std::optional<MapWorkspace::Id>& activate,
               std::optional<MapWorkspace::Id>& close,bool& openRequested) {
        constexpr auto flags=ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
        for(const auto& tab:tabs)if(tab.id!=active) {
            ImGui::SetNextWindowDockID(dockId_,ImGuiCond_Always);
            bool open=true;
            const bool visible=ImGui::Begin(WindowName(tab).c_str(),&open,flags|
                (tab.dirty?ImGuiWindowFlags_UnsavedDocument:0));
            if(!open)close=tab.id;
            if(visible && open && ImGui::GetCurrentWindow()->DockTabIsVisible)activate=tab.id;
            ImGui::End();
        }
        const auto it=std::find_if(tabs.begin(),tabs.end(),[&](const auto& tab){return tab.id==active;});
        const MapWorkspace::Tab fallback{active,{},"NewProject",false};
        const auto& tab=it==tabs.end()?fallback:*it;
        ImGui::SetNextWindowDockID(dockId_,ImGuiCond_Always);
        if(lastActive_!=active) {ImGui::SetNextWindowFocus();activate.reset();}
        bool open=true;
        const bool visible=ImGui::Begin(WindowName(tab).c_str(),&open,flags|
            (tab.dirty?ImGuiWindowFlags_UnsavedDocument:0));
        lastActive_=active;
        if(!open)close=active;
        auto* node=ImGui::GetCurrentWindow()->DockNode;
        if(node && ImGui::DockNodeBeginAmendTabBar(node)) {
            if(ImGui::TabItemButton("+"))openRequested=true;
            ImGui::DockNodeEndAmendTabBar();
        }
        return visible && open && !activate.has_value() && !close.has_value();
    }
private:
    ImGuiID dockId_{};
    MapWorkspace::Id lastActive_{};
};
