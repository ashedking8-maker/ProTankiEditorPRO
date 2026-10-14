#pragma once
#include "MapWorkspace.h"
#include <imgui.h>
#include <imgui_internal.h>

// One native docking tab per map. No second content-area tab bar.
class MapTabsUi {
public:
    void SetDockId(ImGuiID id) {dockId_=id;}
    void PrepareTabOrder() {
        auto* node=ImGui::DockBuilderGetNode(dockId_);
        if(!node || !node->TabBar)return;
        auto& tabs=node->TabBar->Tabs;
        for(int i=0;i+1<tabs.Size;++i)if(tabs[i].Flags&ImGuiTabItemFlags_Button) {
            // Run before DockSpace lays out/draws the native row. New document
            // tabs are appended by docking; keep + after them, not between them.
            std::rotate(tabs.Data+i,tabs.Data+i+1,tabs.Data+tabs.Size);break;
        }
    }
    static std::string WindowName(const MapWorkspace::Tab& tab) {
        return tab.title+"###MapDocument_"+std::to_string(tab.id);
    }
    // Always leaves the active map window begun, even when its contents are
    // hidden. The caller must End() it. Document activation is deferred by App.
    bool Begin(const std::vector<MapWorkspace::Tab>& tabs,MapWorkspace::Id active,
               std::optional<MapWorkspace::Id>& activate,
               std::optional<MapWorkspace::Id>& close,bool& openRequested) {
        constexpr auto flags=ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
        ImGuiWindowClass windowClass;
        windowClass.TabItemFlagsOverrideSet=ImGuiTabItemFlags_NoAssumedClosure;
        for(const auto& tab:tabs)if(tab.id!=active) {
            ImGui::SetNextWindowDockID(dockId_,ImGuiCond_Always);
            ImGui::SetNextWindowClass(&windowClass);
            bool open=true;
            const bool visible=ImGui::Begin(WindowName(tab).c_str(),&open,flags);
            if(!open)close=tab.id;
            if(visible && open && ImGui::GetCurrentWindow()->DockTabIsVisible)activate=tab.id;
            ImGui::End();
        }
        const auto it=std::find_if(tabs.begin(),tabs.end(),[&](const auto& tab){return tab.id==active;});
        const MapWorkspace::Tab fallback{active,{},"NewProject",false};
        const auto& tab=it==tabs.end()?fallback:*it;
        ImGui::SetNextWindowDockID(dockId_,ImGuiCond_Always);
        ImGui::SetNextWindowClass(&windowClass);
        if(lastActive_!=active) {ImGui::SetNextWindowFocus();activate.reset();}
        bool open=true;
        const bool visible=ImGui::Begin(WindowName(tab).c_str(),&open,flags);
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
