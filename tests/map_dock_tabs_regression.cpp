#include "MapTabsUi.h"
#include "ReleaseTestCheck.h"
#include <iostream>

int main() {
    ImGui::CreateContext();
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1000,600};io.DeltaTime=1.f/60;
    io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    MapTabsUi ui;std::vector<MapWorkspace::Tab> tabs{{1,{},"NewProject",false}};
    MapWorkspace::Id active=1;std::optional<MapWorkspace::Id> activate,close;bool opened=false;
    ImGuiID dock=0;
    const auto frame=[&]() {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1000,600});
        ImGui::Begin("Host",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
        if(!dock) {
            dock=ImGui::GetID("Dock");ImGui::DockBuilderAddNode(dock,ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock,{1000,600});ui.SetDockId(dock);
        }
        ImGui::DockSpace(dock);ImGui::End();
        if(ui.Begin(tabs,active,activate,close,opened))ImGui::TextUnformatted("Canvas");
        ImGui::End();ImGui::Render();
    };
    for(int i=0;i<4;++i)frame();
    auto* node=ImGui::DockBuilderGetNode(dock);
    PT_REQUIRE(node && node->TabBar && node->TabBar->Tabs.Size==2); // document and +
    PT_REQUIRE(!ImGui::FindWindowByName("NewProject###MapViewport"));
    PT_REQUIRE(!activate && !close && !opened);
    tabs[0].title="OpenedMap";
    for(int i=0;i<3;++i)frame();
    PT_REQUIRE(node->TabBar->Tabs.Size==2); // renamed placeholder, same single tab
    tabs[0].title="NewProject";frame();
    tabs.push_back({2,{},"Arena",true});active=2;
    for(int i=0;i<4;++i)frame();
    PT_REQUIRE(node->TabBar->Tabs.Size==3 && !activate && !close);
    const auto clickTab=[&](const char* name) {
        ImGuiTabItem* item=nullptr;
        for(auto& tab:node->TabBar->Tabs)if(std::string(ImGui::TabBarGetTabName(node->TabBar,&tab))==name)item=&tab;
        if(!item)return false;
        io.AddMousePosEvent(node->TabBar->BarRect.Min.x+item->Offset+item->Width*.4f,
            (node->TabBar->BarRect.Min.y+node->TabBar->BarRect.Max.y)*.5f);
        frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();return true;
    };
    PT_REQUIRE(clickTab("+"));PT_REQUIRE(opened && !close);
    opened=false;activate.reset();
    PT_REQUIRE(clickTab("NewProject###MapDocument_1"));
    PT_REQUIRE(activate && *activate==1 && !opened);
    active=*activate;activate.reset();frame();frame();
    PT_REQUIRE(!activate && !close);
    ImGui::DestroyContext();
    std::cout<<"PASS: single native docking row, plus action and map-tab activation\n";
}
