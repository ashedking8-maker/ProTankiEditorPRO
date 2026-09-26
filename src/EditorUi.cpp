#include "EditorUi.h"
#include "VerifiedCollisionTemplates.h"
#include "NativeCollisionImport.h"
#include "Theme.h"
#include "Logger.h"
#include "SplashScreen.h"
#include "LegacyMeshImport.h"
#include "LegacyTransform.h"
#include "DraftMeshImport.h"
#include "VolumePicking.h"
#include "GameplayAuthoring.h"
#include "GridStep.h"
#include <pugixml.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <vector>
#include <fstream>
#include <sstream>
#include <system_error>
#include <iterator>
#include <iomanip>
#include <shellapi.h>
#include <cwctype>
#include <cstdint>
#include <cstring>

namespace {
bool launchWindowsPath(const std::filesystem::path& path, bool openFile) {
    const std::wstring target = path.wstring();
    std::wstring command = openFile ? (L"notepad.exe \"" + target + L"\"") : (L"explorer.exe \"" + target + L"\"");
    std::vector<wchar_t> commandLine(command.begin(), command.end());
    commandLine.push_back(L'\0');
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL ok = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process);
    if (ok) { CloseHandle(process.hThread); CloseHandle(process.hProcess); return true; }
    return false;
}

std::filesystem::path openXml(HWND owner, const std::filesystem::path& initialDirectory = {}) {
    wchar_t path[32768]{};
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"ProTanki map (*.xml)\0*.xml\0All files\0*.*\0";
    ofn.lpstrFile = path;
    const std::wstring initial = initialDirectory.wstring();
    if (!initial.empty()) ofn.lpstrInitialDir = initial.c_str();
    ofn.nMaxFile = ARRAYSIZE(path);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    return GetOpenFileNameW(&ofn) ? std::filesystem::path(path) : std::filesystem::path{};
}

std::filesystem::path saveXml(HWND owner, const std::filesystem::path& current) {
    wchar_t path[32768]{};
    if (!current.empty()) wcsncpy_s(path, current.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"ProTanki map (*.xml)\0*.xml\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = ARRAYSIZE(path);
    ofn.lpstrDefExt = L"xml";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_OVERWRITEPROMPT;
    return GetSaveFileNameW(&ofn) ? std::filesystem::path(path) : std::filesystem::path{};
}

std::filesystem::path pickFolder(HWND owner, const std::filesystem::path& initialDirectory = {},
                                  const wchar_t* title = L"Select local asset library folder") {
    IFileDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
    DWORD options{};
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(title);
    if (!initialDirectory.empty()) {
        IShellItem* start = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(initialDirectory.c_str(), nullptr, IID_PPV_ARGS(&start)))) {
            dialog->SetFolder(start); start->Release();
        }
    }
    std::filesystem::path result;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) { result = path; CoTaskMemFree(path); }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

void ToolbarToggle(const char* label, bool& value) {
    if (ImGui::Button(label)) value = !value;
    if (value) {
        const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine({mn.x + 3, mx.y - 1}, {mx.x - 3, mx.y - 1}, IM_COL32(83,148,211,230), 1.0f);
    }
}

void HoverHelp(const char* explanation) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", explanation);
}

// This inspector displays the complete original XML subtree, including unknown
// attributes and nodes. It is deliberately read-only; no schema inference and
// no implicit change to the source library or native game export.
void DrawXmlPropertyNode(pugi::xml_node node, int depth, size_t& shown) {
    if(!node || depth>24 || shown>=512)return;
    ++shown;
    ImGui::PushID(node.internal_object());
    const auto first=node.first_child();
    const bool expandable=static_cast<bool>(first) || static_cast<bool>(node.first_attribute());
    if(expandable) {
        const bool expanded=ImGui::TreeNodeEx(node.name(), ImGuiTreeNodeFlags_SpanAvailWidth);
        if(expanded) {
            for(const auto attribute:node.attributes()) {
                ImGui::TextWrapped("@%s = %s", attribute.name(),attribute.value());
                HoverHelp("Original XML attribute; kept unchanged unless explicitly edited by a supported operation.");
            }
            for(auto child=node.first_child();child;child=child.next_sibling()) {
                if(child.type()==pugi::node_element)DrawXmlPropertyNode(child,depth+1,shown);
                else if(child.type()==pugi::node_pcdata || child.type()==pugi::node_cdata) {
                    const std::string value=child.value();
                    if(value.find_first_not_of(" \t\r\n")!=std::string::npos)
                        ImGui::TextWrapped("%s",value.c_str());
                }
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::TextWrapped("%s",node.name());
    }
    ImGui::PopID();
}
// A stable preorder path is saved as a draft-only inclusion plan. This does
// not modify the full original source XML or pretend to be a game exporter.
void DrawSelectableXmlNode(pugi::xml_node node, const std::string& path,
                           std::vector<std::string>& excluded,int depth) {
    if(!node || depth>20 || path.size()>220)return;
    ImGui::PushID(path.c_str());
    const bool isRoot=depth==0;
    const bool protectedNode=isRoot || (std::string(node.name())=="mesh" && depth==1) ||
        (std::string(node.name())=="sprite" && depth==1);
    bool enabled=std::find(excluded.begin(),excluded.end(),path)==excluded.end();
    if(protectedNode)ImGui::BeginDisabled();
    if(ImGui::Checkbox((std::string("##include_")+node.name()).c_str(),&enabled)) {
        if(enabled)excluded.erase(std::remove(excluded.begin(),excluded.end(),path),excluded.end());
        else if(excluded.size()<256)excluded.push_back(path);
    }
    if(protectedNode)ImGui::EndDisabled();
    ImGui::SameLine();
    const bool open=ImGui::TreeNodeEx(node.name(),ImGuiTreeNodeFlags_SpanAvailWidth);
    if(open) {
        for(auto attr:node.attributes()) {
            const std::string attrPath=path+"/@"+attr.name();
            const bool protectedAttr=isRoot && std::string(attr.name())=="name" ||
                (std::string(attr.name())=="file" &&
                 (std::string(node.name())=="mesh"||std::string(node.name())=="sprite"));
            bool keep=std::find(excluded.begin(),excluded.end(),attrPath)==excluded.end();
            ImGui::PushID(attrPath.c_str());
            if(protectedAttr)ImGui::BeginDisabled();
            if(ImGui::Checkbox((std::string("@")+attr.name()).c_str(),&keep)) {
                if(keep)excluded.erase(std::remove(excluded.begin(),excluded.end(),attrPath),excluded.end());
                else if(excluded.size()<256)excluded.push_back(attrPath);
            }
            if(protectedAttr)ImGui::EndDisabled();
            ImGui::SameLine();ImGui::TextWrapped("= %s",attr.value());
            ImGui::PopID();
        }
        int elementIndex=0;
        for(auto child=node.first_child();child;child=child.next_sibling()) {
            if(child.type()==pugi::node_element) {
                DrawSelectableXmlNode(child,path+"/"+std::to_string(elementIndex++)+":"+child.name(),excluded,depth+1);
            } else if(child.type()==pugi::node_pcdata || child.type()==pugi::node_cdata) {
                const std::string value=child.value();
                if(value.find_first_not_of(" \t\r\n")!=std::string::npos)
                    ImGui::TextWrapped("Value: %s",value.c_str());
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}
void DrawRawPropertyTree(const char* label, const std::string& source) {
    if(!ImGui::TreeNode(label))return;
    pugi::xml_document doc;
    if(source.size()>8u*1024u*1024u || !doc.load_buffer(source.data(),source.size(),pugi::parse_default,pugi::encoding_auto)) {
        ImGui::TextDisabled("Original XML cannot be displayed.");
    } else {
        size_t shown=0;
        for(auto node=doc.first_child();node;node=node.next_sibling())
            if(node.type()==pugi::node_element)DrawXmlPropertyNode(node,0,shown);
        if(shown>=512)ImGui::TextDisabled("Display limited to 512 nodes; original source data is retained in full.");
    }
    ImGui::TreePop();
}

bool ToolButton(const char* label, bool active) {
    const bool pressed = ImGui::Button(label);
    if (active) {
        const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine({mn.x + 3, mx.y - 1}, {mx.x - 3, mx.y - 1}, IM_COL32(83,148,211,235), 1.0f);
    }
    return pressed;
}

bool Same3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
    constexpr float e = 0.00001f;
    return std::fabs(a.x-b.x)<e && std::fabs(a.y-b.y)<e && std::fabs(a.z-b.z)<e;
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return value;
}

bool ContainsInsensitive(const std::string& haystack, const char* needle) {
    if (!needle || !needle[0]) return true;
    return Lower(haystack).find(Lower(needle)) != std::string::npos;
}
// Fast metadata-only category classification. This is heuristic: the original
// library has no universal semantic category attribute; the user can always
// find any asset through All + name search. No 3DS files are opened here.
int BrowseKind(const AssetDefinition& asset) {
    if (!asset.sprite.empty()) return 7;
    const std::string key=Lower(asset.library+" "+asset.group+" "+asset.name);
    auto has=[&](const char* v){return key.find(v)!=std::string::npos;};
    if(has("bridge")||has("ramp")||has("rise")||has("slope")||has("transition")||has("tunnel")) return 4;
    if(has("wall")||has("fence")||has("barrier")||has("cliff")||has("block")) return 2;
    if(has("grass")||has("tile")||has("ground")||has("sand")||has("pave")||has("floor")||has("concrete")) return 1;
    if(has("house")||has("build")||has("garage")||has("fort")||has("tower")||has("city")) return 3;
    if(has("tree")||has("bush")||has("plant")||has("vegetation")||has("rock")) return 5;
    return 6;
}
std::filesystem::path ControlsPath() {
    wchar_t local[32768]{};
    const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", local, ARRAYSIZE(local));
    if (!count || count >= ARRAYSIZE(local)) return {};
    return std::filesystem::path(local) / L"GTanksNextEditor" / L"controls.ini";
}

const char* GestureName(int i) {
    static constexpr const char* names[] = {"Right mouse", "Middle mouse", "Alt + left mouse", "Space + left mouse", "Shift + right mouse"};
    return names[std::clamp(i,0,4)];
}

}

const char* EditorUi::EffectName(int mode) {
    static constexpr const char* names[] = {"Off", "Bright", "Crisp", "Warm", "Cool", "High contrast", "Unlit texture (diagnostic)"};
    return names[std::clamp(mode,0,6)];
}

void EditorUi::OnMapLoaded(const std::filesystem::path& successfulMapPath) {
    collisionBindingsPending_=true;
    history_.Clear(); drag_ = {}; propertyEditActive_ = false; propertyEditIndex_ = -1; selected_ = -1; selectedItems_.clear();
    clipboard_.clear(); placementItems_.clear(); ghostProps_.clear(); ghostValid_=false; selectionBoxActive_=false; placementActive_ = false;
    gameplayMode_ = -1; showGameplay_ = false; showSpawns_=showFlags_=showPoints_=showBonuses_=false; showZones_ = false; browseLibraryOpen_=false;
    functionalSelected_=FunctionalType::None; functionalPlacement_=FunctionalPlacement::None;
    // A new map must not inherit an unverified native bonus identifier from
    // an unrelated library/map, or an old mode-selection warning.
    newBonusKind_=0; newBonusTypeOverride_.clear(); newBonusModes_=1;
    newBonusFree_=true; newBonusParachute_=true; bonusModeExplanation_=false;
    newSpawnYaw_=0.0f;
    functionalDragActive_=false; functionalGhostValid_=false;
    functionalDragBefore_.reset();functionalPropertyBefore_.reset();
    zoneDragBefore_.reset();zonePropertyBefore_.reset();
    if (!successfulMapPath.empty()) {
        lastMapDirectory_=successfulMapPath.parent_path();
        guidance_.RecordSuccess(GuideTarget::Map);
        SaveControls();
    }
}

bool EditorUi::LaunchExternalTester() {
    // External program is only launched. No map paths, arguments, registry or
    // ProTLVK configuration are read or written by this button.
    const auto savedPath=ControlsPath().parent_path()/L"external-tester-path.txt";
    if(testerExecutable_.empty()) {
        std::wifstream in(savedPath); std::wstring saved;
        if(in && std::getline(in,saved)) testerExecutable_=saved;
    }
    auto valid=[](const std::filesystem::path& path) {
        std::error_code ec;
        std::wstring name=path.filename().wstring();
        std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
        return name==L"protlvk32.exe" && std::filesystem::is_regular_file(path,ec) && !ec;
    };
    if(!valid(testerExecutable_)) {
        wchar_t buffer[32768]{};
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner=GetActiveWindow();
        ofn.lpstrFilter=L"ProTLVK32 tester (ProTLVK32.exe)\0ProTLVK32.exe\0Windows applications (*.exe)\0*.exe\0";
        ofn.lpstrFile=buffer;ofn.nMaxFile=ARRAYSIZE(buffer);
        ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
        ofn.lpstrTitle=L"Locate ProTLVK32.exe (first use only)";
        if(!GetOpenFileNameW(&ofn))return false;
        const std::filesystem::path selected=buffer;
        if(!valid(selected)) { SetMessage("Choose the original ProTLVK32.exe executable.",true);return false; }
        testerExecutable_=selected;
        std::error_code ec;std::filesystem::create_directories(savedPath.parent_path(),ec);
        if(!ec) {std::wofstream out(savedPath,std::ios::trunc);if(out)out<<testerExecutable_.wstring()<<L'\n';}
        guidance_.RecordSuccess(GuideTarget::Tester);
        SaveControls();
    }
    const auto executable=testerExecutable_.wstring();
    std::wstring commandLine=L"\""+executable+L"\"";
    std::vector<wchar_t> writable(commandLine.begin(),commandLine.end());writable.push_back(L'\0');
    const auto folder=testerExecutable_.parent_path().wstring();
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),writable.data(),nullptr,nullptr,FALSE,0,nullptr,
                       folder.c_str(),&startup,&process)) {
        const auto code=GetLastError();
        SetMessage("Could not start ProTLVK32.exe (Windows error "+std::to_string(code)+"). Check the saved path.",true);
        // Prompt for a new location next time if the saved target disappears.
        if(!valid(testerExecutable_)) testerExecutable_.clear();
        return false;
    }
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    Log::Info("External ProTLVK32 tester started (no command line arguments or map modifications).");
    SetMessage("ProTLVK32.exe started. Editor map data was not touched.");
    return true;
}

void EditorUi::OnLibraryLoaded(SceneRenderer& previewScene, const std::filesystem::path& successfulLibraryRoot) {
    collisionBindingsPending_=true;
    if (!successfulLibraryRoot.empty()) {
        lastLibraryDirectory_=successfulLibraryRoot;
        guidance_.RecordSuccess(GuideTarget::Library);
        SaveControls();
    }
    selectedAsset_ = -1; selectedTextureVariant_ = 0; assetPreviewReady_ = false; placementActive_ = false;
    recentAssets_.clear(); axCurrent_=0; axPinned_=false; axConfirmRemove_=false; ghostProps_.clear();
    browseLibraryOpen_=false; browseThumbnails_.clear(); browseFrame_=0;
    previewScene.ClearScene();
}

PropTransformState EditorUi::StateOf(const PropInstance& p) { return {p.position, p.rotation}; }

float EditorUi::SnapPosition(float v) const {return GridStep::Quantize(v,gridSize_);}
float EditorUi::SnapDelta(float v) const {return GridStep::Quantize(v,gridSize_);}

float EditorUi::SnapRotation(float radians) const {
    if (rotationSnapDeg_ <= 0.001f) return radians;
    constexpr float pi = 3.14159265358979323846f;
    const float step = rotationSnapDeg_ * pi / 180.0f;
    return std::round(radians / step) * step;
}

EditorUi::EditorUi() {
    ResetCustomBindings();
    LoadControls();
    showControlHelp_ = false; // The full manual remains available from the menu.
}

const char* EditorUi::ActionName(Action action) {
    static constexpr const char* names[] = {
        "Open map", "Save map", "Save as", "Undo", "Redo", "Select tool", "Move tool", "Rotate tool",
        "Delete selected", "Legacy snap (retired)", "Toggle grid", "Toggle bounds", "Toggle gameplay", "Toggle zones",
        "Frame selection/map", "Place selected library asset", "Cancel / clear selection",
        "Move +X (right)", "Move -X (left)", "Move +Y (forward)", "Move -Y (backward)",
        "Move +Z (up)", "Move -Z (down)", "Rotate +Z", "Rotate -Z",
        "Grid size 100", "Grid size 200", "Grid size 300", "Grid size 400", "Grid size 500", "Fullscreen"
    };
    const size_t index = static_cast<size_t>(action);
    return index < std::size(names) ? names[index] : "Unknown";
}

EditorUi::KeyBinding EditorUi::DefaultBinding(Action a) {
    using A = Action;
    switch (a) {
    case A::Open: return {ImGuiKey_O,true};
    case A::Save: return {ImGuiKey_S,true};
    case A::SaveAs: return {ImGuiKey_S,true,true};
    case A::Undo: return {ImGuiKey_Z,true};
    case A::Redo: return {ImGuiKey_Y,true};
    case A::Select: return {ImGuiKey_1};
    case A::Move: return {ImGuiKey_2};
    case A::Rotate: return {ImGuiKey_3};
    case A::Delete: return {ImGuiKey_Delete};
    case A::Snap: return {};
    case A::Grid: return {ImGuiKey_H};
    case A::Bounds: return {ImGuiKey_B};
    case A::Gameplay: return {ImGuiKey_P};
    case A::Zones: return {ImGuiKey_J};
    case A::Frame: return {ImGuiKey_F};
    case A::Place: return {ImGuiKey_Enter};
    case A::Cancel: return {ImGuiKey_Escape};
    case A::MoveXP: return {ImGuiKey_D};
    case A::MoveXN: return {ImGuiKey_A};
    case A::MoveYP: return {ImGuiKey_W};
    case A::MoveYN: return {ImGuiKey_S};
    case A::MoveZP: return {ImGuiKey_E};
    case A::MoveZN: return {ImGuiKey_Q};
    case A::RotateP: return {ImGuiKey_Z};
    case A::RotateN: return {ImGuiKey_X};
    case A::Grid100: return {ImGuiKey_Keypad1};
    case A::Grid200: return {ImGuiKey_Keypad2};
    case A::Grid300: return {ImGuiKey_Keypad3};
    case A::Grid400: return {ImGuiKey_Keypad4};
    case A::Grid500: return {ImGuiKey_Keypad5};
    case A::Fullscreen: return {ImGuiKey_F11};
    default: return {};
    }
}

void EditorUi::ResetCustomBindings() {
    for (size_t i=0;i<customKeys_.size();++i) customKeys_[i]=DefaultBinding(static_cast<Action>(i));
    navCustom_=navSimple_;
    customOrbit_=MouseGesture::Right; customPan_=MouseGesture::Middle;
}

EditorUi::KeyBinding EditorUi::Binding(Action a) const {
    return navigationMode_==NavigationMode::Custom ? customKeys_[static_cast<size_t>(a)] : DefaultBinding(a);
}

bool EditorUi::Pressed(Action a, bool allowFineShift) const {
    const KeyBinding k=Binding(a);
    if (!k.key) return false;
    const ImGuiIO& io=ImGui::GetIO();
    if (io.KeyCtrl != k.ctrl || io.KeyAlt != k.alt) return false;
    if (io.KeyShift != k.shift && !(allowFineShift && io.KeyShift && !k.shift)) return false;
    return ImGui::IsKeyPressed(static_cast<ImGuiKey>(k.key),false);
}

std::string EditorUi::Shortcut(Action a) const {
    const auto k=Binding(a); if (!k.key) return {};
    std::string result;
    if (k.ctrl) result+="Ctrl+";
    if (k.shift) result+="Shift+";
    if (k.alt) result+="Alt+";
    const char* name=ImGui::GetKeyName(static_cast<ImGuiKey>(k.key));
    result+=name && *name ? name : "?";
    return result;
}

std::string EditorUi::ToolbarLabel(const char* name, Action a) const {
    std::string result=name;
    const std::string shortcut=Shortcut(a);
    if (showShortcuts_ && !shortcut.empty()) result+=" ["+shortcut+"]";
    return result;
}

void EditorUi::LoadControls() {
    const auto path=ControlsPath(); if (path.empty()) return;
    std::ifstream input(path); if (!input) return;
    std::string line;
    while (std::getline(input,line)) {
        std::istringstream in(line); std::string tag; in>>tag;
        if (tag=="showShortcuts") { int v{}; if(in>>v) showShortcuts_=v!=0; }
        else if (tag=="welcomeSeen") { int v{}; if(in>>v) welcomePending_=(v==0); }
        else if (tag=="guideSeen") { int which{},value{}; if(in>>which>>value && which>=1 && which<=3) guidance_.Restore(static_cast<GuideTarget>(which-1),value!=0); }
        else if (tag=="lastMapDirectory") { std::string value; if(in>>std::quoted(value)) lastMapDirectory_=std::filesystem::u8path(value); }
        else if (tag=="lastLibraryDirectory") { std::string value; if(in>>std::quoted(value)) lastLibraryDirectory_=std::filesystem::u8path(value); }
        else if (tag=="objectDraftOutputRoot") { std::string value; if(in>>std::quoted(value)) objectDraftOutputRoot_=std::filesystem::u8path(value); }
        else if (tag=="uiTheme") { int value{}; if(in>>value && value>=0 && value<=3) uiTheme_=value; }
        else if (tag=="customSurface") { for(auto& v:customSurface_) if(!(in>>v)) break; for(auto& v:customSurface_) v=std::clamp(v,0.f,1.f); }
        else if (tag=="customText") { for(auto& v:customText_) if(!(in>>v)) break; for(auto& v:customText_) v=std::clamp(v,0.f,1.f); }
        else if (tag=="viewportBackground") { for(auto& v:viewportBackground_) if(!(in>>v)) break; for(auto& v:viewportBackground_) v=std::clamp(v,0.f,1.f); }
        else if (tag=="viewportGridColor") { for(auto& v:viewportGridColor_) if(!(in>>v)) break; for(auto& v:viewportGridColor_) v=std::clamp(v,0.f,1.f); }
        else if (tag=="smoothCameraFocus") { int v{};if(in>>v)smoothCameraFocus_=v!=0; }
        else if (tag=="previewNativeLighting") { int v{}; if(in>>v) previewNativeLighting_=v!=0; }
        else if (tag=="previewLightReach") { if(in>>previewLightReach_)previewLightReach_=std::clamp(previewLightReach_,1.f,500.f); }
        else if (tag=="browseThumbScale") { float value{}; if(in>>value && std::isfinite(value) && value>=0.7f && value<=1.65f) browseThumbnailScale_=value; }
        else if (tag=="navigationMode") { int v{}; if(in>>v && v>=0 && v<=3) navigationMode_=static_cast<NavigationMode>(v); }
        else if (tag=="customOrbit") { int v{}; if(in>>v && v>=0 && v<=4) customOrbit_=static_cast<MouseGesture>(v); }
        else if (tag=="customPan") { int v{}; if(in>>v && v>=0 && v<=4) customPan_=static_cast<MouseGesture>(v); }
        else if (tag=="nav") {
            int which{},invert{},focus{}; NavigationSettings n;
            if (in>>which>>n.orbitSensitivity>>n.panSensitivity>>n.zoomSpeed>>invert>>focus && which>=0 && which<=3) {
                n.orbitSensitivity=std::clamp(n.orbitSensitivity,0.25f,2.5f);
                n.panSensitivity=std::clamp(n.panSensitivity,0.25f,2.5f);
                n.zoomSpeed=std::clamp(n.zoomSpeed,0.25f,2.5f);
                n.invertOrbitY=invert!=0; n.focusSelectionOnClick=focus!=0;
                if (which==0) navLegacy_=n; else if(which==1) navAdobe_=n; else if(which==2) navSimple_=n; else navCustom_=n;
            }
        }
        else if (tag=="customSettings") {
            NavigationSettings n; int invert{}, focus{};
            if (in>>n.orbitSensitivity>>n.panSensitivity>>n.zoomSpeed>>invert>>focus) {
                n.orbitSensitivity=std::clamp(n.orbitSensitivity,0.25f,2.5f);
                n.panSensitivity=std::clamp(n.panSensitivity,0.25f,2.5f);
                n.zoomSpeed=std::clamp(n.zoomSpeed,0.25f,2.5f);
                n.invertOrbitY=invert!=0; n.focusSelectionOnClick=focus!=0; navCustom_=n;
            }
        } else if (tag=="key") {
            int index{}, key{}, ctrl{}, shift{}, alt{};
            if (in>>index>>key>>ctrl>>shift>>alt && index>=0 && index<static_cast<int>(Action::Count)
                && key>=0 && key<static_cast<int>(ImGuiKey_NamedKey_END)) {
                customKeys_[static_cast<size_t>(index)]={key,ctrl!=0,shift!=0,alt!=0};
            }
        }
    }
    // Migrate the old default G=grid binding; G is now reserved for geometry.
    auto& grid = customKeys_[static_cast<size_t>(Action::Grid)];
    if(grid.key==ImGuiKey_G && !grid.ctrl && !grid.shift && !grid.alt) grid.key=ImGuiKey_H;
}

void EditorUi::SaveControls() const {
    const auto path=ControlsPath(); if (path.empty()) return;
    std::error_code ec; std::filesystem::create_directories(path.parent_path(),ec);
    if (ec) { Log::Warning("Could not create controls directory: "+ec.message()); return; }
    std::ofstream out(path,std::ios::trunc); if (!out) return;
    out<<"showShortcuts "<<showShortcuts_<<'\n';
    out<<"welcomeSeen "<<(!welcomePending_)<<'\n';
    for(int i=0;i<3;++i) out<<"guideSeen "<<(i+1)<<' '<<guidance_.Seen(static_cast<GuideTarget>(i))<<'\n';
    out<<"lastMapDirectory "<<std::quoted(Log::PathUtf8(lastMapDirectory_))<<'\n';
    out<<"lastLibraryDirectory "<<std::quoted(Log::PathUtf8(lastLibraryDirectory_))<<'\n';
    out<<"objectDraftOutputRoot "<<std::quoted(Log::PathUtf8(objectDraftOutputRoot_))<<'\n';
    out<<"uiTheme "<<uiTheme_<<'\n';
    out<<"customSurface "<<customSurface_[0]<<' '<<customSurface_[1]<<' '<<customSurface_[2]<<'\n';
    out<<"customText "<<customText_[0]<<' '<<customText_[1]<<' '<<customText_[2]<<'\n';
    out<<"viewportBackground "<<viewportBackground_[0]<<' '<<viewportBackground_[1]<<' '<<viewportBackground_[2]<<'\n';
    out<<"viewportGridColor "<<viewportGridColor_[0]<<' '<<viewportGridColor_[1]<<' '<<viewportGridColor_[2]<<'\n';
    out<<"smoothCameraFocus "<<smoothCameraFocus_<<'\n';
    out<<"previewNativeLighting "<<previewNativeLighting_<<'\n';
    out<<"previewLightReach "<<previewLightReach_<<'\n';
    out<<"browseThumbScale "<<browseThumbnailScale_<<'\n';
    out<<"navigationMode "<<static_cast<int>(navigationMode_)<<'\n';
    out<<"customOrbit "<<static_cast<int>(customOrbit_)<<'\n';
    out<<"customPan "<<static_cast<int>(customPan_)<<'\n';
    const NavigationSettings settings[]={navLegacy_,navAdobe_,navSimple_,navCustom_};
    for (int i=0;i<4;++i) {
        const auto& n=settings[i];
        out<<"nav "<<i<<' '<<n.orbitSensitivity<<' '<<n.panSensitivity<<' '<<n.zoomSpeed<<' '
           <<n.invertOrbitY<<' '<<n.focusSelectionOnClick<<'\n';
    }
    for(size_t i=0;i<customKeys_.size();++i) {
        const auto k=customKeys_[i];
        out<<"key "<<i<<' '<<k.key<<' '<<k.ctrl<<' '<<k.shift<<' '<<k.alt<<'\n';
    }
}

void EditorUi::ApplyPreferredTheme() const { ApplyGTanksTheme(uiTheme_,customSurface_,customText_); }

EditorUi::NavigationSettings& EditorUi::CurrentNavigationSettings() {
    if (navigationMode_==NavigationMode::Legacy) return navLegacy_;
    if (navigationMode_==NavigationMode::Adobe) return navAdobe_;
    if (navigationMode_==NavigationMode::Custom) return navCustom_;
    return navSimple_;
}

const char* EditorUi::NavigationModeName() const {
    if (navigationMode_==NavigationMode::Legacy) return "Legacy";
    if (navigationMode_==NavigationMode::Adobe) return "Adobe-style";
    if (navigationMode_==NavigationMode::Custom) return "Custom";
    return "Simple focus";
}

void EditorUi::SetNavigationMode(NavigationMode mode) {
    if (navigationMode_==mode) return;
    navigationMode_=mode; showControlHelp_=true;
    SaveControls();
    SetMessage(std::string("Navigation mode: ")+NavigationModeName());
}

bool EditorUi::GestureActive(MouseGesture gesture, bool dragging) const {
    const ImGuiIO& io=ImGui::GetIO();
    const ImGuiMouseButton button = (gesture==MouseGesture::Right || gesture==MouseGesture::ShiftRight)
                                    ? ImGuiMouseButton_Right : gesture==MouseGesture::Middle ? ImGuiMouseButton_Middle : ImGuiMouseButton_Left;
    const bool mouse = dragging ? ImGui::IsMouseDragging(button,0.0f) : ImGui::IsMouseDown(button);
    if (!mouse) return false;
    if (gesture==MouseGesture::AltLeft) return io.KeyAlt;
    if (gesture==MouseGesture::SpaceLeft) return ImGui::IsKeyDown(ImGuiKey_Space);
    if (gesture==MouseGesture::ShiftRight) return io.KeyShift;
    return !io.KeyAlt && !io.KeyShift && !ImGui::IsKeyDown(ImGuiKey_Space);
}

void EditorUi::ApplyLiveTransform(MapDocument& map, SceneRenderer& scene, int propIndex, const PropTransformState& state) {
    if (propIndex < 0) return;
    if (!map.SetPropTransform(static_cast<size_t>(propIndex), state.position, state.rotation)) return;
    if (static_cast<size_t>(propIndex) < map.Props().size()) scene.UpdatePropTransform(propIndex, map.Props()[static_cast<size_t>(propIndex)]);
}

void EditorUi::PushDiscreteTransform(MapDocument& map, SceneRenderer& scene, int propIndex, const PropTransformState& after, const char* label) {
    if (propIndex < 0 || static_cast<size_t>(propIndex) >= map.Props().size()) return;
    const auto before = StateOf(map.Props()[static_cast<size_t>(propIndex)]);
    if (Same3(before.position, after.position) && Same3(before.rotation, after.rotation)) return;
    ApplyLiveTransform(map, scene, propIndex, after);
    history_.Push({static_cast<size_t>(propIndex), before, after, label});
}

void EditorUi::SelectOnly(int index, SceneRenderer& scene) {
    selected_=index; selectedItems_.clear(); if(index>=0) functionalSelected_=FunctionalType::None;
    if (index>=0) {selectedItems_.push_back(index);functionalPropertyBefore_.reset();zonePropertyBefore_.reset();}
    scene.SetSelection(selectedItems_);
}

void EditorUi::SelectAllStaticProps(const MapDocument& map, SceneRenderer& scene) {
    selectedItems_.clear();
    for(size_t i=0;i<map.Props().size();++i)
        selectedItems_.push_back(static_cast<int>(i));
    selected_=selectedItems_.empty()?-1:selectedItems_.back();
    functionalSelected_=FunctionalType::None;
    functionalPropertyBefore_.reset();zonePropertyBefore_.reset();
    scene.SetSelection(selectedItems_);
    SetMessage("Selected all "+std::to_string(selectedItems_.size())+
               " static props. Delete removes their static geometry; Undo restores it.");
}

void EditorUi::BeginFunctionalPlacement(FunctionalPlacement type, SceneRenderer& scene) {
    lightPlacementActive_=false;
    functionalPasteActive_=false;
    placementActive_=false; placementItems_.clear(); ghostProps_.clear(); scene.ClearGhost();
    functionalPlacement_=type; functionalGhostValid_=false; functionalCommitRequested_=false;
    showGameplay_=true; if(gameplayMode_<0)gameplayMode_=0;
    if(type==FunctionalPlacement::KillZone || type==FunctionalPlacement::KickZone) showZones_=true;
    else if(type==FunctionalPlacement::BonusRegion) showBonuses_=true;
    else if(type==FunctionalPlacement::RedFlag || type==FunctionalPlacement::BlueFlag) showFlags_=true;
    else if(type==FunctionalPlacement::ControlPoint) showPoints_=true;
    else showSpawns_=true;
    // Do not emit a second temporary notification: the persistent palette explains placement.
    Log::Debug("Gameplay placement started; Space commits, RMB cancels.");
}

bool EditorUi::FunctionalPosition(const MapDocument& map, DirectX::XMFLOAT3& out) const {
    const size_t i=functionalIndex_;
    switch(functionalSelected_) {
    case FunctionalType::Flag: if(i<map.CtfFlags().size()) {out=map.CtfFlags()[i].position;return true;} break;
    case FunctionalType::Spawn: if(i<map.Spawns().size()) {out=map.Spawns()[i].position;return true;} break;
    case FunctionalType::Point: if(i<map.ControlPoints().size()) {out=map.ControlPoints()[i].position;return true;} break;
    case FunctionalType::Bonus: if(i<map.Bonuses().size()) {const auto& p=map.Bonuses()[i];out={(p.min.x+p.max.x)/2,(p.min.y+p.max.y)/2,(p.min.z+p.max.z)/2};return true;} break;
    case FunctionalType::Light: if(i<map.Lights().size()) {out=map.Lights()[i].position;return true;} break;
    case FunctionalType::Zone: if(i<map.SpecialBoxes().size()) {const auto& p=map.SpecialBoxes()[i];out={(p.min.x+p.max.x)/2,(p.min.y+p.max.y)/2,(p.min.z+p.max.z)/2};return true;} break;
    default:break;
    }
    return false;
}

void EditorUi::MoveFunctional(MapDocument& map, const DirectX::XMFLOAT3& at) {
    const size_t i=functionalIndex_;
    switch(functionalSelected_) {
    case FunctionalType::Flag: map.SetFlagPosition(i,at); break;
    case FunctionalType::Spawn: if(i<map.Spawns().size()) {auto p=map.Spawns()[i];p.position=at;map.SetSpawn(i,p);} break;
    case FunctionalType::Point: if(i<map.ControlPoints().size()) {auto p=map.ControlPoints()[i];p.position=at;map.SetControlPoint(i,p);} break;
    case FunctionalType::Light: if(i<map.Lights().size()) {auto light=map.Lights()[i];light.position=at;map.SetLight(i,light);} break;
    case FunctionalType::Bonus: if(i<map.Bonuses().size()) {auto p=map.Bonuses()[i];
        const DirectX::XMFLOAT3 center{(p.min.x+p.max.x)/2,(p.min.y+p.max.y)/2,(p.min.z+p.max.z)/2};
        const float x=at.x-center.x,y=at.y-center.y,z=at.z-center.z;
        p.min.x+=x;p.min.y+=y;p.min.z+=z;p.max.x+=x;p.max.y+=y;p.max.z+=z;map.SetBonusRegion(i,p);
    }break;
    case FunctionalType::Zone: if(i<map.SpecialBoxes().size()) {auto p=map.SpecialBoxes()[i];
        const DirectX::XMFLOAT3 center{(p.min.x+p.max.x)/2,(p.min.y+p.max.y)/2,(p.min.z+p.max.z)/2};
        const float x=at.x-center.x,y=at.y-center.y,z=at.z-center.z;
        p.min.x+=x;p.min.y+=y;p.min.z+=z;p.max.x+=x;p.max.y+=y;p.max.z+=z;map.SetSpecialBox(i,p);
    }break;
    default:break;
    }
}

void EditorUi::CommitFunctionalPlacement(MapDocument& map, SceneRenderer& scene) {
    if(!functionalGhostValid_ || functionalPlacement_==FunctionalPlacement::None || map.Version().empty()) return;
    const auto at=functionalGhostPosition_;
    MapDocument before=map;
    if(functionalPasteActive_ && functionalClipboardKind_!=FunctionalType::None) {
        const auto from=functionalClipboardAnchor_;
        const float dx=at.x-from.x, dy=at.y-from.y, dz=at.z-from.z;
        switch(functionalClipboardKind_) {
        case FunctionalType::Flag: {
            if(!map.AddFlag(functionalClipboardFlag_.team,at)) {
                SetMessage("This map already has that team flag. Select it to move it.",true);return;
            }
            functionalIndex_=map.CtfFlags().size()-1;break;
        }
        case FunctionalType::Spawn: {
            auto item=functionalClipboardSpawn_;item.legacySourceIndex=-1;item.position=at;
            functionalIndex_=map.AddSpawn(item);break;
        }
        case FunctionalType::Point: {
            auto item=functionalClipboardPoint_;item.legacySourceIndex=-1;item.position=at;
            functionalIndex_=map.AddControlPoint(item);break;
        }
        case FunctionalType::Bonus: {
            auto item=functionalClipboardBonus_;item.legacySourceIndex=-1;
            item.min.x+=dx;item.min.y+=dy;item.min.z+=dz;
            item.max.x+=dx;item.max.y+=dy;item.max.z+=dz;
            functionalIndex_=map.AddBonusRegion(item);break;
        }
        case FunctionalType::Zone: {
            auto item=functionalClipboardZone_;item.legacySourceIndex=-1;
            item.min.x+=dx;item.min.y+=dy;item.min.z+=dz;
            item.max.x+=dx;item.max.y+=dy;item.max.z+=dz;
            functionalIndex_=map.AddSpecialBox(item);break;
        }
        default:return;
        }
        functionalSelected_=functionalClipboardKind_;
        history_.PushSnapshot(std::move(before),map);
        RequestSceneRebuild(true);
        SetMessage("Gameplay element pasted with its original properties. Undo restores the map.");
        return;
    }
    switch(functionalPlacement_) {
    case FunctionalPlacement::RedFlag:
    case FunctionalPlacement::BlueFlag: {
        const std::string team=functionalPlacement_==FunctionalPlacement::RedFlag?"red":"blue";
        if(!map.AddFlag(team,at)) {SetMessage("This map already has a "+team+" flag. Select it to move it.",true);return;}
        functionalSelected_=FunctionalType::Flag;functionalIndex_=map.CtfFlags().size()-1;break;
    }
    case FunctionalPlacement::SpawnDm:
    case FunctionalPlacement::SpawnRed:
    case FunctionalPlacement::SpawnBlue:
    case FunctionalPlacement::SpawnDomRed:
    case FunctionalPlacement::SpawnDomBlue: {
        SpawnMarker p;p.position=at;p.rotationZ=newSpawnYaw_;
        if(functionalPlacement_==FunctionalPlacement::SpawnDomRed || functionalPlacement_==FunctionalPlacement::SpawnDomBlue) {
            p.type="dom"; p.team=functionalPlacement_==FunctionalPlacement::SpawnDomRed?"red":"blue";
        } else p.type=functionalPlacement_==FunctionalPlacement::SpawnDm?"dm":
            functionalPlacement_==FunctionalPlacement::SpawnRed?"red":"blue";
        functionalIndex_=map.AddSpawn(p);functionalSelected_=FunctionalType::Spawn;break;
    }
    case FunctionalPlacement::ControlPoint: {
        ControlPointMarker p;p.position=at;p.name="Point "+std::to_string(map.ControlPoints().size()+1);p.distance=1000;
        functionalIndex_=map.AddControlPoint(p);functionalSelected_=FunctionalType::Point;break;
    }
    case FunctionalPlacement::BonusRegion: {
        BonusRegionMarker p;
        if(!map.Bonuses().empty()) {p=map.Bonuses().front();p.legacySourceIndex=-1;}
        p.name="free-bonus";
        p.min={at.x-250,at.y-250,at.z};p.max={at.x+250,at.y+250,at.z+300};
        p.bonusType=newBonusTypeOverride_.empty() ?
            GameplayAuthoring::KnownBonusTypes[std::clamp(newBonusKind_,0,6)] : newBonusTypeOverride_;
        // A missing <game-mode> has unverified game semantics; never silently
        // reassign an unchecked region to DM (or write ambiguous empty modes).
        if(!GameplayAuthoring::ValidBonusType(p.bonusType) || !GameplayAuthoring::HasExplicitModes(newBonusModes_)) {
            bonusModeExplanation_=true;
            return;
        }
        p.free=newBonusFree_;
        p.parachute=newBonusParachute_;
        p.modes=GameplayAuthoring::SelectedModes(newBonusModes_);
        functionalIndex_=map.AddBonusRegion(p);functionalSelected_=FunctionalType::Bonus;break;
    }
    case FunctionalPlacement::KillZone:
    case FunctionalPlacement::KickZone: {
        SpecialBox p;p.action=functionalPlacement_==FunctionalPlacement::KillZone?"kill":"kick";
        p.min={at.x-250,at.y-250,at.z-100};p.max={at.x+250,at.y+250,at.z+250};
        functionalIndex_=map.AddSpecialBox(p);functionalSelected_=FunctionalType::Zone;break;
    }
    default:return;
    }
    history_.PushSnapshot(std::move(before),map);
    scene.SetFunctionalGhost({},0,false);
    RequestSceneRebuild(true);
    Log::Info("Gameplay element added. Undo available.");
}

void EditorUi::DeleteFunctional(MapDocument& map, SceneRenderer& scene) {
    MapDocument before=map;
    bool removed=false;
    switch(functionalSelected_) {
    case FunctionalType::Flag:removed=map.DeleteFlag(functionalIndex_);break;
    case FunctionalType::Spawn:removed=map.DeleteSpawn(functionalIndex_);break;
    case FunctionalType::Point:removed=map.DeleteControlPoint(functionalIndex_);break;
    case FunctionalType::Bonus:removed=map.DeleteBonusRegion(functionalIndex_);break;
    case FunctionalType::Zone:removed=map.DeleteSpecialBox(functionalIndex_);break;
    case FunctionalType::Light:removed=map.DeleteLight(functionalIndex_);break;
    default:break;
    }
    if(removed) {history_.PushSnapshot(std::move(before),map);functionalSelected_=FunctionalType::None;RequestSceneRebuild(true);scene.SetFunctionalGhost({},0,false);SetMessage("Functional element removed. Undo restores it.");}
}

void EditorUi::Undo(MapDocument& map, SceneRenderer& scene) {
    std::vector<size_t> changed;
    if (!history_.Undo(map,changed)) {Log::Info("Undo requested with no available history entry.");return;}
    Log::Info("Map Undo applied; affected prop transforms="+std::to_string(changed.size())+
        " current collision counts planes="+std::to_string(map.CollisionPlanes().size())+
        " boxes="+std::to_string(map.CollisionBoxes().size())+
        " triangles="+std::to_string(map.CollisionTriangles().size()));
    for (size_t i:changed) if (i<map.Props().size()) scene.UpdatePropTransform(static_cast<int>(i),map.Props()[i]);
    if (!changed.empty()) SelectOnly(static_cast<int>(changed.back()),scene);
    else { SelectOnly(-1,scene); functionalSelected_=FunctionalType::None; RequestSceneRebuild(true); }
}

void EditorUi::Redo(MapDocument& map, SceneRenderer& scene) {
    std::vector<size_t> changed;
    if (!history_.Redo(map,changed)) return;
    for (size_t i:changed) if (i<map.Props().size()) scene.UpdatePropTransform(static_cast<int>(i),map.Props()[i]);
    if (!changed.empty()) SelectOnly(static_cast<int>(changed.back()),scene);
    else { SelectOnly(-1,scene); functionalSelected_=FunctionalType::None; RequestSceneRebuild(true); }
}

void EditorUi::DeleteSelected(MapDocument& map, SceneRenderer& scene) {
    if(selectedItems_.empty())return;
    std::vector<int> indices=selectedItems_;
    std::sort(indices.begin(),indices.end());
    indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
    MapDocument before=map;
    std::vector<size_t> valid;
    for(int index:indices)if(index>=0 && static_cast<size_t>(index)<map.Props().size())valid.push_back(static_cast<size_t>(index));
    std::string outcome;
    if(!map.DeletePropsWithCollision(valid,outcome)) { SetMessage(outcome,true); return; }
    history_.PushSnapshot(std::move(before),map);
    SelectOnly(-1,scene);drag_={};dragBefore_.clear();dragIndices_.clear();
    RequestSceneRebuild(true);
    SetMessage(outcome);
}

void EditorUi::RememberCopiedAssets(const AssetRegistry& assets) {
    // AX Library stores asset identities, not independently invented game objects.
    // Keep the copied prop's exact library/group/name for later placement.
    for(const auto& prop:clipboard_) {
        const auto& all=assets.Assets();
        const auto asset=std::find_if(all.begin(),all.end(),[&](const AssetDefinition& a){
            return a.library==prop.library && a.group==prop.group && a.name==prop.name;
        });
        if(asset==all.end())continue;
        const size_t id=static_cast<size_t>(asset-all.begin());
        const auto old=std::find_if(recentAssets_.begin(),recentAssets_.end(),[&](const RecentAsset& entry){return entry.index==id;});
        if(old!=recentAssets_.end()) {RecentAsset existing=*old;recentAssets_.erase(old);recentAssets_.insert(recentAssets_.begin(),std::move(existing));}
        else recentAssets_.insert(recentAssets_.begin(),RecentAsset{id,{}});
        if(!recentAssets_.empty() && recentAssets_.front().index==id) {
            const auto variant=std::find_if(asset->textures.begin(),asset->textures.end(),[&](const TextureVariant& v){return v.name==prop.texture;});
            recentAssets_.front().textureVariant=variant==asset->textures.end()?0:static_cast<int>(variant-asset->textures.begin());
        }
        if(recentAssets_.size()>24)recentAssets_.resize(24);
    }
    axCurrent_=0;
}

void EditorUi::StartClipboardPlacement() {
    if (clipboard_.empty()) return;
    placementItems_=clipboard_;
    clipboardPlacement_=true;
    placementTemplate_=placementItems_.front(); ghostRotation_=0.0f;
    // Clipboard positions are relative to their shared pivot. Caller keeps placementZ_.
    placementActive_=true; ghostValid_=false;
    SetMessage("Copied group follows the cursor. Space drops it; RMB cancels.");
}

bool EditorUi::AuthorCollisionForPlacement(MapDocument& map,const AssetRegistry& assets,
                                           size_t index,std::string& explanation) {
    const auto& prop=map.Props()[index];
    if(prop.hasInvalidNativeMetadata) {
        explanation="Malformed native game flags cannot be copied even with opaque XML approval.";
        return false;
    }
    if(prop.hasUncopyableMetadata && !prop.allowOpaqueMetadataCopy) {
        explanation="Opaque source metadata needs explicit copy approval; it will not be discarded.";
        return false;
    }
    if(prop.nativeWithCollision==0) {
        explanation="Copied prop stores native <with_collision>0</with_collision>. Its engine semantics are unverified; automatic collider generation is blocked.";
        return false;
    }
    if(VerifiedCollisionTemplates::Available(prop.library,prop.group,prop.name)) {
        if(map.AddVerifiedCollisionForProp(index)) {
            explanation="Original map-verified native wall template: 6 planes + 10 triangles.";
            return true;
        }
        explanation="Original wall template could not be authored (duplicate/ambiguous geometry).";
        return false;
    }
    const auto* asset=assets.Find(prop.library,prop.group,prop.name);
    if(!asset) {explanation="Asset identity not found in the selected game library.";return false;}
    if(asset->mesh.empty()) {
        explanation="Sprite asset has no static 3DS collision helpers.";
        return false;
    }
    if(LegacyMeshImport::Lower(asset->mesh.extension().string())!=".3ds") {
        explanation="Native authoring is available only for supported original 3DS helper geometry.";
        return false;
    }
    const auto imported=NativeCollisionImport::Read(asset->mesh);
    if(!imported.Valid()) {explanation=imported.error;return false;}
    if(!map.AddImportedCollisionForProp(index,imported)) {
        explanation="Native helper collision refused (overlapping duplicate or unsupported transform).";
        return false;
    }
    explanation="3DS native helpers: "+std::to_string(imported.planes.size())+" planes / "+
        std::to_string(imported.triangles.size())+" triangles; in-game test pending.";
    return true;
}

void EditorUi::CommitPlacement(MapDocument& map, const AssetRegistry& assets, SceneRenderer& scene) {
    if (!placementActive_ || !ghostValid_ || ghostProps_.empty() || map.Version().empty()) return;
    MapDocument before=map;
    std::vector<int> inserted;
    size_t nativeCount=0,visualOnly=0;
    size_t planes=0,triangles=0;
    std::string failure;
    for (auto p:ghostProps_) {
        if(p.hasUncopyableMetadata)p.allowOpaqueMetadataCopy=allowOpaqueMetadataCopy_;
        const auto index=map.AddProp(std::move(p));
        const auto oldPlanes=map.CollisionPlanes().size(),oldTriangles=map.CollisionTriangles().size();
        std::string info;
        if(AuthorCollisionForPlacement(map,assets,index,info)) {
            ++nativeCount;
            planes+=map.CollisionPlanes().size()-oldPlanes;
            triangles+=map.CollisionTriangles().size()-oldTriangles;
        } else {
            const auto& pending=map.Props()[index];
            if(pending.hasInvalidNativeMetadata) {
                failure=pending.name+": malformed native metadata cannot be copied ("+info+")";
                break;
            }
            if(pending.hasUncopyableMetadata && !pending.allowOpaqueMetadataCopy) {
                failure=pending.name+": unknown source metadata requires explicit approval ("+info+")";
                break;
            }
            const auto* pendingAsset=assets.Find(pending.library,pending.group,pending.name);
            const bool intentionalSprite=pendingAsset && !pendingAsset->sprite.empty();
            // An original 3DS which has NO native collision nodes is distinct
            // from an original 3DS with unsupported/invalid collision nodes.
            // The former is not assigned a fictional collider, and can be
            // placed as original visual-only content without the emergency
            // "allow unsupported" override. Explicit <with_collision>1 is
            // still rejected above; nonempty unsupported helpers still fail.
            const bool originalWithoutHelpers=info==NativeCollisionImport::NoNativeHelpersError;
            // A copied native with_collision=1 is not safe to claim as a
            // visual-only duplicate: its per-instance game flag would survive
            // while no collision geometry was authored for the new instance.
            if(pending.nativeWithCollision==1) {
                failure=pending.name+": copied original <with_collision>1</with_collision> but no complete new collider set ("+info+")";
                break;
            }
            if(!allowVisualOnlyPlacement_ && !intentionalSprite && !originalWithoutHelpers) {
                failure=pending.name+": "+info;break;
            }
            ++visualOnly;
        }
        inserted.push_back(static_cast<int>(index));
    }
    allowOpaqueMetadataCopy_=false; // approval is never carried into a later placement
    if(!failure.empty()) {
        Log::Warning("Placement transaction rolled back: "+failure+
            " (candidate additions were staged, not committed).");
        map=std::move(before); // all-or-nothing, no ghost wall left in the document
        SetMessage("Placement blocked: "+failure+". Check the advanced opaque-copy and visual-only options before retrying.",true);
        return;
    }
    Log::Info("Placement committed: "+std::to_string(inserted.size())+
        " props, native="+std::to_string(nativeCount)+
        " visualOnly="+std::to_string(visualOnly));
    selectedItems_=inserted; selected_=inserted.back(); scene.SetSelection(selectedItems_);
    history_.PushSnapshot(std::move(before),map); RequestSceneRebuild(true);
    if(visualOnly)SetMessage("Placed "+std::to_string(inserted.size())+" props; "+
        std::to_string(visualOnly)+" without authored native collision (tank may pass through them).",true);
    else SetMessage("Placed "+std::to_string(nativeCount)+" props with "+
        std::to_string(planes)+" native planes and "+std::to_string(map.CollisionBoxes().size())+" total boxes and "+std::to_string(triangles)+
        " native triangles. Validate new 3DS helper types in ProTLVK.");
}

void EditorUi::UpdatePlacementGhost(SceneRenderer& scene, const AssetRegistry& assets, float x, float y) {
    if (!placementActive_ || placementItems_.empty()) { ghostValid_=false; ghostProps_.clear(); scene.ClearGhost(); return; }
    DirectX::XMFLOAT3 target{};
    if (!scene.ScreenToLegacyPlane(x,y,placementZ_,target)) {
        ghostValid_=false; ghostProps_.clear(); scene.ClearGhost(); return;
    }
    // Keep the copied prop's original grid phase (e.g. tile centres at 250, 750).
    // A centroid or origin snap shifts edge-aligned tiles by half a tile.
    ghostPivot_=clipboardPlacement_ ? DirectX::XMFLOAT3{
        GridStep::QuantizeAroundAnchor(target.x,gridSize_,clipboardAnchor_.x),
        GridStep::QuantizeAroundAnchor(target.y,gridSize_,clipboardAnchor_.y),placementZ_} :
        DirectX::XMFLOAT3{SnapPosition(target.x),SnapPosition(target.y),SnapPosition(placementZ_)};
    if(surfaceOffsetEnabled_ && !clipboardPlacement_) ghostPivot_.z+=surfaceOffsetZ_;
    ghostProps_=placementItems_;
    const float cs=std::cos(ghostRotation_),sn=std::sin(ghostRotation_);
    for (auto& p:ghostProps_) {
        const float x=p.position.x, y=p.position.y;
        p.position.x=ghostPivot_.x+cs*x-sn*y;
        p.position.y=ghostPivot_.y+sn*x+cs*y;
        p.position.z+=ghostPivot_.z;
        p.rotation.z+=ghostRotation_;
    }
    ghostValid_=true;
    scene.SetGhost(ghostProps_,assets);
    if(edgeSnapEnabled_ && !clipboardPlacement_ && ghostProps_.size()==1) {
        float dx=0.f,dy=0.f;
        if(scene.SuggestEdgeSnap(ghostProps_,edgeSnapTolerance_,edgeSnapClearance_,dx,dy)) {
            ghostPivot_.x+=dx;ghostPivot_.y+=dy;
            ghostProps_[0].position.x+=dx;ghostProps_[0].position.y+=dy;
            scene.SetGhost(ghostProps_,assets);
        }
    }
}

bool EditorUi::Save(MapDocument& map, bool saveAs) {
    if (map.Version().empty()) return false;
    if (map.Path().empty()) saveAs = true;
    std::filesystem::path destination = map.Path();
    if (saveAs) { destination = saveXml(GetActiveWindow(), map.Path()); if (destination.empty()) return false; }
    std::string error;
    const bool ok = saveAs ? map.SaveLegacyAs(destination, error) : map.SaveLegacy(error);
    if (ok) { Log::Info("UI save succeeded: " + Log::PathUtf8(map.Path())); SetMessage("Legacy-compatible map saved: " + map.Path().filename().string()); }
    else { Log::Error("UI save failed: " + error); SetMessage(error, true); }
    return ok;
}

bool EditorUi::CaptureNativeWheel(short delta) {
    // ImGui processes wheel input during NewFrame, so clearing MouseWheel in Draw is too late.
    // Route placement/history wheel at the Win32 boundary to prevent dockspace scrolling.
    const bool tab=(GetAsyncKeyState(VK_TAB)&0x8000)!=0 && navigationMode_==NavigationMode::Simple;
    if (!tab || browseLibraryOpen_) return false;
    pendingNativeWheel_ += static_cast<float>(delta)/static_cast<float>(WHEEL_DELTA);
    return true;
}

void EditorUi::HandleEditorShortcuts(MapDocument& map, SceneRenderer& scene, const AssetRegistry& assets) {
    ImGuiIO& io=ImGui::GetIO();
    if (Pressed(Action::Fullscreen)) fullscreenToggleRequested_=true;
    if (browseLibraryOpen_) {
        axTabHeld_=false; placementWheel_=0.0f;
        if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape,false)) browseLibraryOpen_=false;
        return; // Browse Library owns the whole editing workspace until closed.
    }
    // Tab is momentary: do not steal typing from any input or modal.
    axTabHeld_=!io.WantTextInput && navigationMode_==NavigationMode::Simple && ImGui::IsKeyDown(ImGuiKey_Tab);
    placementWheel_=pendingNativeWheel_; pendingNativeWheel_=0.0f;
    if (axTabHeld_ && io.MouseWheel!=0.0f) {
        placementWheel_=io.MouseWheel;
        io.MouseWheel=0.0f; // Do not scroll dock panels or zoom while browsing placement history.
    }
    if (io.WantTextInput) return;

    if (objectEditorOpen_) {
        // Route object shortcuts BEFORE any map command. This window owns model edits.
        if(Pressed(Action::Save) || Pressed(Action::SaveAs)) objectSaveRequested_=true;
        // Object authoring owns Undo/Redo for its whole lifetime. Never change the map behind it.
        if (io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Z,false)) UndoObject();
        if (io.KeyCtrl && !io.KeyAlt && (ImGui::IsKeyPressed(ImGuiKey_Y,false) ||
            (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z,false)))) RedoObject();
        return;
    }
    if (Pressed(Action::Open)) { RequestGuidedOpen(1); return; }
    if (Pressed(Action::SaveAs)) { Save(map,true); return; }
    if (Pressed(Action::Save)) { Save(map,false); return; }
    if (Pressed(Action::Undo)) { Undo(map,scene); return; }
    if (Pressed(Action::Redo)) { Redo(map,scene); return; }
    if(io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_A,false)) {
        SelectAllStaticProps(map,scene);return;
    }

    if (functionalPlacement_!=FunctionalPlacement::None &&
        (ImGui::IsKeyPressed(ImGuiKey_Space,false) || Pressed(Action::Place))) {
        functionalCommitRequested_=true; return;
    }
    // X rotates a selected spawn as well as a not-yet-placed spawn.
    if (!io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_X,false) &&
        functionalPlacement_==FunctionalPlacement::None && functionalSelected_==FunctionalType::Spawn &&
        functionalIndex_<map.Spawns().size()) {
        MapDocument before=map;
        auto spawn=map.Spawns()[functionalIndex_];
        // Independent of static-prop rotation snap: eight headings per turn.
        spawn.rotationZ=GameplayAuthoring::RotateSpawn(spawn.rotationZ,io.KeyShift);
        map.SetSpawn(functionalIndex_,spawn);
        history_.PushSnapshot(std::move(before),map);RequestSceneRebuild(true);
        return;
    }
    if (!io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_X,false) &&
        (functionalPlacement_==FunctionalPlacement::SpawnDm || functionalPlacement_==FunctionalPlacement::SpawnRed ||
         functionalPlacement_==FunctionalPlacement::SpawnBlue || functionalPlacement_==FunctionalPlacement::SpawnDomRed ||
         functionalPlacement_==FunctionalPlacement::SpawnDomBlue)) {
        newSpawnYaw_=GameplayAuthoring::RotateSpawn(newSpawnYaw_,io.KeyShift);
        return;
    }
    if (functionalPlacement_!=FunctionalPlacement::None && Pressed(Action::Cancel)) {
        functionalPlacement_=FunctionalPlacement::None; functionalPasteActive_=false; functionalGhostValid_=false; scene.SetFunctionalGhost({},0,false);
        SetMessage("Functional placement cancelled.");return;
    }
    if(io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_C,false) &&
       functionalSelected_!=FunctionalType::None) {
        DirectX::XMFLOAT3 at{};
        if(FunctionalPosition(map,at)) {
            functionalClipboardKind_=functionalSelected_;
            functionalClipboardAnchor_=at;
            const auto index=functionalIndex_;
            switch(functionalSelected_) {
            case FunctionalType::Flag: functionalClipboardFlag_=map.CtfFlags()[index];break;
            case FunctionalType::Spawn: functionalClipboardSpawn_=map.Spawns()[index];break;
            case FunctionalType::Point: functionalClipboardPoint_=map.ControlPoints()[index];break;
            case FunctionalType::Bonus: functionalClipboardBonus_=map.Bonuses()[index];break;
            case FunctionalType::Zone: functionalClipboardZone_=map.SpecialBoxes()[index];break;
            default:functionalClipboardKind_=FunctionalType::None;break;
            }
            clipboard_.clear();
            if(functionalClipboardKind_!=FunctionalType::None)SetMessage("Gameplay properties copied. Ctrl+V previews a duplicate.");
        }
        return;
    }
    if (io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_C,false) && !selectedItems_.empty()) {
        clipboard_.clear();functionalClipboardKind_=FunctionalType::None;functionalPasteActive_=false;
        for (int index:selectedItems_) if (index>=0 && static_cast<size_t>(index)<map.Props().size())
            clipboard_.push_back(map.Props()[static_cast<size_t>(index)]);
        if (!clipboard_.empty()) {
            const DirectX::XMFLOAT3 center=clipboard_.front().position;
            clipboardAnchor_=center;
            for (auto& p:clipboard_) { p.position.x-=center.x; p.position.y-=center.y; p.position.z-=center.z; }
            // The target plane matches the original selection height.
            placementZ_=center.z;
            RememberCopiedAssets(assets); // identical original library assets appear in AX recents
            SetMessage("Copied "+std::to_string(clipboard_.size())+" prop(s). Ctrl+V follows cursor.");
        }
        return;
    }
    if(io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_V,false) &&
       functionalClipboardKind_!=FunctionalType::None) {
        FunctionalPlacement type=FunctionalPlacement::None;
        switch(functionalClipboardKind_) {
        case FunctionalType::Flag: type=functionalClipboardFlag_.team=="blue"?FunctionalPlacement::BlueFlag:FunctionalPlacement::RedFlag;break;
        case FunctionalType::Spawn: type=FunctionalPlacement::SpawnDm;break;
        case FunctionalType::Point: type=FunctionalPlacement::ControlPoint;break;
        case FunctionalType::Bonus: type=FunctionalPlacement::BonusRegion;break;
        case FunctionalType::Zone: type=functionalClipboardZone_.action=="kick"?FunctionalPlacement::KickZone:FunctionalPlacement::KillZone;break;
        default:break;
        }
        if(type!=FunctionalPlacement::None) {
            BeginFunctionalPlacement(type,scene);
            functionalPasteActive_=true;placementZ_=functionalClipboardAnchor_.z;
            SetMessage("Move copied gameplay element; Space places it, RMB cancels.");
        }
        return;
    }
    if (io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_V,false) && !clipboard_.empty()) {
        const float height=placementZ_;
        StartClipboardPlacement(); placementZ_=height; return;
    }

    if (navigationMode_ != NavigationMode::Custom && !io.KeyCtrl && !io.KeyAlt &&
        ImGui::IsKeyPressed(ImGuiKey_Keypad0,false)) { gridFocusRequested_=true; return; }
    constexpr Action gridActions[]={Action::Grid100,Action::Grid200,Action::Grid300,Action::Grid400,Action::Grid500};
    for (int i=0;i<5;++i) if (Pressed(gridActions[i])) { gridSize_=float((i+1)*100); SetMessage("Grid: "+std::to_string((i+1)*100)); return; }
    if (placementActive_ && !io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_X,false)) {
        constexpr float pi=3.14159265358979323846f;
        ghostRotation_ += (io.KeyShift ? -1.0f : 1.0f) *
            (rotationSnapDeg_>0.01f ? rotationSnapDeg_ : 45.0f)*pi/180.0f;
        return;
    }
    if (Pressed(Action::Select)) tool_=ToolMode::Select;
    if (Pressed(Action::Move)) tool_=ToolMode::Move;
    if (Pressed(Action::Rotate)) tool_=ToolMode::Rotate;
    // G toggles native XML geometry regardless of the navigation profile.
    if (!io.KeyCtrl && !io.KeyAlt && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_G,false)) {
        showCollision_=!showCollision_;
        if(showCollision_) {
            placementActive_=false;placementCommitRequested_=false;ghostValid_=false;
            ghostProps_.clear();scene.ClearGhost();
            functionalPlacement_=FunctionalPlacement::None;functionalCommitRequested_=false;
            functionalGhostValid_=false;scene.SetFunctionalGhost({},0,false);
        }
        return;
    }
    if (Pressed(Action::Grid)) showGrid_=!showGrid_;
    if (Pressed(Action::Bounds)) showBounds_=!showBounds_;
    if (Pressed(Action::Gameplay)) { showGameplay_=!showGameplay_; if(!showGameplay_) showSpawns_=showFlags_=showPoints_=showBonuses_=showZones_=false; }
    if (Pressed(Action::Zones)) {showZones_=!showZones_; if(showZones_) showGameplay_=true;}
    if (Pressed(Action::Frame)) { if(selected_>=0) scene.FrameSelection(); else scene.FrameScene(); }
    if (placementActive_ && (ImGui::IsKeyPressed(ImGuiKey_Space,false) || Pressed(Action::Place))) {
        placementCommitRequested_=true; return;
    }
    if (Pressed(Action::Place) && selectedAsset_>=0) { BeginPlacement(assets); return; }
    if (Pressed(Action::Cancel)) {
        if (placementActive_) { placementActive_=false; ghostProps_.clear(); scene.ClearGhost(); SetMessage("Placement cancelled."); }
        else { drag_={}; SelectOnly(-1,scene); }
        return;
    }
    if (Pressed(Action::Delete) || (navigationMode_!=NavigationMode::Custom && ImGui::IsKeyPressed(ImGuiKey_Backspace,false))) {
        if (functionalSelected_!=FunctionalType::None) DeleteFunctional(map,scene);
        else DeleteSelected(map,scene);
        return;
    }
    if (functionalSelected_!=FunctionalType::None && selectedItems_.empty() && functionalPlacement_==FunctionalPlacement::None) {
        DirectX::XMFLOAT3 old{}; if(!FunctionalPosition(map,old))return;
        float right=0,forward=0,height=0;
        if(Pressed(Action::MoveXP,true))right+=1;if(Pressed(Action::MoveXN,true))right-=1;
        if(Pressed(Action::MoveYP,true))forward+=1;if(Pressed(Action::MoveYN,true))forward-=1;
        if(Pressed(Action::MoveZP,true))height+=1;if(Pressed(Action::MoveZN,true))height-=1;
        if(right!=0||forward!=0||height!=0) {
            DirectX::XMFLOAT3 r{1,0,0},f{0,1,0};
            if(navigationMode_==NavigationMode::Simple || navigationMode_==NavigationMode::Custom)scene.CameraMoveBasisLegacy(r,f);
            const float step=GridStep::KeyboardStep(gridSize_,io.KeyShift);
            old.x+=step*(r.x*right+f.x*forward);old.y+=step*(r.y*right+f.y*forward);old.z+=step*height;
    if(functionalSelected_==FunctionalType::Zone && functionalIndex_<map.SpecialBoxes().size()) {
                const auto before=map.SpecialBoxes()[functionalIndex_];
                MoveFunctional(map,old);
                history_.PushZone(functionalIndex_,before,map.SpecialBoxes()[functionalIndex_]);
            } else {
                MapDocument before=map;
                MoveFunctional(map,old);history_.PushSnapshot(std::move(before),map);
            }
            RequestSceneRebuild(true);
        }
        return;
    }
    if (placementActive_) {
        // While the ghost follows the cursor, WASD/QE navigate the CAMERA, not the object.
        const float speed=std::max(500.0f,gridSize_*2.0f)*std::clamp(io.DeltaTime,0.0f,0.05f)*4.0f;
        const float right=float(ImGui::IsKeyDown(ImGuiKey_D))-float(ImGui::IsKeyDown(ImGuiKey_A));
        const float forward=float(ImGui::IsKeyDown(ImGuiKey_W))-float(ImGui::IsKeyDown(ImGuiKey_S));
        const float height=float(ImGui::IsKeyDown(ImGuiKey_E))-float(ImGui::IsKeyDown(ImGuiKey_Q));
        if (!io.KeyCtrl && !io.KeyAlt && (right!=0||forward!=0||height!=0)) {
            DirectX::XMFLOAT3 r{1,0,0},f{0,1,0};scene.CameraMoveBasisLegacy(r,f);
            scene.MoveCameraLegacy({speed*(r.x*right+f.x*forward),
                speed*(r.y*right+f.y*forward),speed*height});
        }
        return;
    }
    if (selectedItems_.empty()) return;
    const float step=GridStep::KeyboardStep(gridSize_,io.KeyShift);
    constexpr float pi=3.14159265358979323846f;
    const float angle=(rotationSnapDeg_>0.001f?rotationSnapDeg_:15.0f)*pi/180.0f*(io.KeyShift?0.1f:1.0f);
    const auto down=[&](Action action){ return Pressed(action,true); };
    float right=0, forward=0, height=0, rotation=0;
    if (down(Action::MoveXP)) right+=1; if (down(Action::MoveXN)) right-=1;
    if (down(Action::MoveYP)) forward+=1; if (down(Action::MoveYN)) forward-=1;
    if (down(Action::MoveZP)) height+=1; if (down(Action::MoveZN)) height-=1;
    // In Simple, X rotates forward. Shift+X reverses. Legacy/Custom keep their defined bindings.
    if (navigationMode_==NavigationMode::Simple && !io.KeyCtrl && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_X,false)) rotation+=io.KeyShift?-1.0f:1.0f;
    } else { if (down(Action::RotateP)) rotation+=1; if (down(Action::RotateN)) rotation-=1; }
    if (navigationMode_!=NavigationMode::Custom && !io.KeyCtrl && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow,false)) right+=1;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow,false)) right-=1;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow,false)) forward+=1;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow,false)) forward-=1;
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp,false)) height+=1;
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown,false)) height-=1;
    }
    if (right==0 && forward==0 && height==0 && rotation==0) return;
    DirectX::XMFLOAT3 basisRight{1,0,0},basisForward{0,1,0};
    if (navigationMode_==NavigationMode::Simple || navigationMode_==NavigationMode::Custom) scene.CameraMoveBasisLegacy(basisRight,basisForward);
    const DirectX::XMFLOAT3 delta{step*(basisRight.x*right+basisForward.x*forward),
        step*(basisRight.y*right+basisForward.y*forward),step*height};
    std::vector<TransformEdit> edits;
    for (int index:selectedItems_) {
        if (index<0 || static_cast<size_t>(index)>=map.Props().size()) continue;
        const auto before=StateOf(map.Props()[static_cast<size_t>(index)]);
        auto after=before;
        after.position.x+=delta.x; after.position.y+=delta.y; after.position.z+=delta.z;
        // Quantize the WORLD position, not only the camera-relative delta. This
        // removes fractional drift left by mouse placement and prior transforms.
        if (absoluteGridSnap_ && (right!=0 || forward!=0 || height!=0)) {
            const float cell=GridStep::KeyboardStep(gridSize_,io.KeyShift);
            if (right!=0 || forward!=0) {
                after.position.x=GridStep::Quantize(after.position.x,cell);
                after.position.y=GridStep::Quantize(after.position.y,cell);
            }
            if (height!=0) after.position.z=GridStep::Quantize(after.position.z,cell);
        }
        after.rotation.z+=angle*rotation;
        ApplyLiveTransform(map,scene,index,after);
        edits.push_back({static_cast<size_t>(index),before,after,"Keyboard transform"});
    }
    history_.PushBatch(std::move(edits));
}

void EditorUi::Draw(MapDocument& map, AssetRegistry& assets, SceneRenderer& scene, SceneRenderer& previewScene) {
    if(objectEditorOpening_ && GetTickCount64()-objectEditorOpenAt_>=1000ULL) {
        SplashScreen::Close(); objectEditorOpening_=false; objectEditorOpen_=true;
    }
    if(collisionBindingsPending_ && !assets.Root().empty() && !map.Version().empty()) {
        collisionBindingsPending_=false;
        size_t bound=0;
        std::unordered_map<std::string,NativeCollisionImport::Result> meshCache;
        for(size_t i=0;i<map.Props().size();++i) {
            const auto& prop=map.Props()[i];
            if(map.HasNativeCollisionForProp(i))continue;
            const auto* asset=assets.Find(prop.library,prop.group,prop.name);
            if(!asset||asset->mesh.empty())continue;
            const auto key=asset->mesh.string();
            auto it=meshCache.find(key);
            if(it==meshCache.end())it=meshCache.emplace(key,NativeCollisionImport::Read(asset->mesh)).first;
            if(it->second.Valid()&&map.BindImportedCollisionForProp(i,it->second))++bound;
        }
        if(bound)Log::Info("Native 3DS helper collision ownership rebound: "+std::to_string(bound)+" props.");
    }
    HandleEditorShortcuts(map, scene, assets);
    scene.AdvanceCameraFocus(ImGui::GetIO().DeltaTime);
    if (axTabHeld_ && !axTabWasHeld_ && !recentAssets_.empty() && !browseLibraryOpen_) {
        axCurrent_=std::clamp(axCurrent_,0,static_cast<int>(recentAssets_.size())-1);
        ActivateRecent(static_cast<size_t>(axCurrent_),assets,previewScene);
    }
    axTabWasHeld_=axTabHeld_;
    if (axTabHeld_ && placementWheel_!=0.0f && !recentAssets_.empty()) {
        axCurrent_=(axCurrent_+(placementWheel_<0?1:static_cast<int>(recentAssets_.size())-1))%static_cast<int>(recentAssets_.size());
        ActivateRecent(static_cast<size_t>(axCurrent_),assets,previewScene);
        placementWheel_=0.0f;
    }
    scene.SetEffectMode(effectMode_);
    scene.SetBackgroundColor({viewportBackground_[0],viewportBackground_[1],viewportBackground_[2]});
    scene.SetGridColor({viewportGridColor_[0],viewportGridColor_[1],viewportGridColor_[2]});
    previewScene.SetBackgroundColor({viewportBackground_[0],viewportBackground_[1],viewportBackground_[2]});
    previewScene.SetGridColor({viewportGridColor_[0],viewportGridColor_[1],viewportGridColor_[2]});
    objectScene_.SetBackgroundColor({viewportBackground_[0],viewportBackground_[1],viewportBackground_[2]});
    objectScene_.SetGridColor({viewportGridColor_[0],viewportGridColor_[1],viewportGridColor_[2]});
    // Multi-selection is owned by the editor; renderer only displays its bounds.
    scene.SetSelection(selectedItems_);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos); ImGui::SetNextWindowSize(vp->WorkSize); ImGui::SetNextWindowViewport(vp->ID);
    constexpr ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f); ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##Root", nullptr, rootFlags); ImGui::PopStyleVar(2);

    DrawMenu(map, scene); DrawToolbar(map, scene);
    browseWorkspaceY_=ImGui::GetCursorScreenPos().y;
    const ImGuiID dock = ImGui::GetID("GTanksDockspace");
    ImGui::DockSpace(dock, {0, -23}, ImGuiDockNodeFlags_PassthruCentralNode);

    static bool first = true;
    if (first) {
        first = false;
        ImGui::DockBuilderRemoveNode(dock); ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, ImGui::GetContentRegionAvail());
        ImGuiID center = dock, left, right; ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.16f, &left, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.23f, &right, &center);
        ImGuiID rightBottom; ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.35f, &rightBottom, &right);
        ImGui::DockBuilderDockWindow("Gameplay", left); ImGui::DockBuilderDockWindow("Library", right);
        ImGui::DockBuilderDockWindow("Scene", rightBottom); ImGui::DockBuilderDockWindow("Lighting", rightBottom); ImGui::DockBuilderDockWindow("Properties", rightBottom);
        ImGui::DockBuilderDockWindow("Viewport", center);
        ImGui::DockBuilderFinish(dock);
    }

    DrawStatus(map, assets, scene); ImGui::End();
    DrawScene(map, scene); DrawLibrary(map, assets, scene, previewScene); DrawProperties(map, assets, scene);
    DrawGameplay(map, scene); DrawLighting(map, scene); DrawViewport(map, assets, scene);
    DrawObjectEditor(assets,scene);
    if (firstGameplayFocus_) { ImGui::SetWindowFocus("Gameplay"); firstGameplayFocus_=false; }
    if (browsePreviewNeedsRestore_ && !browseLibraryOpen_) {
        browsePreviewNeedsRestore_=false; RebuildAssetPreview(assets,previewScene);
    }
    if (!browseLibraryOpen_) DrawAxLibrary(map, assets, scene, previewScene);
    if(showCustomThemePopup_) { ImGui::OpenPopup("Custom editor theme"); showCustomThemePopup_=false; }
    if(ImGui::BeginPopupModal("Custom editor theme",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        const bool a=ImGui::ColorEdit3("Panel color",customSurface_.data());
        const bool b=ImGui::ColorEdit3("Text color",customText_.data());
        if(a||b) { uiTheme_=3;ApplyPreferredTheme();SaveControls(); }
        if(ImGui::Button("Use custom theme")) {uiTheme_=3;ApplyPreferredTheme();SaveControls();ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Close##theme"))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if(showBackgroundPopup_) {ImGui::OpenPopup("Viewport background");showBackgroundPopup_=false;}
    if(ImGui::BeginPopupModal("Viewport background",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        if(ImGui::ColorEdit3("Background RGB",viewportBackground_))SaveControls();
        if(ImGui::ColorEdit3("Grid line RGB",viewportGridColor_))SaveControls();
        if(ImGui::Button("Restore dark blue background")) {viewportBackground_[0]=0.055f;viewportBackground_[1]=0.073f;viewportBackground_[2]=0.10f;SaveControls();}
        ImGui::SameLine();if(ImGui::Button("Close##background"))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (showToastOverlay_ && !browseLibraryOpen_) DrawToast(); DrawControlHelp(); DrawSupportPopup(); DrawFirstRunGuidance();
    // Last, full-workspace opaque window: masks Scene, Viewport, Library, Properties and Gameplay.
    if (browseLibraryOpen_) DrawBrowseLibrary(map, assets, scene, previewScene);
}

void EditorUi::DrawMenu(MapDocument& map, SceneRenderer& scene) {
    if (!ImGui::BeginMenuBar()) return;
    auto shortcut=[&](Action a) -> std::string { return showShortcuts_?Shortcut(a):std::string{}; };
    if (ImGui::BeginMenu("File")) {
        ImGui::BeginDisabled(objectEditorOpen_);
        if (ImGui::MenuItem("New map...")) newMapRequested_=true;
        if (ImGui::MenuItem("Open map...",shortcut(Action::Open).c_str())) RequestGuidedOpen(1);
        if (ImGui::MenuItem("Save map",shortcut(Action::Save).c_str(),false,!map.Version().empty())) Save(map,false);
        if (ImGui::MenuItem("Save map as...",shortcut(Action::SaveAs).c_str(),false,!map.Version().empty())) Save(map,true);
        ImGui::EndDisabled();
        if(objectEditorOpen_ && ImGui::MenuItem("Save object draft...","Ctrl+S")) objectSaveRequested_=true;
        ImGui::Separator(); if (ImGui::MenuItem("Set library folder...")) RequestGuidedOpen(2);
        ImGui::Separator(); if (ImGui::MenuItem("Exit")) PostMessageW(GetActiveWindow(),WM_CLOSE,0,0); ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo",shortcut(Action::Undo).c_str(),false,objectEditorOpen_?!objectUndo_.empty():history_.CanUndo())) { if(objectEditorOpen_)UndoObject();else Undo(map,scene); }
        if (ImGui::MenuItem("Redo",shortcut(Action::Redo).c_str(),false,objectEditorOpen_?!objectRedo_.empty():history_.CanRedo())) { if(objectEditorOpen_)RedoObject();else Redo(map,scene); }
        if (ImGui::MenuItem("Select all static props","Ctrl+A",false,!objectEditorOpen_ && !map.Props().empty()))
            SelectAllStaticProps(map,scene);
        if (ImGui::MenuItem("Delete selected",shortcut(Action::Delete).c_str(),false,!objectEditorOpen_ && (selected_>=0 || functionalSelected_!=FunctionalType::None))) {
            if(functionalSelected_!=FunctionalType::None)DeleteFunctional(map,scene);else DeleteSelected(map,scene);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Grid",shortcut(Action::Grid).c_str(),&showGrid_);
        ImGui::MenuItem("Bounds",shortcut(Action::Bounds).c_str(),&showBounds_);
        if(ImGui::MenuItem("Gameplay overlays",shortcut(Action::Gameplay).c_str(),&showGameplay_) && !showGameplay_)
            showSpawns_=showFlags_=showPoints_=showBonuses_=showZones_=false;
        if(ImGui::MenuItem("Special / kill zones",shortcut(Action::Zones).c_str(),&showZones_) && showZones_) showGameplay_=true;
        ImGui::MenuItem("Status popups",nullptr,&showToastOverlay_);
        if(ImGui::MenuItem("Viewport background..."))showBackgroundPopup_=true;
        if (ImGui::BeginMenu("Editor theme")) {
            static constexpr const char* themes[]={"Graphite", "Matte dark green", "Silver gray"};
            for(int i=0;i<3;++i) if(ImGui::MenuItem(themes[i],nullptr,uiTheme_==i)) {
                uiTheme_=i; ApplyPreferredTheme(); SaveControls();
            }
            if(ImGui::MenuItem("Custom theme...",nullptr,uiTheme_==3))showCustomThemePopup_=true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Effects")) {
            for (int mode=0;mode<7;++mode)
                if (ImGui::MenuItem(EffectName(mode),nullptr,effectMode_==mode)) effectMode_=mode;
            ImGui::EndMenu();
        }
        ImGui::MenuItem("AX Library: only used",nullptr,&axOnlyUsed_);
        if (ImGui::MenuItem("Gameplay inspector...")) ImGui::SetWindowFocus("Gameplay");
        if (ImGui::MenuItem("Geometry / collision view (G)",nullptr,showCollision_)) {
            showCollision_=!showCollision_;
            if (showCollision_) {
                placementActive_=false;placementCommitRequested_=false;ghostValid_=false;
                ghostProps_.clear();scene.ClearGhost();
                functionalPlacement_=FunctionalPlacement::None;functionalCommitRequested_=false;
                functionalGhostValid_=false;scene.SetFunctionalGhost({},0,false);
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Display actual XML collision surfaces, not material colors or passability.");
        ImGui::Separator();
        if (ImGui::MenuItem(selected_>=0?"Frame selection":"Frame map",shortcut(Action::Frame).c_str())) { if(selected_>=0) scene.FrameSelection(); else scene.FrameScene(); }
        if (ImGui::MenuItem("Reference-side camera direction")) scene.ResetReferenceViewDirection();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset only the camera angle, without modifying XML, object rotations or physics.");
        if (ImGui::MenuItem("View from opposite side (180 degrees)")) scene.ReverseViewDirection();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Compare an imported map from the other side. This is a camera-only operation.");
        if (ImGui::MenuItem("Fullscreen (F11)",shortcut(Action::Fullscreen).c_str(),fullscreen_)) fullscreenToggleRequested_=true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Controls")) {
        if (ImGui::MenuItem("Legacy",nullptr,navigationMode_==NavigationMode::Legacy)) SetNavigationMode(NavigationMode::Legacy);
        if (ImGui::MenuItem("Adobe-style",nullptr,navigationMode_==NavigationMode::Adobe)) SetNavigationMode(NavigationMode::Adobe);
        if (ImGui::MenuItem("Simple focus",nullptr,navigationMode_==NavigationMode::Simple)) SetNavigationMode(NavigationMode::Simple);
        if (ImGui::MenuItem("Custom",nullptr,navigationMode_==NavigationMode::Custom)) SetNavigationMode(NavigationMode::Custom);
        ImGui::Separator();
        if (ImGui::MenuItem("Show shortcuts",nullptr,showShortcuts_)) { showShortcuts_=!showShortcuts_; SaveControls(); }
        if (ImGui::MenuItem("Controls manual and settings...")) showControlHelp_=true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools")) {
        if(ImGui::MenuItem("Placement settings...")) showPlacementSettings_=true;
        if(ImGui::MenuItem("Absolute grid snap",nullptr,absoluteGridSnap_)) absoluteGridSnap_=!absoluteGridSnap_;
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Quantize the final world position during keyboard movement, not only the movement delta.");
        ImGui::Separator();
        ImGui::BeginDisabled(); ImGui::MenuItem("Map validator (not implemented)"); ImGui::MenuItem("Profiler (not implemented)"); ImGui::EndDisabled(); ImGui::Separator();
        if (ImGui::MenuItem("Open logs folder")) { Log::Flush(); launchWindowsPath(Log::LogDirectory(),false); }
        if (ImGui::MenuItem("Open current session log")) { Log::Flush(); launchWindowsPath(Log::SessionFile(),true); }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Support")) {
        if (ImGui::MenuItem("ProTanki Discord"))
            ShellExecuteW(nullptr,L"open",L"https://discord.gg/4GRZymqYG3",nullptr,nullptr,SW_SHOWNORMAL);
        if (ImGui::MenuItem("Contact Author")) showAuthorPopup_=true;
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
    if(showPlacementSettings_) {
        ImGui::SetNextWindowSize({445.f,0.f},ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Placement settings##tools",&showPlacementSettings_,
                        ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDocking)) {
            ImGui::Checkbox("Absolute grid snap (keyboard movement)",&absoluteGridSnap_);
            HoverHelp("When enabled, final world coordinates snap to Grid Step; Shift uses one tenth step. Existing maps are not modified automatically.");
            ImGui::Checkbox("Geometric edge snap (new ghost, orthogonal)",&edgeSnapEnabled_);
            HoverHelp("Uses actual world mesh bounds to align adjacent edges when placing a single 0/90/180/270-degree object. No snapping to rotated diagonal or different-floor meshes.");
            if(edgeSnapEnabled_) {
                ImGui::SetNextItemWidth(125.f);ImGui::InputFloat("Edge tolerance (units)",&edgeSnapTolerance_,0,0,"%.2f");
                ImGui::SetNextItemWidth(125.f);ImGui::InputFloat("Horizontal edge gap",&edgeSnapClearance_,0,0,"%.2f");
                edgeSnapTolerance_=std::clamp(edgeSnapTolerance_,0.01f,100.f);
                edgeSnapClearance_=std::clamp(edgeSnapClearance_,0.f,10.f);
                HoverHelp("0 means exact shared edge, not overlapping coplanar surfaces. This is NOT a vertical material offset.");
            }
            ImGui::Checkbox("New-object surface Z offset",&surfaceOffsetEnabled_);
            HoverHelp("Opt-in height offset for new props, to separate coplanar surfaces. The object's native collision follows its changed Z too; do NOT use for a collision-free decal.");
            if(surfaceOffsetEnabled_) {
                ImGui::SetNextItemWidth(125.f);ImGui::InputFloat("Z offset (units)",&surfaceOffsetZ_,0,0,"%.3f");
                surfaceOffsetZ_=std::clamp(surfaceOffsetZ_,0.f,20.f);
            }
            ImGui::Checkbox("Allow visual-only props (no tank collision)",&allowVisualOnlyPlacement_);
            HoverHelp("Only for deliberate visual placement. New unsupported 3DS solids will not receive collision.");
            ImGui::Checkbox("Allow opaque XML copy for NEXT placement (advanced)",&allowOpaqueMetadataCopy_);
            HoverHelp("One-shot approval for copying original per-instance XML whose semantics are unknown; never automatically proves gameplay equivalence.");
        }
        ImGui::End();
    }
}

void EditorUi::DrawToolbar(MapDocument& map, SceneRenderer& scene) {
    auto label=[&](const char* n,Action a) { return ToolbarLabel(n,a); };
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{7,3});
    ImGui::BeginDisabled(false);
    if (ImGui::Button(label("Undo",Action::Undo).c_str())) { if(objectEditorOpen_)UndoObject();else if(history_.CanUndo())Undo(map,scene); } ImGui::SameLine();
    if (ImGui::Button(label("Redo",Action::Redo).c_str())) { if(objectEditorOpen_)RedoObject();else if(history_.CanRedo())Redo(map,scene); }
    ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
    if (ToolButton(label("Select",Action::Select).c_str(),tool_==ToolMode::Select)) tool_=ToolMode::Select; ImGui::SameLine();
    if (ToolButton(label("Move",Action::Move).c_str(),tool_==ToolMode::Move)) tool_=ToolMode::Move; ImGui::SameLine();
    if (ToolButton(label("Rotate",Action::Rotate).c_str(),tool_==ToolMode::Rotate)) tool_=ToolMode::Rotate;
    ImGui::SameLine(); ImGui::BeginDisabled(objectEditorOpen_); if (ImGui::Button(label("Delete",Action::Delete).c_str())) DeleteSelected(map,scene); ImGui::EndDisabled();
    ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
    if (ToolButton("Geometry view [G]",showCollision_)) {
        showCollision_=!showCollision_;
        if (showCollision_) {
            // Geometry mode hides props; never allow a pending invisible ghost to be stamped.
            placementActive_=false;placementCommitRequested_=false;ghostValid_=false;
            ghostProps_.clear();scene.ClearGhost();
            functionalPlacement_=FunctionalPlacement::None;functionalCommitRequested_=false;
            functionalGhostValid_=false;scene.SetFunctionalGhost({},0,false);
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Diagnostic view of native XML physics; click again for textured editing.");
    ImGui::SameLine();
    ImGui::TextDisabled("Step:"); ImGui::SameLine();
    if (gridFocusRequested_) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(68); ImGui::InputFloat("##grid_size",&gridSize_,0.0f,0.0f,"%.0f",ImGuiInputTextFlags_EnterReturnsTrue);
    gridFocusRequested_=false;
    gridSize_=std::clamp(gridSize_,1.0f,10000.0f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Movement always snaps to this step. Numpad 1..5 = 100..500; Shift + keyboard move = one tenth step.");
    ImGui::SameLine(); const auto gridLabel=label("Grid",Action::Grid); ToolbarToggle(gridLabel.c_str(),showGrid_);
    ImGui::SameLine(); const auto boundsLabel=label("Bounds",Action::Bounds); ToolbarToggle(boundsLabel.c_str(),showBounds_);
    ImGui::EndDisabled();
    // Centered, background-free green Test link with a vector triangle (no missing font glyphs).
    const float testX=ImGui::GetWindowWidth()*0.5f-36.0f;
    ImGui::SameLine(std::max(testX,ImGui::GetCursorPosX()+8.0f));
    const ImVec2 testPos=ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##test_map",{78,21});
    if (ImGui::IsItemClicked()) RequestGuidedOpen(3);
    auto* draw=ImGui::GetWindowDrawList();
    const ImU32 green=IM_COL32(103,210,130,255);
    draw->AddTriangleFilled({testPos.x+2,testPos.y+4},{testPos.x+2,testPos.y+17},
        {testPos.x+13,testPos.y+10.5f},green);
    draw->AddText({testPos.x+19,testPos.y+3},green,"Test...");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Launch the original ProTLVK32.exe. The first click asks for its location and remembers it. No map/configuration is modified.");
    ImGui::SameLine();if(ImGui::Button("[ ] Obj. Editor") && !objectEditorOpen_ && !objectEditorOpening_) {
        objectEditorOpening_=true; objectEditorOpenAt_=GetTickCount64();
        SplashScreen::Open(GetModuleHandleW(nullptr));
        SplashScreen::Status(L"Preparing 3D object workspace...");
    }
    ImGui::PopStyleVar(); ImGui::Separator();
}

void EditorUi::DrawScene(MapDocument& map, SceneRenderer& scene) {
    ImGui::Begin("Scene");
    if (browseLibraryOpen_) { ImGui::End(); return; }
    static char filter[128]{}; ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##scene_filter", "Search objects...", filter, sizeof(filter)); ImGui::Spacing();
    if (ImGui::TreeNodeEx("Geometry")) {
        int index = 0;
        for (const auto& p : map.Props()) {
            if (!ContainsInsensitive(p.name, filter) && !ContainsInsensitive(p.library, filter)) { ++index; continue; }
            char label[320]; std::snprintf(label, sizeof(label), "%s##%d", p.name.c_str(), index);
            if (ImGui::Selectable(label, selected_ == index)) { selected_ = index; scene.SetSelected(index); if (CurrentNavigationSettings().focusSelectionOnClick) scene.FocusSelectionKeepDistance(smoothCameraFocus_); }
            ++index;
        }
        ImGui::TreePop();
    }
    if (ImGui::Button("Open gameplay inspector...")) ImGui::SetWindowFocus("Gameplay");
    char collisionLabel[96]; std::snprintf(collisionLabel, sizeof(collisionLabel), "Collision (%zu)", map.Stats().collisionPlanes + map.Stats().collisionBoxes + map.Stats().collisionTriangles);
    if(ImGui::TreeNode(collisionLabel)) {
        HoverHelp("Collision is stored separately from visible props; deleting a visual alone can leave an invisible obstacle. Positions come from the real XML.");
        ImGui::Checkbox("Show all colliders",&showAllColliders_);
        // Windows headers may define `near` as a legacy macro. Avoid that identifier here.
        const bool hasSelectedProp=selected_>=0 && static_cast<size_t>(selected_)<map.Props().size();
        const DirectX::XMFLOAT3 pivot=hasSelectedProp?map.Props()[static_cast<size_t>(selected_)].position:DirectX::XMFLOAT3{};
        auto matches=[&](DirectX::XMFLOAT3 at) {
            if(showAllColliders_)return true;
            if(!hasSelectedProp)return false;
            return std::fabs(at.x-pivot.x)<650 && std::fabs(at.y-pivot.y)<650 && std::fabs(at.z-pivot.z)<650;
        };
        if(!hasSelectedProp && !showAllColliders_)ImGui::TextDisabled("Select a prop to see nearby collision, or choose Show all.");
        auto colliderList=[&](MapDocument::ColliderKind kind,const char* type,const auto& values) {
            size_t displayed=0;
            for(size_t i=0;i<values.size();++i) {
                const auto at=values[i].position;
                if(!matches(at))continue;
                if(++displayed>150 && showAllColliders_) {ImGui::TextDisabled("Showing first 150; select a prop to filter.");break;}
                // Linked native helper sets belong to their prop. Hide the
                // misleading individual Remove action and show ownership.
                if(values[i].authoredOwnerIndex>=0) {
                    ImGui::TextDisabled("Linked %s #%zu (prop #%d)  (%.1f, %.1f, %.1f)",
                        type,i+1,values[i].authoredOwnerIndex+1,at.x,at.y,at.z);
                    HoverHelp("Delete or transform the parent object to change its complete native collision set.");
                    continue;
                }
                const auto label=std::string("Remove##collision_")+type+std::to_string(i);
                if(ImGui::SmallButton(label.c_str())) {
                    MapDocument before=map;
                    if(map.DeleteCollider(kind,i)) {
                        history_.PushSnapshot(std::move(before),map);
                        RequestSceneRebuild(true);
                        SetMessage(std::string("Removed ")+type+" collision #"+std::to_string(i+1)+" from XML. Undo restores it.");
                    } else SetMessage("This native helper belongs to a prop; delete the prop to remove the complete collider set.",true);
                    break;
                }
                ImGui::SameLine();ImGui::Text("%s #%zu  (%.1f, %.1f, %.1f)",type,i+1,at.x,at.y,at.z);
            }
        };
        colliderList(MapDocument::ColliderKind::Plane,"Plane",map.CollisionPlanes());
        colliderList(MapDocument::ColliderKind::Box,"Box",map.CollisionBoxes());
        colliderList(MapDocument::ColliderKind::Triangle,"Triangle",map.CollisionTriangles());
        ImGui::TreePop();
    }
    ImGui::End();
}

void EditorUi::SelectAsset(const AssetRegistry& assets, size_t index, SceneRenderer& previewScene) {
    if (index >= assets.Assets().size()) return;
    selectedAsset_ = static_cast<int>(index); selectedTextureVariant_ = 0;
    RebuildAssetPreview(assets, previewScene);
    const auto old=std::find_if(recentAssets_.begin(),recentAssets_.end(),[&](const RecentAsset& item){return item.index==index;});
    if (old!=recentAssets_.end()) { RecentAsset entry=*old; recentAssets_.erase(old); recentAssets_.insert(recentAssets_.begin(),std::move(entry)); }
    else recentAssets_.insert(recentAssets_.begin(),RecentAsset{index,{}});
    if (recentAssets_.size()>24) recentAssets_.resize(24);
    axCurrent_=0; BeginPlacement(assets);
}

void EditorUi::RebuildAssetPreview(const AssetRegistry& assets, SceneRenderer& previewScene) {
    assetPreviewReady_ = false;
    if (selectedAsset_ < 0 || static_cast<size_t>(selectedAsset_) >= assets.Assets().size()) { previewScene.ClearScene(); return; }
    const auto& asset = assets.Assets()[static_cast<size_t>(selectedAsset_)];
    std::string variant;
    if (!asset.textures.empty()) {
        selectedTextureVariant_ = std::clamp(selectedTextureVariant_, 0, static_cast<int>(asset.textures.size()) - 1);
        variant = asset.textures[static_cast<size_t>(selectedTextureVariant_)].name;
    }
    std::string error;
    assetPreviewReady_ = previewScene.BuildAssetPreview(asset, variant, error);
    if (!assetPreviewReady_ && !error.empty()) SetMessage("Preview: " + error, true);
}

void EditorUi::BeginPlacement(const AssetRegistry& assets) {
    if (selectedAsset_ < 0 || static_cast<size_t>(selectedAsset_) >= assets.Assets().size()) return;
    const auto& a = assets.Assets()[static_cast<size_t>(selectedAsset_)];
    placementTemplate_ = {};
    placementTemplate_.library = a.library; placementTemplate_.group = a.group; placementTemplate_.name = a.name;
    if (!a.textures.empty()) {
        const int index = std::clamp(selectedTextureVariant_, 0, static_cast<int>(a.textures.size()) - 1);
        placementTemplate_.texture = a.textures[static_cast<size_t>(index)].name;
    }
    placementTemplate_.rotation = {0,0,0}; placementTemplate_.position={0,0,0};
    clipboardPlacement_=false;
    placementItems_={placementTemplate_}; placementActive_=true; ghostValid_=false; ghostRotation_=0.0f;
    SetMessage("Move cursor over map; Space places "+a.name+". RMB cancels.");
}

void EditorUi::DrawLibrary(MapDocument& map, const AssetRegistry& assets, SceneRenderer& scene, SceneRenderer& previewScene) {
    ImGui::Begin("Library");
    if (browseLibraryOpen_) { ImGui::End(); return; }
    if (ImGui::Button("Library")) axPinned_=false;
    ImGui::SameLine();
    if (ImGui::Button("Browse Library")) { browseLibraryOpen_=true; axPinned_=false; axTabHeld_=false; }
    ImGui::SameLine();
    if (ImGui::Button("AX")) { axPinned_=!axPinned_; axPinnedOpenedAt_=ImGui::GetTime(); }
    ImGui::Separator();
    static char search[128]{}; ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##asset_filter", "Search props...", search, sizeof(search)); ImGui::Spacing();
    if (!assets.AssetCount()) {
        ImGui::TextDisabled("No legacy library indexed"); ImGui::Spacing();
        ImGui::TextWrapped("File > Set library folder... and select the original ProTanki 'library' directory.");
        ImGui::End(); return;
    }
    ImGui::TextDisabled("%zu libraries / %zu assets", assets.LibraryCount(), assets.AssetCount());

    const auto& all = assets.Assets();
    const float listHeight = std::max(130.0f, ImGui::GetContentRegionAvail().y * 0.42f);
    ImGui::BeginChild("##asset_list", {0,listHeight}, true);
    if (search[0]) {
        for (size_t i=0;i<all.size();++i) {
            const auto& a=all[i];
            const std::string full=a.library+" / "+a.group+" / "+a.name;
            if (!ContainsInsensitive(full, search)) continue;
            if (ImGui::Selectable((full+"##asset"+std::to_string(i)).c_str(), selectedAsset_==static_cast<int>(i))) SelectAsset(assets,i,previewScene);
        }
    } else {
        size_t i=0;
        while (i<all.size()) {
            const std::string lib=all[i].library; const size_t libStart=i; while (i<all.size() && all[i].library==lib) ++i; const size_t libEnd=i;
            ImGui::PushID(lib.c_str());
            if (ImGui::TreeNodeEx(lib.c_str())) {
                size_t g=libStart;
                while (g<libEnd) {
                    const std::string group=all[g].group; const size_t groupStart=g; while (g<libEnd && all[g].group==group) ++g;
                    ImGui::PushID(group.c_str());
                    if (ImGui::TreeNodeEx(group.empty()?"default":group.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (size_t k=groupStart;k<g;++k) {
                            if (ImGui::Selectable((all[k].name+"##asset"+std::to_string(k)).c_str(), selectedAsset_==static_cast<int>(k))) SelectAsset(assets,k,previewScene);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    if (selectedAsset_ >= 0 && static_cast<size_t>(selectedAsset_) < all.size()) {
        const auto& a=all[static_cast<size_t>(selectedAsset_)];
        ImGui::SeparatorText("Preview"); ImGui::Text("%s", a.name.c_str()); ImGui::SameLine(); ImGui::TextDisabled("%s / %s", a.library.c_str(), a.group.c_str());
        ImGui::Spacing();
        if (!a.textures.empty()) {
            const char* current=a.textures[static_cast<size_t>(std::clamp(selectedTextureVariant_,0,static_cast<int>(a.textures.size())-1))].name.c_str();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##texture_variant", current)) {
                for (int t=0;t<static_cast<int>(a.textures.size());++t) {
                    if (ImGui::Selectable(a.textures[static_cast<size_t>(t)].name.c_str(), selectedTextureVariant_==t)) { selectedTextureVariant_=t;
                        if(!recentAssets_.empty() && recentAssets_.front().index==static_cast<size_t>(selectedAsset_)) {
                            recentAssets_.front().textureVariant=t;
                            recentAssets_.front().thumbnail.Reset();
                        }
                        RebuildAssetPreview(assets,previewScene); if (placementActive_) BeginPlacement(assets); }
                }
                ImGui::EndCombo();
            }
        }

        ImVec2 previewSize{ImGui::GetContentRegionAvail().x, std::min(220.0f, ImGui::GetContentRegionAvail().x * 0.68f)};
        previewSize.x=std::max(previewSize.x,80.0f); previewSize.y=std::max(previewSize.y,90.0f);
        previewScene.Resize(static_cast<unsigned>(previewSize.x), static_cast<unsigned>(previewSize.y)); previewScene.Render(true,false,false,false);
        if (assetPreviewReady_ && previewScene.Output()) ImGui::Image((ImTextureID)previewScene.Output(),previewSize);
        else { ImGui::InvisibleButton("##preview_empty",previewSize); ImGui::TextDisabled("Preview unavailable"); }
        if (ImGui::IsItemHovered()) {
            ImGuiIO& io=ImGui::GetIO(); if (io.MouseWheel!=0) previewScene.Zoom(io.MouseWheel,1.0f);
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right,0)) previewScene.Orbit(io.MouseDelta.x,io.MouseDelta.y);
            // Eight deliberate left-clicks rotate exactly one full turn (45 degrees each).
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) previewScene.Orbit(-130.8996939f,0.0f);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to rotate");
        CaptureRecentThumbnail(previewScene);
        if(placementActive_ && !clipboardPlacement_) {
            ImGui::TextDisabled("Left click: Place object  |  Right click: Cancel");
            HoverHelp("In the map viewport: left click places this asset; right click cancels. Click the preview itself to rotate it.");
        }
        // Choosing an asset already starts its cursor-following placement.
        // The preview is a clean, interactive image without duplicate placement controls.
    } else {
        ImGui::TextDisabled("Select an object to preview");
    }
    ImGui::End();
}

// Full-workspace browser. Library names are metadata-only until the user opens one.
// One visible thumbnail is generated per frame; hidden/collapsed categories do no GPU work.
void EditorUi::CaptureBrowseThumbnail(size_t index, size_t variantIndex, const AssetRegistry& assets, SceneRenderer& previewScene) {
    if (browseRenderedThisFrame_ || index>=assets.Assets().size()) return;
    const uint64_t key=(static_cast<uint64_t>(index)<<32)|static_cast<uint64_t>(variantIndex);
    auto it=browseThumbnails_.find(key);
    if (it!=browseThumbnails_.end()) { it->second.touched=browseFrame_; return; }
    browseRenderedThisFrame_=true;
    const auto& asset=assets.Assets()[index];
    std::string variant;
    if (!asset.textures.empty() && variantIndex<asset.textures.size()) variant=asset.textures[variantIndex].name;
    std::string error;
    BrowseThumbnail thumb; thumb.touched=browseFrame_;
    const bool built=previewScene.BuildAssetPreview(asset,variant,error);
    if (built) {
        thumb.dimensions=previewScene.PreviewDimensionsLegacy();
        thumb.hasDimensions=true;
        previewScene.Resize(194,146);
        previewScene.Render(false,false,false,false);
        Microsoft::WRL::ComPtr<ID3D11Resource> raw;
        if (auto* output=previewScene.Output()) output->GetResource(raw.GetAddressOf());
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        if (raw && SUCCEEDED(raw.As(&texture))) {
            D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
            desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags=0; desc.MiscFlags=0; desc.Usage=D3D11_USAGE_DEFAULT;
            Microsoft::WRL::ComPtr<ID3D11Device> device;
            texture->GetDevice(device.GetAddressOf());
            if (device) {
                Microsoft::WRL::ComPtr<ID3D11Texture2D> copy;
                if (SUCCEEDED(device->CreateTexture2D(&desc,nullptr,copy.GetAddressOf()))) {
                    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
                    device->GetImmediateContext(context.GetAddressOf());
                    if (context) context->CopyResource(copy.Get(),texture.Get());
                    device->CreateShaderResourceView(copy.Get(),nullptr,thumb.srv.GetAddressOf());
                }
            }
        }
    }
    thumb.failed=!thumb.srv;
    browseThumbnails_.insert_or_assign(key,std::move(thumb));
    previewScene.ReleasePreviewResources(); // Map renderer has its own separate caches.
    // Fixed upper bound: 96 * 194 * 146 * 4 bytes ~= 10.9 MiB of thumbnail pixels.
    constexpr size_t budget=96;
    if (browseThumbnails_.size()>budget) {
        auto victim=browseThumbnails_.begin();
        for (auto i=browseThumbnails_.begin();i!=browseThumbnails_.end();++i)
            if (i->second.touched<victim->second.touched) victim=i;
        browseThumbnails_.erase(victim);
    }
}

void EditorUi::DrawBrowseLibrary(MapDocument& map, const AssetRegistry& assets,
                                  SceneRenderer& scene, SceneRenderer& previewScene) {
    (void)map; (void)scene;
    ++browseFrame_; browseRenderedThisFrame_=false;
    ImGuiViewport* viewport=ImGui::GetMainViewport();
    const float top=std::max(browseWorkspaceY_,viewport->WorkPos.y);
    const ImVec2 pos{viewport->WorkPos.x,top};
    const ImVec2 dimensions{viewport->WorkSize.x,
        std::max(100.0f,viewport->WorkPos.y+viewport->WorkSize.y-top)};
    ImGui::SetNextWindowPos(pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize(dimensions,ImGuiCond_Always);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowBgAlpha(1.0f);
    constexpr ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoDocking|
        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoScrollbar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18,14});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.035f,0.045f,0.058f,1.0f));
    if (!ImGui::Begin("Browse Library##full_workspace",nullptr,flags)) {
        ImGui::End();ImGui::PopStyleColor();ImGui::PopStyleVar();return;
    }
    ImGui::TextUnformatted("Browse Library");
    ImGui::SameLine(); ImGui::TextDisabled("| Open a library to load its visible thumbnails");
    ImGui::SameLine(ImGui::GetWindowWidth()-90.0f);
    if (ImGui::Button("X Close##browse")) { browseLibraryOpen_=false; browsePreviewNeedsRestore_=true; }
    ImGui::Separator();
    ImGui::SetNextItemWidth(std::min(400.0f,ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##browse_search","Filter library / objects...",browseSearch_,sizeof(browseSearch_));
    static constexpr const char* kinds[]={"All types","Ground / tiles","Walls / fences","Buildings","Bridges / ramps","Vegetation / rocks","Props / other","Sprites"};
    ImGui::SetNextItemWidth(200.0f);ImGui::Combo("Type##browse",&browseCategory_,kinds,IM_ARRAYSIZE(kinds));
    ImGui::SameLine();
    static constexpr const char* sorts[]={"Library order","Name A-Z","Type, then name"};
    ImGui::SetNextItemWidth(170.0f);ImGui::Combo("Order##browse",&browseSort_,sorts,IM_ARRAYSIZE(sorts));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(145.f);ImGui::SliderFloat("Thumbnail size##browse",&browseThumbnailScale_,0.7f,1.65f,"%.2fx");
    if(ImGui::IsItemDeactivatedAfterEdit())SaveControls();
    ImGui::Separator();
    const auto& items=assets.Assets();
    if (items.empty()) ImGui::TextDisabled("Set the external ProTanki library folder in File first.");
    ImGui::BeginChild("##browse_scroll",{0,0},false,ImGuiWindowFlags_AlwaysVerticalScrollbar);
    size_t start=0;
    while (start<items.size()) {
        const auto& library=items[start].library;
        size_t finish=start+1;
        while (finish<items.size()&&items[finish].library==library)++finish;
        size_t matched=0;
        const bool filter=browseSearch_[0]!=0;
        for (size_t i=start;i<finish;++i) {
            const auto& a=items[i];
            if ((!filter || ContainsInsensitive(a.library+" / "+a.group+" / "+a.name,browseSearch_)) &&
                (browseCategory_==0 || BrowseKind(a)==browseCategory_))++matched;
        }
        if (!matched) {start=finish;continue;}
        ImGui::PushID(library.c_str());
        // UI is deliberately flat beneath each library: the native `group`
        // remains intact in AssetDefinition and therefore in exported XML.
        const std::string heading=library+"  ("+std::to_string(matched)+" objects)";
        if (ImGui::TreeNodeEx(heading.c_str(),ImGuiTreeNodeFlags_SpanAvailWidth)) {
            struct BrowseTile { size_t asset, variant; };
            std::vector<BrowseTile> visible;
            for (size_t i=start;i<finish;++i) {
                const auto& a=items[i];
                if (browseCategory_!=0 && BrowseKind(a)!=browseCategory_) continue;
                const size_t count=std::max(size_t{1},a.textures.size());
                for (size_t variant=0;variant<count;++variant) {
                    const std::string name=a.library+" / "+a.group+" / "+a.name+" / "+
                        (a.textures.empty()?std::string("default"):a.textures[variant].name);
                    if (!filter || ContainsInsensitive(name,browseSearch_)) visible.push_back({i,variant});
                }
            }
            if (browseSort_!=0) std::stable_sort(visible.begin(),visible.end(),[&](const BrowseTile& a,const BrowseTile& b) {
                if(browseSort_==2 && BrowseKind(items[a.asset])!=BrowseKind(items[b.asset]))
                    return BrowseKind(items[a.asset])<BrowseKind(items[b.asset]);
                return Lower(items[a.asset].name)<Lower(items[b.asset].name);
            });
            const float cellWidth=222.0f*browseThumbnailScale_;
            const float rowHeight=206.f*browseThumbnailScale_;
            const ImVec2 thumbnailSize{194.f*browseThumbnailScale_,146.f*browseThumbnailScale_};
            const int columns=std::max(1,static_cast<int>(ImGui::GetContentRegionAvail().x/cellWidth));
            const int rows=(static_cast<int>(visible.size())+columns-1)/columns;
            ImGuiListClipper clipper;
            clipper.Begin(rows,rowHeight);
            while (clipper.Step()) {
                for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
                    const ImVec2 rowPos=ImGui::GetCursorScreenPos();
                    for(int col=0;col<columns;++col) {
                        const int at=row*columns+col;
                        if (at>=static_cast<int>(visible.size())) break;
                        const auto tile=visible[static_cast<size_t>(at)];
                        const size_t index=tile.asset;
                        const auto& a=items[index];
                        const uint64_t key=(static_cast<uint64_t>(index)<<32)|static_cast<uint64_t>(tile.variant);
                        ImGui::PushID(static_cast<int>(index));ImGui::PushID(static_cast<int>(tile.variant));
                        const ImVec2 startPos{rowPos.x+cellWidth*col,rowPos.y};
                        ImGui::SetCursorScreenPos(startPos);
                        ImGui::BeginGroup();
                        auto thumb=browseThumbnails_.find(key);
                        if (thumb==browseThumbnails_.end()) CaptureBrowseThumbnail(index,tile.variant,assets,previewScene);
                        auto stored=browseThumbnails_.find(key);
                        if (stored!=browseThumbnails_.end()) stored->second.touched=browseFrame_;
                        if (stored!=browseThumbnails_.end()&&stored->second.srv)
                            ImGui::Image((ImTextureID)stored->second.srv.Get(),thumbnailSize);
                        else {
                            ImGui::InvisibleButton("##thumbnail",thumbnailSize);
                            ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(),ImGui::GetItemRectMax(),IM_COL32(19,23,29,255));
                            ImGui::GetWindowDrawList()->AddText({startPos.x+10,startPos.y+thumbnailSize.y*.42f},IM_COL32(140,150,160,255),
                                stored!=browseThumbnails_.end()&&stored->second.failed?"Preview unavailable":"Loading preview...");
                        }
                        ImGui::SetCursorScreenPos(startPos);
                        if (ImGui::InvisibleButton("##select_asset",{thumbnailSize.x,189.f*browseThumbnailScale_})) {
                            SelectAsset(assets,index,previewScene);
                            selectedTextureVariant_=static_cast<int>(tile.variant);
                            if(!recentAssets_.empty() && recentAssets_.front().index==index) {
                                recentAssets_.front().textureVariant=selectedTextureVariant_;
                                recentAssets_.front().thumbnail.Reset();
                            }
                            RebuildAssetPreview(assets,previewScene);
                            BeginPlacement(assets);
                            browseLibraryOpen_=false; browsePreviewNeedsRestore_=false;
                        }
                        ImGui::SetCursorScreenPos({startPos.x,startPos.y+150.f*browseThumbnailScale_});
                        const std::string caption=a.name+(a.textures.empty()?std::string{}:" / "+a.textures[tile.variant].name);
                        ImGui::TextUnformatted(caption.c_str());
                        if(stored!=browseThumbnails_.end()&&stored->second.hasDimensions) {
                            const auto d=stored->second.dimensions;
                            ImGui::SetCursorScreenPos({startPos.x,startPos.y+171.f*browseThumbnailScale_});
                            ImGui::TextDisabled("%.0f x %.0f x %.0f",d.x,d.y,d.z);
                        }
                        ImGui::EndGroup();ImGui::PopID();ImGui::PopID();
                    }
                    ImGui::SetCursorScreenPos({rowPos.x,rowPos.y+rowHeight});
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();start=finish;
    }
    ImGui::EndChild();
    ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar();
}

EditorUi::ObjectSnapshot EditorUi::CaptureObjectSnapshot() const {
    return {objectDraft_,objectVisualVertices_,objectVisualIndices_,objectSelectedVertex_};
}
void EditorUi::PushObjectUndo() {
    // Bounded mesh snapshots. Never touch map Undo/Redo while authoring a model.
    if(objectUndo_.size()>=12)objectUndo_.erase(objectUndo_.begin());
    objectUndo_.push_back(CaptureObjectSnapshot());objectRedo_.clear();objectDirty_=true;
}
void EditorUi::RestoreObjectSnapshot(ObjectSnapshot&& state) {
    objectDraft_=std::move(state.draft);
    objectVisualVertices_=std::move(state.vertices);
    objectVisualIndices_=std::move(state.indices);
    objectSelectedVertex_=state.selectedVertex;
    objectVertexDragActive_=false;objectVertexDragHistoryCaptured_=false;objectFacePointCount_=0;
    if(objectSceneHasModel_ && objectMeshEditable_) {
        objectScene_.UpdatePreviewMeshGeometry(objectVisualVertices_,objectVisualIndices_);
        objectScene_.ApplyDraftPreviewTint(objectDraft_.tint);
    }
}
void EditorUi::UndoObject() {
    if(objectUndo_.empty())return;
    if(objectRedo_.size()>=12)objectRedo_.erase(objectRedo_.begin());
    objectRedo_.push_back(CaptureObjectSnapshot());
    auto previous=std::move(objectUndo_.back());objectUndo_.pop_back();
    RestoreObjectSnapshot(std::move(previous));
}
void EditorUi::RedoObject() {
    if(objectRedo_.empty())return;
    if(objectUndo_.size()>=12)objectUndo_.erase(objectUndo_.begin());
    objectUndo_.push_back(CaptureObjectSnapshot());
    auto next=std::move(objectRedo_.back());objectRedo_.pop_back();
    RestoreObjectSnapshot(std::move(next));
}
void EditorUi::ResetObjectHistory() {
    objectUndo_.clear();objectRedo_.clear();objectVertexDragHistoryCaptured_=false;objectDirty_=false;
}

bool EditorUi::SaveObjectDraft(const AssetRegistry& assets) {
    if(objectMeshEditable_ && !objectVisualVertices_.empty()) {
        objectDraft_.meshVertices.clear();objectDraft_.meshVertices.reserve(objectVisualVertices_.size());
        for(const auto& vertex:objectVisualVertices_)
            objectDraft_.meshVertices.push_back({vertex.x,vertex.y,vertex.z});
        objectDraft_.meshIndices=objectVisualIndices_;
    }
    std::filesystem::path saved;std::string error;
    if(!ObjectDraft::SaveNew(objectDraft_,objectDraftOutputRoot_,saved,error,assets.Root())) {
        SetMessage("Object draft save failed: "+error,true);return false;
    }
    objectDirty_=false;
    SetMessage("Object draft saved: "+Log::PathUtf8(saved));return true;
}

void EditorUi::DrawObjectEditor(const AssetRegistry& assets, SceneRenderer& scene) {
    if (!objectEditorOpen_) return;
    const ImGuiViewport* vp=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(),ImGuiCond_FirstUseEver,{0.5f,0.5f});
    // Size follows the current available editor workspace on each opening.
    // The user can still resize the window after it appears.
    ImGui::SetNextWindowSize({vp->WorkSize.x*0.94f,vp->WorkSize.y*0.94f},ImGuiCond_Appearing);
    bool windowOpen=true;
    const bool shown=ImGui::Begin("Object Editor##object_editor_0513",&windowOpen,
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoDocking);
    if(!windowOpen) {if(objectDirty_)objectCloseRequested_=true;else objectEditorOpen_=false;}
    if(!shown) {ImGui::End();return;}
    ImGui::TextDisabled("Object draft"); HoverHelp("Separate model workspace; original library files are never changed.");
    ImGui::SameLine();if(ImGui::SmallButton("?##obj_help"))objectHelpRequested_=true;
    ImGui::Separator();
    ImGui::BeginChild("##object_controls",{310,0},ImGuiChildFlags_Borders);
    if(ImGui::Button("Import GLB...")) objectPickerRequest_=1; ImGui::SameLine();
    if(ImGui::Button("Open 3DS...")) objectPickerRequest_=3;
    if(ImGui::Button("Open saved draft...")) objectPickerRequest_=4;
    if(ImGui::CollapsingHeader("Use existing library model")) {
        ImGui::InputTextWithHint("##obj_asset_search","Search original library...",objectLegacySearch_,sizeof(objectLegacySearch_));
        ImGui::BeginChild("##obj_existing",{0,130},ImGuiChildFlags_Borders);
        size_t shown=0;
        for(const auto& asset:assets.Assets()) {
            if(asset.mesh.empty() || !ContainsInsensitive(asset.library+" / "+asset.name,objectLegacySearch_)) continue;
            if(++shown>100) break; // bounded UI; narrow search to find more
            ImGui::PushID(static_cast<int>(shown));
            const std::string label=asset.library+" / "+asset.name;
            if(ImGui::Selectable(label.c_str())) {
                objectDraft_.model=asset.mesh;
                objectDraft_.name=asset.name.substr(0,127);
                objectDraft_.templateLibrary=asset.library;
                objectDraft_.templateGroup=asset.group;
                objectDraft_.templateName=asset.name;
                objectDraft_.templatePropXml=asset.originalPropXml;
                objectDraft_.excludedTemplateFields.clear();
                objectDraft_.libraryTemplateXml=asset.originalLibraryXml;
                objectDraft_.meshVertices.clear();objectDraft_.meshIndices.clear();objectDraft_.scale=1.f;
                ResetObjectHistory();objectDirty_=true;objectSelectedVertex_=-1;objectFacePointCount_=0;
                objectSceneNeedsBuild_=true; objectSceneHasModel_=false;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::Spacing();
    }
    ImGui::TextWrapped("Source: %s",objectDraft_.model.empty()?"None":objectDraft_.model.filename().string().c_str());
    if(ImGui::CollapsingHeader("Attach native library reference (read only)")) {
        HoverHelp("Copy ALL original library XML into draft sidecars, including unknown properties. This does not produce a native game export.");
        ImGui::InputTextWithHint("##obj_template_search","Search library / group / prop...",objectLegacySearch_,sizeof(objectLegacySearch_));
        ImGui::BeginChild("##obj_template_select",{0,110},ImGuiChildFlags_Borders);
        size_t shownTemplates=0;
        for(const auto& a:assets.Assets()) {
            if(!ContainsInsensitive(a.library+" / "+a.group+" / "+a.name,objectLegacySearch_))continue;
            if(++shownTemplates>100)break;
            ImGui::PushID(static_cast<int>(shownTemplates));
            if(ImGui::Selectable((a.library+" / "+a.group+" / "+a.name).c_str())) {
                PushObjectUndo();
                objectDraft_.templateLibrary=a.library;objectDraft_.templateGroup=a.group;objectDraft_.templateName=a.name;
                objectDraft_.templatePropXml=a.originalPropXml;objectDraft_.libraryTemplateXml=a.originalLibraryXml;
                objectDraft_.excludedTemplateFields.clear();
                objectDirty_=true;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    if(objectDraft_.libraryTemplateXml) {
        ImGui::TextWrapped("Original XML template: %s / %s / %s",objectDraft_.templateLibrary.c_str(),
            objectDraft_.templateGroup.c_str(),objectDraft_.templateName.c_str());
        if(ImGui::SmallButton("Clear source XML template")) {
            PushObjectUndo();objectDraft_.libraryTemplateXml.reset();objectDraft_.templatePropXml.clear();
            objectDraft_.templateLibrary.clear();objectDraft_.templateGroup.clear();objectDraft_.templateName.clear();
            objectDraft_.excludedTemplateFields.clear();
            objectDirty_=true;
        }
    } else { ImGui::TextDisabled("Template: none"); HoverHelp("A new draft does not inherit native gameplay properties unless a library template is attached."); }
    if(objectDraft_.libraryTemplateXml && ImGui::CollapsingHeader("Imported library properties (read only)")) {
        HoverHelp("Displays every original XML field of the selected template, including unrecognized fields. All fields are retained in the source sidecars; the draft does not export a game object.");
        DrawRawPropertyTree("Selected template <prop>",objectDraft_.templatePropXml);
        if(ImGui::TreeNode("Choose draft-inherited fields")) {
            HoverHelp("Unchecked fields are recorded in the isolated draft's selection manifest. Full original XML remains unchanged; native game export is not implemented.");
            pugi::xml_document source;
            if(source.load_buffer(objectDraft_.templatePropXml.data(),objectDraft_.templatePropXml.size())) {
                auto selected=objectDraft_.excludedTemplateFields;
                DrawSelectableXmlNode(source.child("prop"),"/prop",selected,0);
                if(selected!=objectDraft_.excludedTemplateFields) {
                    PushObjectUndo();objectDraft_.excludedTemplateFields=std::move(selected);
                }
            } else ImGui::TextDisabled("Template XML could not be parsed for selection.");
            ImGui::TreePop();
        }
        if(ImGui::TreeNode("Original library root attributes")) {
            pugi::xml_document full;
            if(full.load_buffer(objectDraft_.libraryTemplateXml->data(),objectDraft_.libraryTemplateXml->size())) {
                for(auto attr:full.child("library").attributes())ImGui::TextWrapped("@%s = %s",attr.name(),attr.value());
            }
            ImGui::TreePop();
        }
    }
    // Shared native helper inspector. Draft boxes remain independently editable;
    // GLB-to-game 3DS export is intentionally NOT claimed or silently performed.
    static std::filesystem::path inspectedSource;
    static NativeCollisionImport::Result inspectedHelpers;
    if(objectDraft_.model!=inspectedSource) {
        inspectedSource=objectDraft_.model;
        inspectedHelpers={};
        if(!inspectedSource.empty()&&LegacyMeshImport::Lower(inspectedSource.extension().string())==".3ds")
            inspectedHelpers=NativeCollisionImport::Read(inspectedSource);
    }
    if(LegacyMeshImport::Lower(objectDraft_.model.extension().string())==".3ds") {
        if(inspectedHelpers.Valid())
            ImGui::TextColored({.30f,.88f,.50f,1.f},"Native 3DS helpers: %zu planes / %zu boxes / %zu triangles",
                inspectedHelpers.planes.size(),inspectedHelpers.boxes.size(),inspectedHelpers.triangles.size());
        else ImGui::TextWrapped("Native helper import blocked: %s",inspectedHelpers.error.c_str());
        HoverHelp("Custom collision boxes below are separate editable drafts, not a native game export.");
    } else { ImGui::TextDisabled("GLB: collision draft"); HoverHelp("Edit solid/trigger boxes below. There is no native 3DS or ProTLVK game export yet."); }
    char draftName[128]{};
    std::memcpy(draftName,objectDraft_.name.data(),std::min(objectDraft_.name.size(),sizeof(draftName)-1));
    if(ImGui::InputText("Object name",draftName,sizeof(draftName))) {objectDraft_.name=draftName;objectDirty_=true;}
    float draftTint[3]={objectDraft_.tint[0],objectDraft_.tint[1],objectDraft_.tint[2]};
    if(ImGui::ColorEdit3("Draft tint",draftTint)) {
        PushObjectUndo();objectDraft_.tint={draftTint[0],draftTint[1],draftTint[2]};
        if(objectSceneHasModel_)objectScene_.ApplyDraftPreviewTint(objectDraft_.tint);
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::SeparatorText("Editable visual mesh");
    ImGui::Checkbox("Show face edges",&objectShowMeshEdges_);ImGui::SameLine();
    ImGui::Checkbox("Show vertices",&objectShowVertices_);
    ImGui::Spacing();
    if(objectSceneHasModel_ && objectMeshEditable_) {
        float scale=objectDraft_.scale;
        if(ImGui::InputFloat("Geometry scale",&scale,0,0,"%.5f") &&
           std::isfinite(scale) && scale>=0.001f && scale<=1000.f) {
            const float factor=scale/objectDraft_.scale;
            bool safe=true;
            for(const auto& vertex:objectVisualVertices_)
                if(std::abs(vertex.x*factor)>1000000.f || std::abs(vertex.y*factor)>1000000.f || std::abs(vertex.z*factor)>1000000.f)safe=false;
            if(safe) {
                PushObjectUndo();
                for(auto& vertex:objectVisualVertices_) {vertex.x*=factor;vertex.y*=factor;vertex.z*=factor;}
                objectDraft_.scale=scale;
                objectScene_.UpdatePreviewMeshGeometry(objectVisualVertices_,objectVisualIndices_);
            }
        }
        if(objectSelectedVertex_>=0 && static_cast<size_t>(objectSelectedVertex_)<objectVisualVertices_.size()) {
            auto& vertex=objectVisualVertices_[static_cast<size_t>(objectSelectedVertex_)];
            ImGui::Text("Selected point %d / %zu",objectSelectedVertex_+1,objectVisualVertices_.size());
            float coords[3]={vertex.x,vertex.y,vertex.z};
            if(ImGui::InputFloat3("Point X / Y / Z",coords,"%.3f")) {
                const DirectX::XMFLOAT3 edited{coords[0],coords[1],coords[2]};
                if(std::isfinite(edited.x)&&std::isfinite(edited.y)&&std::isfinite(edited.z) &&
                   std::abs(edited.x)<=1000000.f&&std::abs(edited.y)<=1000000.f&&std::abs(edited.z)<=1000000.f) {
                    PushObjectUndo();vertex=edited;objectScene_.UpdatePreviewMeshGeometry(objectVisualVertices_,objectVisualIndices_);
                }
            }
            if(ImGui::Button("Restore selected point") &&
               static_cast<size_t>(objectSelectedVertex_)<objectOriginalVertices_.size()) {
                const auto original=objectOriginalVertices_[static_cast<size_t>(objectSelectedVertex_)];
                PushObjectUndo();vertex={original.x*objectDraft_.scale,original.y*objectDraft_.scale,original.z*objectDraft_.scale};
                objectScene_.UpdatePreviewMeshGeometry(objectVisualVertices_,objectVisualIndices_);
            }
        }
        ImGui::SeparatorText("Mesh topology / point tools");
        HoverHelp("Ctrl+click three points to choose a triangle. New points start near the selection; new faces reuse the last source material. Draft only.");
        if(ImGui::Button("Add mesh point") && objectVisualVertices_.size()<200000) {
            PushObjectUndo();DirectX::XMFLOAT3 next{0,0,0};
            if(objectSelectedVertex_>=0 && static_cast<size_t>(objectSelectedVertex_)<objectVisualVertices_.size()) {
                next=objectVisualVertices_[static_cast<size_t>(objectSelectedVertex_)];next.x+=30.f;
            }
            objectVisualVertices_.push_back(next);
            objectSelectedVertex_=static_cast<int>(objectVisualVertices_.size())-1;
            objectScene_.UpdatePreviewMeshGeometry(objectVisualVertices_,objectVisualIndices_);
        }
        ImGui::Text("Triangle points: %d/3",objectFacePointCount_);
        if(objectFacePointCount_==3) {
            ImGui::Text("%d, %d, %d",objectFacePoints_[0]+1,objectFacePoints_[1]+1,objectFacePoints_[2]+1);
        }
        ImGui::BeginDisabled(objectFacePointCount_!=3 || objectVisualIndices_.size()+3>600000);
        if(ImGui::Button("Create triangle from points")) {
            PushObjectUndo();for(int i=0;i<3;++i)objectVisualIndices_.push_back(static_cast<uint32_t>(objectFacePoints_[i]));
            objectScene_.UpdatePreviewMeshGeometry(objectVisualVertices_,objectVisualIndices_);
            objectFacePointCount_=0;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();if(ImGui::Button("Clear triangle selection"))objectFacePointCount_=0;
        ImGui::BeginDisabled(objectVisualIndices_.size()<=objectOriginalIndices_.size());
        if(ImGui::Button("Remove last added triangle")) {
            PushObjectUndo();objectVisualIndices_.resize(objectVisualIndices_.size()-3);
            objectScene_.UpdatePreviewMeshGeometry(objectVisualVertices_,objectVisualIndices_);
        }
        ImGui::EndDisabled();
        if(ImGui::Button("Reset all mesh points")) {
            PushObjectUndo();objectVisualVertices_=objectOriginalVertices_;objectVisualIndices_=objectOriginalIndices_;
            objectDraft_.scale=1.f;objectSelectedVertex_=-1;objectFacePointCount_=0;
            objectScene_.UpdatePreviewMeshGeometry(objectVisualVertices_,objectVisualIndices_);
        }
    } else ImGui::TextDisabled("Editable preview mesh is limited to 200,000 source vertices.");
    ImGui::SeparatorText("Independent collision annotations");
    int purpose=static_cast<int>(objectDraft_.purpose);
    if(ImGui::Combo("Object purpose (draft)",&purpose,
        "Decoration / passable\0Solid / obstacle\0Driveable surface\0Trigger / region\0")) {
        PushObjectUndo();objectDraft_.purpose=static_cast<ObjectDraft::Purpose>(purpose);
        if(objectDraft_.purpose==ObjectDraft::Purpose::Decorative) {
            objectDraft_.boxes.clear();objectSelectedBox_=0;objectShowBoxes_=false;
        } else if(objectDraft_.boxes.empty())objectDraft_.boxes.push_back(ObjectDraft::Box{});
        objectDirty_=true;
    }
    HoverHelp("Draft intent only: native collision, with_collision flags and dirt/dust material effects are NOT exported to ProTLVK from this workspace.");
    ImGui::Checkbox("Show collision boxes",&objectShowBoxes_);
    ImGui::Text("Collision draft (%zu boxes)",objectDraft_.boxes.size());
    ImGui::Spacing();
    if(!objectVisualVertices_.empty() && !objectDraft_.boxes.empty()) {
        if(ImGui::Button("Fit selected box to mesh bounds")) {
            PushObjectUndo();auto& b=objectDraft_.boxes[static_cast<size_t>(std::clamp(objectSelectedBox_,0,static_cast<int>(objectDraft_.boxes.size())-1))];
            const auto& v=objectVisualVertices_.front();
            const auto local=LegacyTransform::ToLegacyPosition(v);
            b.min={local.x,local.y,local.z};b.max={local.x,local.y,local.z};
            for(const auto& point:objectVisualVertices_) {
                const auto p=LegacyTransform::ToLegacyPosition(point);
                b.min[0]=std::min(b.min[0],p.x);b.max[0]=std::max(b.max[0],p.x);
                b.min[1]=std::min(b.min[1],p.y);b.max[1]=std::max(b.max[1],p.y);
                b.min[2]=std::min(b.min[2],p.z);b.max[2]=std::max(b.max[2],p.z);
            }
            for(int axis=0;axis<3;++axis)if(b.max[axis]-b.min[axis]<1.f)b.max[axis]=b.min[axis]+1.f;
        }
    }
    if(ImGui::Button("Add collision box") && objectDraft_.boxes.size()<128) {
        PushObjectUndo();auto next=objectDraft_.boxes.empty()?ObjectDraft::Box{}:
            objectDraft_.boxes[static_cast<size_t>(std::clamp(objectSelectedBox_,0,static_cast<int>(objectDraft_.boxes.size())-1))];
        next.min[0]+=125.f;next.max[0]+=125.f;
        objectDraft_.boxes.push_back(next);
        if(objectDraft_.purpose==ObjectDraft::Purpose::Decorative)
            objectDraft_.purpose=ObjectDraft::Purpose::SolidDraft;
        objectSelectedBox_=static_cast<int>(objectDraft_.boxes.size())-1;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(objectDraft_.boxes.size()<=1);
    if(ImGui::Button("Remove selected")) {
        PushObjectUndo();objectDraft_.boxes.erase(objectDraft_.boxes.begin()+std::clamp(objectSelectedBox_,0,static_cast<int>(objectDraft_.boxes.size())-1));
        objectSelectedBox_=std::clamp(objectSelectedBox_,0,static_cast<int>(objectDraft_.boxes.size())-1);
    }
    ImGui::EndDisabled();
    for(size_t i=0;i<objectDraft_.boxes.size();++i) {
        ImGui::PushID(static_cast<int>(i));
        const bool selected=objectSelectedBox_==static_cast<int>(i);
        const std::string title="Box "+std::to_string(i+1)+(objectDraft_.boxes[i].role==ObjectDraft::BoxRole::Solid?" - Solid":" - Trigger");
        if(ImGui::Selectable(title.c_str(),selected))objectSelectedBox_=static_cast<int>(i);
        ImGui::PopID();
    }
    if(!objectDraft_.boxes.empty()) {
        auto& box=objectDraft_.boxes[static_cast<size_t>(std::clamp(objectSelectedBox_,0,static_cast<int>(objectDraft_.boxes.size())-1))];
        int role=box.role==ObjectDraft::BoxRole::Solid?0:1;
        if(ImGui::Combo("Draft type",&role,"Solid / impassable\0Trigger / region\0")) {
            PushObjectUndo();box.role=role==0?ObjectDraft::BoxRole::Solid:ObjectDraft::BoxRole::Trigger;
        }
        float minimum[3]={box.min[0],box.min[1],box.min[2]};
        if(ImGui::InputFloat3("Min X/Y/Z",minimum,"%.1f")) {
            PushObjectUndo();box.min={minimum[0],minimum[1],minimum[2]};
        }
        float maximum[3]={box.max[0],box.max[1],box.max[2]};
        if(ImGui::InputFloat3("Max X/Y/Z",maximum,"%.1f")) {
            PushObjectUndo();box.max={maximum[0],maximum[1],maximum[2]};
        }
        if(!ObjectDraft::ValidBox(box))ImGui::TextColored({1.f,.42f,.36f,1.f},"Invalid box bounds; fix before saving.");
    }
    ImGui::Separator();
    if(ImGui::Button("Choose output folder...",{-1,0}))objectPickerRequest_=2;
    ImGui::TextWrapped("Output: %s",objectDraftOutputRoot_.empty()?"Choose a separate writable folder":objectDraftOutputRoot_.string().c_str());
    HoverHelp("A new subfolder is created; existing drafts are never overwritten.");
    std::string validation;
    const bool outputInsideOriginal=ObjectDraft::IsWithin(objectDraftOutputRoot_,assets.Root());
    const bool valid=ObjectDraft::Validate(objectDraft_,validation) && !objectDraftOutputRoot_.empty() &&
        !outputInsideOriginal && objectSceneHasModel_;
    if(!validation.empty())ImGui::TextWrapped("%s",validation.c_str());
    else if(outputInsideOriginal)ImGui::TextWrapped("Choose an output OUTSIDE the original game library.");
    else if(!objectSceneHasModel_)ImGui::TextDisabled("Save requires a successfully imported 3D model.");
    ImGui::BeginDisabled(!valid);
    if(ImGui::Button("Save isolated object draft",{-1,34})) objectSaveRequested_=true;
    ImGui::EndDisabled();
    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##object_3d",{0,0},ImGuiChildFlags_Borders);
    if(!objectSceneInitialized_) {
        std::string err;
        objectSceneInitialized_=objectScene_.Initialize(scene.Device(),scene.Context(),err);
        if(!objectSceneInitialized_)SetMessage("Object preview init: "+err,true);
        else objectSceneNeedsBuild_=true;
    }
    if(objectSceneInitialized_ && objectSceneNeedsBuild_) {
        objectSceneNeedsBuild_=false;
        objectSceneHasModel_=false;
        objectVisualVertices_.clear();objectOriginalVertices_.clear();objectVisualIndices_.clear();objectOriginalIndices_.clear();
        objectMeshEditable_=false;objectSelectedVertex_=-1;objectVertexDragActive_=false;objectFacePointCount_=0;
        objectScene_.ReleasePreviewResources();
        if(!objectDraft_.model.empty()) {
            AssetDefinition preview;preview.name=objectDraft_.name;preview.mesh=objectDraft_.model;
            std::string err;
            objectSceneHasModel_=objectScene_.BuildAssetPreview(preview,{},err);
            if(objectSceneHasModel_) {
                objectScene_.ApplyDraftPreviewTint(objectDraft_.tint);
                try {
                    const auto imported=LegacyMeshImport::Lower(objectDraft_.model.extension().string())==".glb" ?
                        DraftMeshImport::Load(objectDraft_.model) : LegacyMeshImport::Load(objectDraft_.model);
                    objectVisualVertices_.reserve(imported.vertices.size());
                    for(const auto& v:imported.vertices)objectVisualVertices_.push_back({v.position.x,v.position.y,v.position.z});
                    objectVisualIndices_=imported.indices;
                    objectOriginalVertices_=objectVisualVertices_;
                    objectOriginalIndices_=objectVisualIndices_;
                    objectMeshEditable_=!objectVisualVertices_.empty() && objectVisualVertices_.size()<=200000 &&
                        !objectVisualIndices_.empty() && objectVisualIndices_.size()<=600000;
                    if(objectMeshEditable_) {
                        if(objectDraft_.meshVertices.size()>=objectVisualVertices_.size() &&
                           objectDraft_.meshVertices.size()<=200000) {
                            objectVisualVertices_.clear();
                            objectVisualVertices_.reserve(objectDraft_.meshVertices.size());
                            for(const auto& v:objectDraft_.meshVertices)
                                objectVisualVertices_.push_back({v[0],v[1],v[2]});
                        } else if(objectDraft_.meshVertices.empty()) {
                            for(auto& v:objectVisualVertices_) {
                                v.x*=objectDraft_.scale;v.y*=objectDraft_.scale;v.z*=objectDraft_.scale;
                            }
                        } else {
                            Log::Warning("Saved draft vertex count differs from source mesh; ignoring edit layer.");
                            objectDraft_.meshVertices.clear();objectDraft_.scale=1.f;
                        }
                        if(!objectDraft_.meshIndices.empty()) {
                            bool good=objectDraft_.meshIndices.size()<=600000 && objectDraft_.meshIndices.size()%3==0;
                            for(const auto i:objectDraft_.meshIndices)if(i>=objectVisualVertices_.size())good=false;
                            if(good)objectVisualIndices_=objectDraft_.meshIndices;
                            else {Log::Warning("Saved draft mesh faces are incompatible with source mesh; using original faces.");objectDraft_.meshIndices.clear();}
                        }
                        objectScene_.UpdatePreviewMeshGeometry(objectVisualVertices_,objectVisualIndices_);
                    }
                } catch(const std::exception& ex) {
                    Log::Warning(std::string("Optional object mesh wireframe unavailable: ")+ex.what());
                    objectVisualVertices_.clear();objectOriginalVertices_.clear();objectVisualIndices_.clear();objectOriginalIndices_.clear();
                    objectMeshEditable_=false;
                }
            }
            if(!objectSceneHasModel_)SetMessage("Cannot preview model: "+err,true);
        }
    }
    ImGui::TextUnformatted("3D model + independent collision boxes");
    ImGui::SameLine(); if(ImGui::Button("Frame model") && objectSceneInitialized_)objectScene_.FrameScene();
    // View controls are available on hover, not as a permanent instruction row.
    HoverHelp("Right drag: orbit | Middle drag: pan | Wheel: zoom | Left-click point: drag in view plane");
    if(objectSceneInitialized_) {
        const ImVec2 size={std::max(300.f,ImGui::GetContentRegionAvail().x),
                           std::max(240.f,ImGui::GetContentRegionAvail().y-14.f)};
        objectScene_.Resize(static_cast<unsigned>(size.x),static_cast<unsigned>(size.y));
        objectScene_.Render(true);
        const ImVec2 top=ImGui::GetCursorScreenPos();
        ImGui::Image(reinterpret_cast<ImTextureID>(objectScene_.Output()),size);
        const ImVec2 after=ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(top);
        ImGui::InvisibleButton("##obj_view_interaction",size,ImGuiButtonFlags_MouseButtonLeft|
            ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);
        const bool hover=ImGui::IsItemHovered();
        const auto& io=ImGui::GetIO();
        if(hover && io.MouseWheel!=0)objectScene_.Zoom(io.MouseWheel);
        if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            objectScene_.Orbit(io.MouseDelta.x,io.MouseDelta.y);
        if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            objectScene_.Pan(io.MouseDelta.x,io.MouseDelta.y);
        if(hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && objectMeshEditable_ && objectShowVertices_) {
            int candidate=-1;float best=11.f*11.f;
            const float mx=io.MousePos.x-top.x,my=io.MousePos.y-top.y;
            for(size_t i=0;i<objectVisualVertices_.size();++i) {
                float px{},py{};
                if(!objectScene_.ProjectWorld(objectVisualVertices_[i],px,py))continue;
                const float d=(mx-px)*(mx-px)+(my-py)*(my-py);
                if(d<best){best=d;candidate=static_cast<int>(i);}
            }
            objectSelectedVertex_=candidate;
            objectVertexDragActive_=candidate>=0 && !io.KeyCtrl;
            objectVertexDragHistoryCaptured_=false;
            if(candidate>=0 && io.KeyCtrl) {
                bool repeated=false;
                for(int i=0;i<objectFacePointCount_;++i)if(objectFacePoints_[i]==candidate)repeated=true;
                if(!repeated) {
                    if(objectFacePointCount_==3)objectFacePointCount_=0;
                    objectFacePoints_[objectFacePointCount_++]=candidate;
                }
            }
        }
        if(objectVertexDragActive_ && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
           ImGui::IsMouseDragging(ImGuiMouseButton_Left,1.f) && objectSelectedVertex_>=0 &&
           static_cast<size_t>(objectSelectedVertex_)<objectVisualVertices_.size()) {
            auto& vertex=objectVisualVertices_[static_cast<size_t>(objectSelectedVertex_)];
            const float mx=io.MousePos.x-top.x,my=io.MousePos.y-top.y;
            DirectX::XMFLOAT3 at{},before{};
            if(objectScene_.ScreenToWorldViewPlane(mx,my,vertex,at) &&
               objectScene_.ScreenToWorldViewPlane(mx-io.MouseDelta.x,my-io.MouseDelta.y,vertex,before)) {
                const DirectX::XMFLOAT3 moved{vertex.x+at.x-before.x,vertex.y+at.y-before.y,vertex.z+at.z-before.z};
                if(std::isfinite(moved.x)&&std::isfinite(moved.y)&&std::isfinite(moved.z) &&
                   std::abs(moved.x)<1000000.f&&std::abs(moved.y)<1000000.f&&std::abs(moved.z)<1000000.f) {
                    if(!objectVertexDragHistoryCaptured_){PushObjectUndo();objectVertexDragHistoryCaptured_=true;}
                    vertex=moved;objectScene_.UpdatePreviewMeshGeometry(objectVisualVertices_,objectVisualIndices_);
                }
            }
        }
        if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
            objectVertexDragActive_=false;objectVertexDragHistoryCaptured_=false;
        }
        if(ImGui::IsItemActive() && !objectVertexDragActive_ && objectShowBoxes_ && io.KeyShift && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !objectDraft_.boxes.empty()) {
            auto& b=objectDraft_.boxes[static_cast<size_t>(std::clamp(objectSelectedBox_,0,static_cast<int>(objectDraft_.boxes.size())-1))];
            DirectX::XMFLOAT3 current{},previous{};
            const float x=io.MousePos.x-top.x,y=io.MousePos.y-top.y,mid=(b.min[2]+b.max[2])*.5f;
            if(objectScene_.ScreenToLegacyPlane(x,y,mid,current) &&
               objectScene_.ScreenToLegacyPlane(x-io.MouseDelta.x,y-io.MouseDelta.y,mid,previous)) {
                const float dx=current.x-previous.x,dy=current.y-previous.y;
                b.min[0]+=dx;b.max[0]+=dx;b.min[1]+=dy;b.max[1]+=dy;
            }
        }
        // Model vertices are already renderer Y-up; box annotations are legacy Z-up.
        // Never pass mesh vertices through ProjectLegacy (it swaps their axes again).
        ImDrawList* dl=ImGui::GetWindowDrawList();
        dl->PushClipRect(top,{top.x+size.x,top.y+size.y},true);
        if(objectShowMeshEdges_ && objectSceneHasModel_) {
            // Source-derived face edges, not a fabricated rectangular collider.
            const size_t faces=std::min<size_t>(objectVisualIndices_.size()/3,20000);
            std::vector<ImVec2> projected(objectVisualVertices_.size());
            std::vector<uint8_t> validVertices(objectVisualVertices_.size());
            for(size_t i=0;i<objectVisualVertices_.size();++i) {
                float x{},y{};
                validVertices[i]=objectScene_.ProjectWorld(objectVisualVertices_[i],x,y)?1:0;
                projected[i]={top.x+x,top.y+y};
            }
            for(size_t face=0;face<faces;++face) {
                const uint32_t a=objectVisualIndices_[face*3],b=objectVisualIndices_[face*3+1],c=objectVisualIndices_[face*3+2];
                if(a>=projected.size()||b>=projected.size()||c>=projected.size()||
                   !validVertices[a]||!validVertices[b]||!validVertices[c])continue;
                const ImU32 meshColor=IM_COL32(108,217,188,150);
                dl->AddLine(projected[a],projected[b],meshColor,1.0f);
                dl->AddLine(projected[b],projected[c],meshColor,1.0f);
                dl->AddLine(projected[c],projected[a],meshColor,1.0f);
            }
        }
        if(objectShowVertices_ && objectSceneHasModel_ && objectMeshEditable_) {
            for(size_t i=0;i<objectVisualVertices_.size();++i) {
                float px{},py{};if(!objectScene_.ProjectWorld(objectVisualVertices_[i],px,py))continue;
                const bool selected=objectSelectedVertex_==static_cast<int>(i);
                dl->AddCircleFilled({top.x+px,top.y+py},selected?5.f:2.3f,
                    selected?IM_COL32(255,219,98,255):IM_COL32(128,234,220,210),selected?12:6);
            }
        }
        static constexpr int edges[12][2]={{0,1},{0,2},{1,3},{2,3},{4,5},{4,6},
            {5,7},{6,7},{0,4},{1,5},{2,6},{3,7}};
        if(objectShowBoxes_)for(size_t j=0;j<objectDraft_.boxes.size();++j) {
            const auto& b=objectDraft_.boxes[j];
            if(!ObjectDraft::ValidBox(b))continue;
            ImVec2 corners[8]{};bool visible[8]{};
            for(int k=0;k<8;++k) {
                const float x=(k&1)?b.max[0]:b.min[0],
                    y=(k&2)?b.max[1]:b.min[1],z=(k&4)?b.max[2]:b.min[2];
                float px{},py{};
                visible[k]=objectScene_.ProjectLegacy({x,y,z},px,py);
                corners[k]={top.x+px,top.y+py};
            }
            const bool selected=static_cast<int>(j)==objectSelectedBox_;
            const ImU32 color=b.role==ObjectDraft::BoxRole::Solid ? IM_COL32(226,178,92,225):IM_COL32(83,205,240,225);
            for(const auto& edge:edges)if(visible[edge[0]]&&visible[edge[1]])
                dl->AddLine(corners[edge[0]],corners[edge[1]],color,selected?3.f:1.5f);
        }
        dl->PopClipRect();
        ImGui::SetCursorScreenPos(after);
        if(!objectSceneHasModel_)ImGui::TextDisabled("No drawable model loaded; collision draft is still editable.");
    } else ImGui::TextDisabled("3D preview unavailable; check renderer initialization.");
    ImGui::EndChild();
    if(objectHelpRequested_) {ImGui::OpenPopup("How to make a GLB model?");objectHelpRequested_=false;}
    if(ImGui::BeginPopupModal("How to make a GLB model?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("You can make a 3D model with Meshy at https://www.meshy.ai/. Export the result as GLB, then use Import GLB in this Object Editor.");
        ImGui::TextWrapped("The editor can modify the draft mesh and save it separately. Native 3DS collision compatibility is not yet verified.");
        if(ImGui::Button("Open Meshy website"))ShellExecuteW(nullptr,L"open",L"https://www.meshy.ai/",nullptr,nullptr,SW_SHOWNORMAL);
        ImGui::SameLine();if(ImGui::Button("Close##meshy"))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if(objectSaveRequested_) {ImGui::OpenPopup("Confirm object draft save");objectSaveRequested_=false;}
    // Independent modal: predictable width and centre, not a 70px-high-column at
    // the upper edge of the docked Object Editor.
    const ImGuiViewport* draftViewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({draftViewport->WorkPos.x+draftViewport->WorkSize.x*.5f,
        draftViewport->WorkPos.y+draftViewport->WorkSize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
    ImGui::SetNextWindowSizeConstraints({340.f,0.f},{490.f,280.f});
    if(ImGui::BeginPopupModal("Confirm object draft save",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save a new isolated object draft?");
        HoverHelp("Creates a new draft folder; original library assets are not overwritten. Native 3DS/library.xml game export is not available.");
        if(ImGui::Button("Save draft")) {
            SaveObjectDraft(assets);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();if(ImGui::Button("Cancel##save_draft"))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if(objectCloseRequested_) {ImGui::OpenPopup("Save object changes?##objclose");objectCloseRequested_=false;}
    ImGui::SetNextWindowPos({draftViewport->WorkPos.x+draftViewport->WorkSize.x*.5f,
        draftViewport->WorkPos.y+draftViewport->WorkSize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
    ImGui::SetNextWindowSizeConstraints({420.f,0.f},{510.f,320.f});
    if(ImGui::BeginPopupModal("Save object changes?##objclose",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Save your changes as a new custom object draft?");
        HoverHelp("A draft is isolated. This does not replace original library assets or export a native game object.");
        ImGui::BeginDisabled(!objectDirty_);
        if(ImGui::Button("Save as new draft")) {
            if(SaveObjectDraft(assets)) {objectEditorOpen_=false;ImGui::CloseCurrentPopup();}
        }
        ImGui::EndDisabled();
        ImGui::SameLine();if(ImGui::Button("Discard changes")) {objectDirty_=false;objectEditorOpen_=false;ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();
}

void EditorUi::DrawLighting(MapDocument& map, SceneRenderer& scene) {
    ImGui::Begin("Lighting");
    if(browseLibraryOpen_){ImGui::End();return;}
    ImGui::TextDisabled("Native <lights> | %zu light(s)",map.Lights().size());
    ImGui::Checkbox("Show editable light markers",&showLights_);
    if(ImGui::Checkbox("Preview nearby light on surfaces",&previewNativeLighting_)) SaveControls();
    if(ImGui::SliderFloat("Preview reach (editor only)",&previewLightReach_,1.f,500.f,"x%.0f"))SaveControls();
    HoverHelp("Approximate editor preview only. ProTLVK remains authoritative for native lighting, skybox and shadows.");
    float tint[3]={((pendingLight_.color>>16)&255)/255.f,((pendingLight_.color>>8)&255)/255.f,(pendingLight_.color&255)/255.f};
    if(ImGui::ColorEdit3("New light color",tint)) {
        const auto toByte=[](float x){return static_cast<unsigned>(std::clamp(x,0.f,1.f)*255.f+0.5f);};
        pendingLight_.color=(toByte(tint[0])<<16)|(toByte(tint[1])<<8)|toByte(tint[2]);
    }
    ImGui::InputFloat("New intensity",&pendingLight_.intensity,0,0,"%.3f");
    float attenuation[2]={pendingLight_.attenuationBegin,pendingLight_.attenuationEnd};
    if(ImGui::InputFloat2("New fade begin/end",attenuation,"%.3f")) {
        if(attenuation[0]>=0 && attenuation[1]>attenuation[0]) {
            pendingLight_.attenuationBegin=attenuation[0];pendingLight_.attenuationEnd=attenuation[1];
        }
    }
    const bool ready=!map.Version().empty() && std::isfinite(pendingLight_.intensity) && pendingLight_.intensity>=0;
    ImGui::BeginDisabled(!ready);
    if(ImGui::Button(lightPlacementActive_?"Cancel light placement":"Add Light")) {
        lightPlacementActive_=!lightPlacementActive_;showLights_=true;
        if(lightPlacementActive_) {placementActive_=false;functionalPlacement_=FunctionalPlacement::None;scene.ClearGhost();}
    }
    ImGui::EndDisabled();
    if(lightPlacementActive_)HoverHelp("Click in the viewport to place the light at the current placement height; RMB cancels.");
    if(selected_>=0 && static_cast<size_t>(selected_)<map.Props().size()) {
        const auto& prop=map.Props()[static_cast<size_t>(selected_)];
        ImGui::TextDisabled("Selected prop: %s",prop.name.c_str());
        ImGui::BeginDisabled(!ready);
        if(ImGui::Button("Add light at selected prop")) {
            auto light=pendingLight_;light.position=prop.position;
            // Native map stores lights separately. Author can edit Z after adding.
            MapDocument before=map;const size_t index=map.AddLight(light);
            if(index<map.Lights().size()){
                history_.PushSnapshot(std::move(before),map);RequestSceneRebuild(true);
                showLights_=true;selectedLight_=static_cast<int>(index);
                SelectOnly(-1,scene);functionalSelected_=FunctionalType::Light;functionalIndex_=index;
                ImGui::SetWindowFocus("Properties");
            }
        }
        ImGui::EndDisabled();
        HoverHelp("The lamp and light remain separate native XML items.");
    }
    ImGui::SeparatorText("Existing native lights");
    for(size_t i=0;i<map.Lights().size();++i){
        const auto& light=map.Lights()[i];
        const std::string label="Light "+std::to_string(i+1)+" / "+light.type+"##light"+std::to_string(i);
        if(ImGui::Selectable(label.c_str(),functionalSelected_==FunctionalType::Light && functionalIndex_==i)) {
            selectedLight_=static_cast<int>(i);functionalSelected_=FunctionalType::Light;functionalIndex_=i;
            SelectOnly(-1,scene);showLights_=true;scene.FocusLegacyPoint(light.position,1650.f);
            ImGui::SetWindowFocus("Properties");
        }
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Color #%06X | intensity %.3f",light.color,light.intensity);
    }
    if(functionalSelected_==FunctionalType::Light && functionalIndex_<map.Lights().size()) {
        ImGui::BeginDisabled(map.Lights()[functionalIndex_].type!="omni");
        if(ImGui::Button("Duplicate selected light")) {
            auto light=map.Lights()[functionalIndex_];light.position.x+=100.f;
            MapDocument before=map;const size_t index=map.AddLight(light);
            if(index<map.Lights().size()) {
                history_.PushSnapshot(std::move(before),map);functionalIndex_=index;
                selectedLight_=static_cast<int>(index);RequestSceneRebuild(true);
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Delete selected light"))DeleteFunctional(map,scene);
    }
    HoverHelp("Native XML stores light type, RGB, intensity, attenuation and position.");
    ImGui::End();
}

void EditorUi::DrawGameplay(MapDocument& map, SceneRenderer& scene) {
    ImGui::Begin("Gameplay");
    if (browseLibraryOpen_) { ImGui::End(); return; }
    const auto& stats=map.Stats();
    ImGui::TextDisabled("%zu spawns | %zu flags | %zu control points | %zu bonuses | %zu special zones",
        stats.spawnPoints,stats.ctfFlags,stats.dominationPoints,stats.bonusRegions,stats.specialBoxes);
    if(ImGui::Button("Add gameplay element...")) gameplayPaletteOpen_=true;
    if(ImGui::TreeNode("Quick add")) {
    ImGui::BeginChild("##functional_buttons",{0.0f,0.0f},ImGuiChildFlags_AutoResizeY);
    if (ImGui::Button("Blue flag",{-1,0})) BeginFunctionalPlacement(FunctionalPlacement::BlueFlag,scene);
    if (ImGui::Button("Red flag",{-1,0})) BeginFunctionalPlacement(FunctionalPlacement::RedFlag,scene);
    if (ImGui::Button("DM spawn",{-1,0})) BeginFunctionalPlacement(FunctionalPlacement::SpawnDm,scene);
    if (ImGui::Button("Blue spawn",{-1,0})) BeginFunctionalPlacement(FunctionalPlacement::SpawnBlue,scene);
    if (ImGui::Button("Red spawn",{-1,0})) BeginFunctionalPlacement(FunctionalPlacement::SpawnRed,scene);
    if (ImGui::Button("DOM red spawn",{-1,0})) BeginFunctionalPlacement(FunctionalPlacement::SpawnDomRed,scene);
    if (ImGui::Button("DOM blue spawn",{-1,0})) BeginFunctionalPlacement(FunctionalPlacement::SpawnDomBlue,scene);
    if (ImGui::Button("DOM / control point",{-1,0})) BeginFunctionalPlacement(FunctionalPlacement::ControlPoint,scene);
    if (ImGui::Button("Bonus / drop region",{-1,0})) gameplayPaletteOpen_=true;
    if (ImGui::Button("Kill zone",{-1,0})) BeginFunctionalPlacement(FunctionalPlacement::KillZone,scene);
    if (ImGui::Button("Kick zone",{-1,0})) BeginFunctionalPlacement(FunctionalPlacement::KickZone,scene);
    ImGui::EndChild();ImGui::TreePop();}
    ImGui::SeparatorText("Game-mode view");
    // This is an editor-only filter. It never changes the gameplay sections written to XML.
    const char* modes[]={"None (hide all)","All", "DM", "TDM", "CTF", "DOM / CP / CTP"};
    ImGui::SetNextItemWidth(-1);
    int modeSelection=gameplayMode_+1;
    if(ImGui::Combo("##mode_filter",&modeSelection,modes,IM_ARRAYSIZE(modes))) gameplayMode_=modeSelection-1;
    ImGui::SeparatorText("Visibility (off by default)");
    if(ImGui::Checkbox("Show gameplay overlays",&showGameplay_) && !showGameplay_)
        showSpawns_=showFlags_=showPoints_=showBonuses_=showZones_=false;
    if (showGameplay_) {
        ImGui::Checkbox("Tank spawn markers",&showSpawns_);
        ImGui::Checkbox("CTF flag objects",&showFlags_);
        ImGui::Checkbox("Control point markers",&showPoints_);
        ImGui::Checkbox("Bonus region wireframes",&showBonuses_);
    }
    if(showGameplay_) ImGui::Checkbox("Special kill/kick volumes",&showZones_);
    if (ImGui::Button("Hide all overlays")) { showGameplay_=showSpawns_=showFlags_=showPoints_=showBonuses_=showZones_=false; gameplayMode_=-1; }
    ImGui::SeparatorText("Map elements");
    if (map.Version().empty()) { ImGui::TextDisabled("Open or create a map to inspect gameplay data."); ImGui::End(); return; }
    auto modeMatches=[&](const SpawnMarker& p) {
        const auto kind=Lower(p.type);
        if (gameplayMode_<0) return false;
        if (gameplayMode_==0) return true;
        if (gameplayMode_==1) return kind=="dm";
        if (gameplayMode_==2 || gameplayMode_==3) return kind=="red" || kind=="blue";
        return kind=="dom" || kind=="cp" || kind=="ctp";
    };
    auto focus=[&](const DirectX::XMFLOAT3& pos,float distance) { scene.FocusLegacyPoint(pos,distance); showGameplay_=true; if(gameplayMode_<0) gameplayMode_=0; };
    if (ImGui::TreeNodeEx("Tank spawns")) {
        size_t shown=0;
        for (size_t i=0;i<map.Spawns().size();++i) {
            const auto& p=map.Spawns()[i]; if (!modeMatches(p)) continue;
            ++shown;
            const std::string name="Spawn "+std::to_string(i+1)+" / "+p.type+(p.team.empty()?"":" / "+p.team)+"##spawn"+std::to_string(i);
            if (ImGui::Selectable(name.c_str(),functionalSelected_==FunctionalType::Spawn && functionalIndex_==i)) { functionalSelected_=FunctionalType::Spawn;functionalIndex_=i;SelectOnly(-1,scene);showSpawns_=true; focus(p.position,1700.0f); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Position: %.1f, %.1f, %.1f; heading Z: %.4f",p.position.x,p.position.y,p.position.z,p.rotationZ);
        }
        if (!shown) ImGui::TextDisabled("No spawns for this game-mode filter.");
        ImGui::TreePop();
    }
    if (gameplayMode_==0 || gameplayMode_==3) {
        if (ImGui::TreeNodeEx("CTF flags")) {
            if (map.CtfFlags().empty()) ImGui::TextDisabled("No CTF flag positions in this map.");
            for (size_t i=0;i<map.CtfFlags().size();++i) {
                const auto& f=map.CtfFlags()[i];
                const std::string name=f.team+" flag##flag"+std::to_string(i);
                if (ImGui::Selectable(name.c_str(),functionalSelected_==FunctionalType::Flag && functionalIndex_==i)) { functionalSelected_=FunctionalType::Flag;functionalIndex_=i;SelectOnly(-1,scene);showFlags_=true; focus(f.position,1900.0f); }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Flag position: %.1f, %.1f, %.1f",f.position.x,f.position.y,f.position.z);
            }
            ImGui::TreePop();
        }
    }
    if (gameplayMode_==0 || gameplayMode_==4) {
        if (ImGui::TreeNodeEx("DOM / Control points")) {
            if (map.ControlPoints().empty()) ImGui::TextDisabled("No control points in this map.");
            for (size_t i=0;i<map.ControlPoints().size();++i) {
                const auto& c=map.ControlPoints()[i];
                const std::string name="Point "+c.name+"##point"+std::to_string(i);
                if (ImGui::Selectable(name.c_str(),functionalSelected_==FunctionalType::Point && functionalIndex_==i)) { functionalSelected_=FunctionalType::Point;functionalIndex_=i;SelectOnly(-1,scene);showPoints_=true; focus(c.position,std::max(1700.0f,c.distance*2.0f)); }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Position %.1f, %.1f, %.1f | distance %.1f",c.position.x,c.position.y,c.position.z,c.distance);
            }
            ImGui::TreePop();
        }
    }
    if (ImGui::TreeNodeEx("Bonus regions")) {
        size_t shown=0;
        for (size_t i=0;i<map.Bonuses().size();++i) {
            const auto& b=map.Bonuses()[i];
            bool matches=gameplayMode_==0; // Empty native mode lists have unverified semantics; only show in All.
            const std::string key=gameplayMode_==1?"dm":gameplayMode_==2?"tdm":gameplayMode_==3?"ctf":"dom";
            for (const auto& m:b.modes) if (gameplayMode_>=0 && Lower(m)==key) matches=true;
            if (!matches) continue;
            ++shown;
            const std::string name=(b.name.empty()?"Bonus":b.name)+" / "+b.bonusType+(b.modes.empty()?" / modes unspecified":"")+"##bonus"+std::to_string(i);
            if (ImGui::Selectable(name.c_str(),functionalSelected_==FunctionalType::Bonus && functionalIndex_==i)) {
                functionalSelected_=FunctionalType::Bonus;functionalIndex_=i;SelectOnly(-1,scene); showBonuses_=true;
                focus({(b.min.x+b.max.x)*0.5f,(b.min.y+b.max.y)*0.5f,(b.min.z+b.max.z)*0.5f},1800.0f);
            }
            if(functionalSelected_==FunctionalType::Bonus && functionalIndex_==i) {
                ImGui::Indent();ImGui::TextDisabled("[%s]",b.bonusType.c_str());ImGui::Unindent();
            }
        }
        if (!shown) ImGui::TextDisabled("No bonus regions for this game mode.");
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Kill / kick / special zones")) {
        if (map.SpecialBoxes().empty()) ImGui::TextDisabled("No special volumes.");
        for (size_t i=0;i<map.SpecialBoxes().size();++i) {
            const auto& b=map.SpecialBoxes()[i];
            const std::string name="Zone "+std::to_string(i+1)+" / "+b.action+"##special"+std::to_string(i);
            if (ImGui::Selectable(name.c_str(),functionalSelected_==FunctionalType::Zone && functionalIndex_==i)) {
                functionalSelected_=FunctionalType::Zone;functionalIndex_=i;
                functionalPropertyBefore_.reset();zonePropertyBefore_.reset();SelectOnly(-1,scene);showZones_=true;showGameplay_=true;if(gameplayMode_<0)gameplayMode_=0;
                ImGui::SetWindowFocus("Properties");scene.FocusLegacyPoint({(b.min.x+b.max.x)*0.5f,(b.min.y+b.max.y)*0.5f,(b.min.z+b.max.z)*0.5f},2400.0f);
            }
        }
        ImGui::TreePop();
    }
    ImGui::Separator();
    ImGui::End();
    if(gameplayPaletteOpen_) {
        ImGui::SetNextWindowSize({540,680},ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Add gameplay elements##palette",&gameplayPaletteOpen_)) {
            HoverHelp("Choose an element, move the cursor into the map and press Space. Right-click cancels placement.");
            ImGui::SeparatorText("Spawns");
            if(ImGui::Button("DM spawn##palette")) BeginFunctionalPlacement(FunctionalPlacement::SpawnDm,scene);
            ImGui::SameLine(); if(ImGui::Button("Red spawn##palette")) BeginFunctionalPlacement(FunctionalPlacement::SpawnRed,scene);
            ImGui::SameLine(); if(ImGui::Button("Blue spawn##palette")) BeginFunctionalPlacement(FunctionalPlacement::SpawnBlue,scene);
            if(ImGui::Button("DOM red spawn##palette")) BeginFunctionalPlacement(FunctionalPlacement::SpawnDomRed,scene);
            ImGui::SameLine(); if(ImGui::Button("DOM blue spawn##palette")) BeginFunctionalPlacement(FunctionalPlacement::SpawnDomBlue,scene);
            ImGui::InputFloat("New spawn rotation Z (radians)",&newSpawnYaw_,0,0,"%.5f");
            HoverHelp("X: +45 degrees (8 headings); Shift+X: -45 degrees. Exact radians above.");
            ImGui::SeparatorText("Flags and objectives");
            if(ImGui::Button("Red CTF flag##palette")) BeginFunctionalPlacement(FunctionalPlacement::RedFlag,scene);
            ImGui::SameLine(); if(ImGui::Button("Blue CTF flag##palette")) BeginFunctionalPlacement(FunctionalPlacement::BlueFlag,scene);
            if(ImGui::Button("DOM / CP point##palette")) BeginFunctionalPlacement(FunctionalPlacement::ControlPoint,scene);
            ImGui::SeparatorText("Volumes");
            static constexpr const char* newBonusKinds[]={"armorup","damageup","nitro","crystal","crystal_100","medkit","crystal_500"};
            if(ImGui::Combo("Known drop content##new",&newBonusKind_,newBonusKinds,IM_ARRAYSIZE(newBonusKinds)))
                newBonusTypeOverride_.clear();
            // A map may carry more native names than the seven observed examples.
            // Offer exact reuse without claiming unverified names are game-supported.
            if(ImGui::BeginCombo("Reuse type from this map##new","Select an existing native name...")) {
                std::vector<std::string> displayed;
                for(const auto& existing:map.Bonuses()) {
                    if(existing.bonusType.empty() ||
                       std::find(displayed.begin(),displayed.end(),existing.bonusType)!=displayed.end()) continue;
                    displayed.push_back(existing.bonusType);
                    if(ImGui::Selectable(existing.bonusType.c_str()))newBonusTypeOverride_=existing.bonusType;
                }
                if(displayed.empty())ImGui::TextDisabled("No bonus regions in this map.");
                ImGui::EndCombo();
            }
            char nativeType[128]{};
            std::snprintf(nativeType,sizeof(nativeType),"%s",newBonusTypeOverride_.c_str());
            if(ImGui::InputText("Custom native bonus type##new",nativeType,sizeof(nativeType)))
                newBonusTypeOverride_=nativeType;
            const std::string pendingType=newBonusTypeOverride_.empty() ?
                GameplayAuthoring::KnownBonusTypes[std::clamp(newBonusKind_,0,6)] : newBonusTypeOverride_;
            ImGui::TextDisabled("New region type: %s",pendingType.c_str());
            HoverHelp("Listed names were observed in original maps. Existing unknown names are preserved; new names need game verification.");
            ImGui::Checkbox("Free drop##new",&newBonusFree_);
            ImGui::SameLine(); ImGui::Checkbox("Parachute##new",&newBonusParachute_);
            for(int bit=0;bit<5;++bit) {
                bool active=(newBonusModes_&(1<<bit))!=0;
                const char* labels[]={"DM##new","TDM##new","CTF##new","DOM##new","AS##new"};
                if(bit && bit!=4)ImGui::SameLine();
                if(ImGui::Checkbox(labels[bit],&active)) {
                    if(active)newBonusModes_|=(1<<bit);else newBonusModes_&=~(1<<bit);
                }
            }
            const bool validBonusType=GameplayAuthoring::ValidBonusType(pendingType);
            const bool anyBonusModes=GameplayAuthoring::HasExplicitModes(newBonusModes_);
            if(!anyBonusModes)
                HoverHelp("Select at least one game mode. Zero native game-mode entries have unverified semantics; no mode is added automatically.");
            if(!validBonusType)
                HoverHelp("Bonus type must contain 1-64 letters, digits, underscore or hyphen.");
            ImGui::BeginDisabled(!anyBonusModes || !validBonusType);
            if(ImGui::Button("Bonus / drop region##palette")) BeginFunctionalPlacement(FunctionalPlacement::BonusRegion,scene);
            ImGui::EndDisabled();
            if(ImGui::Button("Kill zone##palette")) BeginFunctionalPlacement(FunctionalPlacement::KillZone,scene);
            ImGui::SameLine(); if(ImGui::Button("Kick zone##palette")) BeginFunctionalPlacement(FunctionalPlacement::KickZone,scene);
            HoverHelp("Select an existing gameplay item to edit its native XML bounds in Properties.");
        }
        ImGui::End();
    }
}

void EditorUi::DrawFunctionalProperties(MapDocument& map,SceneRenderer& scene) {
    DirectX::XMFLOAT3 at{};
    if(!FunctionalPosition(map,at)) {
        functionalPropertyBefore_.reset();zonePropertyBefore_.reset();
        ImGui::TextDisabled("Functional object is no longer available.");return;
    }
    ImGui::TextUnformatted("Selected functional element");
    ImGui::SeparatorText("Transform / legacy XML");
    // Coalesce continuous edits into one Undo entry. Zone value changes store
    // only the small native SpecialBox, NOT a duplicate of the entire map XML.
    auto change=[&](auto&& apply) {
        if(functionalSelected_==FunctionalType::Zone && functionalIndex_<map.SpecialBoxes().size()) {
            if(!zonePropertyBefore_) {
                zonePropertyBefore_=map.SpecialBoxes()[functionalIndex_];
                zonePropertyIndex_=functionalIndex_;
            }
        } else if(!functionalPropertyBefore_) functionalPropertyBefore_=map;
        apply();RequestSceneRebuild(true);
    };
    auto finish=[&]() {
        if(!ImGui::IsItemDeactivatedAfterEdit())return;
        if(zonePropertyBefore_) {
            if(zonePropertyIndex_<map.SpecialBoxes().size())
                history_.PushZone(zonePropertyIndex_,*zonePropertyBefore_,map.SpecialBoxes()[zonePropertyIndex_]);
            zonePropertyBefore_.reset();
        } else if(functionalPropertyBefore_) {
            history_.PushSnapshot(std::move(*functionalPropertyBefore_),map);
            functionalPropertyBefore_.reset();
        }
    };
    float pos[3]={at.x,at.y,at.z};
    if(ImGui::InputFloat3("Position",pos,"%.3f")) {
        change([&]{MoveFunctional(map,{pos[0],pos[1],pos[2]});});
    }
    finish();
    if(functionalSelected_==FunctionalType::Spawn && functionalIndex_<map.Spawns().size()) {
        auto point=map.Spawns()[functionalIndex_];
        if(ImGui::InputFloat("Rotation Z (radians)",&point.rotationZ,0,0,"%.6f"))
            change([&]{map.SetSpawn(functionalIndex_,point);});
        finish();
        char type[64]{};std::snprintf(type,sizeof(type),"%s",point.type.c_str());
        if(ImGui::InputText("Spawn type",type,sizeof(type)))change([&]{point.type=type;map.SetSpawn(functionalIndex_,point);});
        finish();
        char team[64]{};std::snprintf(team,sizeof(team),"%s",point.team.c_str());
        if(ImGui::InputText("Spawn team",team,sizeof(team)))change([&]{point.team=team;map.SetSpawn(functionalIndex_,point);});
        finish();
    }
    if(functionalSelected_==FunctionalType::Point && functionalIndex_<map.ControlPoints().size()) {
        auto point=map.ControlPoints()[functionalIndex_];
        if(ImGui::InputFloat("Capture distance",&point.distance,10.0f))
            change([&]{map.SetControlPoint(functionalIndex_,point);});
        finish();
        char name[128]{};std::snprintf(name,sizeof(name),"%s",point.name.c_str());
        if(ImGui::InputText("Point name",name,sizeof(name)))change([&]{point.name=name;map.SetControlPoint(functionalIndex_,point);});
        finish();
        if(ImGui::Checkbox("Point free",&point.free))change([&]{map.SetControlPoint(functionalIndex_,point);});
        finish();
    }
if(functionalSelected_==FunctionalType::Light && functionalIndex_<map.Lights().size()) {
        auto light=map.Lights()[functionalIndex_];
        ImGui::SeparatorText("Native omni light");
        HoverHelp("Reads native lights XML. No invented glow or pulse fields are written.");
        ImGui::TextDisabled("Legacy type: %s",light.type.c_str());
        if(light.type!="omni") {
            HoverHelp("Unsupported original light type is preserved verbatim unless the native lights section is changed.");
        } else {
            float tint[3]={((light.color>>16)&255)/255.f,((light.color>>8)&255)/255.f,(light.color&255)/255.f};
            if(ImGui::ColorEdit3("Light color",tint))change([&]{
                const auto byte=[](float v){return static_cast<unsigned>(std::clamp(v,0.f,1.f)*255.f+0.5f);};
                light.color=(byte(tint[0])<<16)|(byte(tint[1])<<8)|byte(tint[2]);
                map.SetLight(functionalIndex_,light);
            });
            finish();
            if(ImGui::DragFloat("Intensity",&light.intensity,0.01f,0.f,100.f,"%.3f"))change([&]{map.SetLight(functionalIndex_,light);});
            finish();
            float fade[2]={light.attenuationBegin,light.attenuationEnd};
            if(ImGui::InputFloat2("Attenuation begin/end",fade,"%.4f")) {
                if(fade[0]>=0.f && fade[1]>fade[0]) change([&]{light.attenuationBegin=fade[0];light.attenuationEnd=fade[1];map.SetLight(functionalIndex_,light);});
            }
            finish();
            if(ImGui::InputFloat("Light rotation Z",&light.rotationZ,0,0,"%.6f"))change([&]{map.SetLight(functionalIndex_,light);});
            finish();
            HoverHelp("Attenuation uses original units; the editor marker size is symbolic.");
        }
    }
    if(functionalSelected_==FunctionalType::Bonus && functionalIndex_<map.Bonuses().size()) {
        auto region=map.Bonuses()[functionalIndex_];
        ImGui::SeparatorText("Bonus / drop region details");
        char nameBuffer[128]{};
        std::snprintf(nameBuffer,sizeof(nameBuffer),"%s",region.name.c_str());
        if(ImGui::InputText("Region name",nameBuffer,sizeof(nameBuffer)))
            change([&]{region.name=nameBuffer;map.SetBonusRegion(functionalIndex_,region);});
        finish();
        char typeBuffer[128]{};
        std::snprintf(typeBuffer,sizeof(typeBuffer),"%s",region.bonusType.c_str());
        if(ImGui::InputText("Bonus type (legacy XML)",typeBuffer,sizeof(typeBuffer)))
            change([&]{region.bonusType=typeBuffer;map.SetBonusRegion(functionalIndex_,region);});
        finish();
        HoverHelp("Examples in original fixtures: armorup, crystal_100. Other native names are preserved.");
        if(ImGui::Checkbox("Free drop",&region.free))change([&]{map.SetBonusRegion(functionalIndex_,region);});
        finish();
        if(ImGui::Checkbox("Parachute",&region.parachute))change([&]{map.SetBonusRegion(functionalIndex_,region);});
        finish();

        if(region.modes.empty())
            HoverHelp("Legacy region has no explicit game-mode nodes. Game behavior is unverified until you author a mode explicitly.");
        for(const char* mode:{"dm","tdm","ctf","dom","as"}) {
            const auto has=[&](const std::string& m){return Lower(m)==mode;};
            bool enabled=std::any_of(region.modes.begin(),region.modes.end(),has);
            const std::string label=std::string("Mode: ")+mode;
            if(ImGui::Checkbox(label.c_str(),&enabled)) {
                const bool lastMode=!enabled && region.modes.size()==1 &&
                    std::any_of(region.modes.begin(),region.modes.end(),has);
                if(lastMode) bonusModeExplanation_=true;
                else change([&]{
                    region.modes.erase(std::remove_if(region.modes.begin(),region.modes.end(),has),region.modes.end());
                    if(enabled)region.modes.push_back(mode);
                    map.SetBonusRegion(functionalIndex_,region);
                });
            }
            finish();
        }
        HoverHelp("AS occurs in native XML. Its exact game mechanics have not been inferred.");
        if(bonusModeExplanation_)
            HoverHelp("Select at least one explicit mode. Existing zero-mode records remain unchanged until you select a mode.");
        float lo[3]={region.min.x,region.min.y,region.min.z},hi[3]={region.max.x,region.max.y,region.max.z};
        if(ImGui::InputFloat3("Minimum",lo,"%.3f"))change([&]{region.min={lo[0],lo[1],lo[2]};map.SetBonusRegion(functionalIndex_,region);});
        finish();
        if(ImGui::InputFloat3("Maximum",hi,"%.3f"))change([&]{region.max={hi[0],hi[1],hi[2]};map.SetBonusRegion(functionalIndex_,region);});
        finish();
    }
    if(functionalSelected_==FunctionalType::Zone && functionalIndex_<map.SpecialBoxes().size()) {
        auto region=map.SpecialBoxes()[functionalIndex_];
        ImGui::TextDisabled("Zone %zu: drag the box edge or use the coordinates below.",functionalIndex_+1);
        float lo[3]={region.min.x,region.min.y,region.min.z},hi[3]={region.max.x,region.max.y,region.max.z};
        if(ImGui::InputFloat3("Minimum",lo,"%.3f"))change([&]{region.min={lo[0],lo[1],lo[2]};map.SetSpecialBox(functionalIndex_,region);});
        finish();
        if(ImGui::InputFloat3("Maximum",hi,"%.3f"))change([&]{region.max={hi[0],hi[1],hi[2]};map.SetSpecialBox(functionalIndex_,region);});
        finish();
        if(region.min.x>region.max.x || region.min.y>region.max.y || region.min.z>region.max.z)
            HoverHelp("Minimum must not exceed Maximum on any axis.");
        int action=region.action=="kill"?0:region.action=="kick"?1:2;
        if(ImGui::Combo("Action",&action,"Kill\0Kick\0Other (preserved)\0")) {
            if(action<2) {const auto before=region;region.action=action==0?"kill":"kick";
                map.SetSpecialBox(functionalIndex_,region);history_.PushZone(functionalIndex_,before,region);RequestSceneRebuild(true);}
        }
        if(ImGui::Checkbox("Free",&region.free)) {
            const auto before=map.SpecialBoxes()[functionalIndex_];map.SetSpecialBox(functionalIndex_,region);
            history_.PushZone(functionalIndex_,before,region);RequestSceneRebuild(true);
        }
        ImGui::Spacing();
        if(ImGui::Button("Duplicate zone")) {
            MapDocument before=map;region.legacySourceIndex=-1;
            region.min.x+=100;region.max.x+=100;
            functionalIndex_=map.AddSpecialBox(region);
            history_.PushSnapshot(std::move(before),map);RequestSceneRebuild(true);
        }
    }
    ImGui::Separator();
    if(ImGui::Button("Delete selected functional object")) {
        functionalPropertyBefore_.reset();zonePropertyBefore_.reset();DeleteFunctional(map,scene);
    }
    HoverHelp("Save writes to the original functional XML sections; Undo restores edits.");
}

void EditorUi::DrawProperties(MapDocument& map, const AssetRegistry& assets, SceneRenderer& scene) {
    ImGui::Begin("Properties");
    if (browseLibraryOpen_) { ImGui::End(); return; }
    if (selected_ < 0 || static_cast<size_t>(selected_) >= map.Props().size()) {
        if(functionalSelected_!=FunctionalType::None)DrawFunctionalProperties(map,scene);
        else ImGui::TextDisabled("No object selected");
        ImGui::End(); return;
    }
    const auto& p = map.Props()[static_cast<size_t>(selected_)];
    ImGui::TextUnformatted(p.name.c_str()); ImGui::SameLine(); ImGui::TextDisabled("%s / %s", p.library.c_str(), p.group.c_str()); ImGui::Spacing(); ImGui::SeparatorText("Transform");
    float pos[3] = {p.position.x,p.position.y,p.position.z};
    if (ImGui::InputFloat3("Position", pos, "%.3f")) { auto state=StateOf(map.Props()[static_cast<size_t>(selected_)]); state.position={pos[0],pos[1],pos[2]}; ApplyLiveTransform(map,scene,selected_,state); }
    if (ImGui::IsItemActivated()) { propertyEditActive_=true; propertyEditIndex_=selected_; propertyEditBefore_=StateOf(p); }
    if (ImGui::IsItemDeactivatedAfterEdit() && propertyEditActive_ && propertyEditIndex_==selected_) { const auto after=StateOf(map.Props()[static_cast<size_t>(selected_)]); history_.Push({static_cast<size_t>(selected_),propertyEditBefore_,after,"Edit position"}); propertyEditActive_=false; }
    float rz=map.Props()[static_cast<size_t>(selected_)].rotation.z;
    if (ImGui::InputFloat("Rotation Z",&rz,0,0,"%.6f")) { auto state=StateOf(map.Props()[static_cast<size_t>(selected_)]); state.rotation.z=rz; ApplyLiveTransform(map,scene,selected_,state); }
    if (ImGui::IsItemActivated()) { propertyEditActive_=true; propertyEditIndex_=selected_; propertyEditBefore_=StateOf(map.Props()[static_cast<size_t>(selected_)]); }
    if (ImGui::IsItemDeactivatedAfterEdit() && propertyEditActive_ && propertyEditIndex_==selected_) { const auto after=StateOf(map.Props()[static_cast<size_t>(selected_)]); history_.Push({static_cast<size_t>(selected_),propertyEditBefore_,after,"Edit rotation"}); propertyEditActive_=false; }
    HoverHelp("Native XML saves positions with three decimals and Z rotation with six decimals.");
    ImGui::Spacing(); ImGui::SeparatorText("Material");
    const auto* asset=assets.Find(p.library,p.group,p.name);
    // Collision source inspection is file I/O. Cache per selected mesh and
    // re-check modification time instead of re-parsing a 3DS every UI frame.
    static std::filesystem::path inspectedCollisionMesh;
    static std::filesystem::file_time_type inspectedCollisionTimestamp{};
    static NativeCollisionImport::Result inspectedCollisionInfo;
    if(asset && !asset->mesh.empty()) {
        std::error_code ec;
        const auto timestamp=std::filesystem::last_write_time(asset->mesh,ec);
        if(asset->mesh!=inspectedCollisionMesh || (ec ? false : timestamp!=inspectedCollisionTimestamp)) {
            inspectedCollisionMesh=asset->mesh;
            if(!ec)inspectedCollisionTimestamp=timestamp;
            inspectedCollisionInfo=NativeCollisionImport::Read(asset->mesh);
        }
    }
    if(!p.texture.empty()) {
        ImGui::TextDisabled("Texture: %s",p.texture.c_str());
        HoverHelp("Named texture variant from the original library; the map stores this variant name.");
    } else if(asset && !asset->sprite.empty()) {
        ImGui::TextDisabled("Sprite: %s",asset->sprite.filename().string().c_str());
        HoverHelp("Source sprite from the original external library.");
    } else if(asset && !asset->mesh.empty()) {
        // Material references are embedded in the 3DS, not in this prop's XML.
        // Cache the selected source file: never re-import the mesh on every frame.
        static std::filesystem::path inspectedMaterialMesh;
        static std::string inspectedMaterialName;
        if(inspectedMaterialMesh!=asset->mesh) {
            inspectedMaterialMesh=asset->mesh; inspectedMaterialName="3DS material";
            try {
                const auto imported=LegacyMeshImport::Load(asset->mesh);
                for(const auto& part:imported.parts) if(!part.diffuse.empty()) {
                    inspectedMaterialName=part.diffuse.filename().string(); break;
                }
            } catch(const std::exception&) { inspectedMaterialName="3DS material (unverified)"; }
        }
        ImGui::TextDisabled("Texture: %s",inspectedMaterialName.c_str());
        HoverHelp("Resolved from the original 3DS material; an empty map texture variant is not a fallback texture.");
    }
    ImGui::Spacing(); ImGui::SeparatorText("Collision");
    const bool hasAuthoredCollision=map.HasNativeCollisionForProp(static_cast<size_t>(selected_)) ||
        map.HasVerifiedCollisionForProp(static_cast<size_t>(selected_));
    const bool intentionalSprite=asset&&!asset->sprite.empty();
    if(hasAuthoredCollision) {
        ImGui::TextUnformatted("Collision: linked");
        HoverHelp("Original supported collision helper set is bound to this map instance. The precise game interaction depends on its surface geometry; it is not inferred from with_collision alone.");
    } else if(intentionalSprite) {
        ImGui::TextDisabled("Decoration: no physical collision");
        HoverHelp("This source is a sprite and is intentionally visual-only. For example a bush can be driven through. It is not a missing solid-wall collider.");
    } else if(asset && !asset->mesh.empty()) {
        const auto& info=inspectedCollisionInfo;
        if(info.Valid()) {
            ImGui::TextUnformatted("Collision: source helpers found (not linked)");
            HoverHelp("Source helper geometry exists, but this saved instance has not been safely matched to a complete owned collider set. No automatic repair is performed.");
        } else {
            ImGui::TextUnformatted("Collision: needs verification");
            HoverHelp(info.error.c_str());
        }
    } else {
        ImGui::TextUnformatted("Collision: no source geometry available");
        HoverHelp("No supported source mesh or native collision is available for this object.");
    }
    if(p.hasInvalidNativeMetadata) {
        ImGui::TextColored({1.f,.43f,.36f,1.f},"Native metadata: invalid (copy blocked)");
        HoverHelp("Malformed original native metadata: duplication is blocked, even when opaque XML copy is approved.");
    } else if(p.hasUncopyableMetadata) {
        ImGui::TextColored({1.f,.76f,.35f,1.f},"Unknown XML: copying needs approval");
        HoverHelp("Unknown instance-specific fields are retained. Approve one copy via Tools > Placement settings, or use the contextual button.");
        ImGui::SameLine();
        if(ImGui::SmallButton("Approve next copy##opaque_prop")) allowOpaqueMetadataCopy_=true;
        HoverHelp("Allow one copy of the unknown original XML. This does not validate its meaning in the game.");
    } else {
        ImGui::TextDisabled("Original XML: %s",p.originalPropXml?"retained":"new instance");
        HoverHelp("The original library remains external and unchanged. Original map metadata is preserved when present.");
    }
    if(ImGui::CollapsingHeader("All imported object properties")) {
        HoverHelp("Original library definition and original map instance, including unknown XML attributes and child nodes. Read-only: no game behavior is inferred.");
        ImGui::Text("Library: %s",p.library.c_str());
        ImGui::Text("Group: %s",p.group.c_str());
        ImGui::Text("Object: %s",p.name.c_str());
        ImGui::Text("Texture variant: %s",p.texture.empty()?"(3DS material / none)":p.texture.c_str());
        ImGui::Text("Position: %.3f, %.3f, %.3f",p.position.x,p.position.y,p.position.z);
        ImGui::Text("Rotation: %.6f, %.6f, %.6f",p.rotation.x,p.rotation.y,p.rotation.z);
        if(asset) {
            if(!asset->mesh.empty())ImGui::TextWrapped("3DS source: %s",asset->mesh.filename().string().c_str());
            if(!asset->sprite.empty())ImGui::TextWrapped("Sprite source: %s",asset->sprite.filename().string().c_str());
            if(!asset->originalPropXml.empty())DrawRawPropertyTree("Original library <prop> (all fields)",asset->originalPropXml);
        } else ImGui::TextDisabled("Source library definition not found; original map data remains intact.");
        if(p.originalPropXml)DrawRawPropertyTree("Original map <prop> (all fields)",*p.originalPropXml);
        else ImGui::TextDisabled("New instance: no original map XML subtree; fields above are the authored values.");
        if(ImGui::TreeNode("Bound map collision XML values")) {
            size_t linked=0;
            for(const auto& c:map.CollisionBoxes())if(c.authoredOwnerIndex==selected_) {
                ImGui::Text("Box: pos (%.3f, %.3f, %.3f), size (%.3f, %.3f, %.3f)",
                    c.position.x,c.position.y,c.position.z,c.size.x,c.size.y,c.size.z);++linked;
            }
            for(const auto& c:map.CollisionPlanes())if(c.authoredOwnerIndex==selected_) {
                ImGui::Text("Plane: pos (%.3f, %.3f, %.3f), width %.3f, length %.3f",
                    c.position.x,c.position.y,c.position.z,c.width,c.length);++linked;
            }
            for(const auto& c:map.CollisionTriangles())if(c.authoredOwnerIndex==selected_) {
                ImGui::Text("Triangle: pos (%.3f, %.3f, %.3f)",c.position.x,c.position.y,c.position.z);++linked;
            }
            if(!linked)ImGui::TextDisabled("No safely linked native collision. Original unbound XML is still preserved.");
            HoverHelp("Collision nodes live outside the object record in original map XML. Unbound primitives cannot safely be attributed to an individual prop.");
            ImGui::TreePop();
        }
        if(asset && !asset->mesh.empty()) {
            const auto& helpers=inspectedCollisionInfo;
            if(helpers.Valid()) {
                ImGui::Text("Source collision helpers: %zu planes, %zu boxes, %zu triangles; %zu occlusion nodes",
                    helpers.planes.size(),helpers.boxes.size(),helpers.triangles.size(),helpers.occlusionHelpers);
                HoverHelp("Values come from the original 3DS file. The map XML stores collision separately for each placement.");
                if(ImGui::TreeNode("Original 3DS helper values")) {
                    for(size_t i=0;i<helpers.boxes.size();++i) {
                        const auto& b=helpers.boxes[i];
                        ImGui::Text("Box %zu: center (%.3f, %.3f, %.3f), size (%.3f, %.3f, %.3f)",i+1,
                            b.offset.x,b.offset.y,b.offset.z,b.size.x,b.size.y,b.size.z);
                    }
                    for(size_t i=0;i<helpers.planes.size();++i) {
                        const auto& c=helpers.planes[i];
                        ImGui::Text("Plane %zu: center (%.3f, %.3f, %.3f), width %.3f, length %.3f",i+1,
                            c.offset.x,c.offset.y,c.offset.z,c.width,c.length);
                    }
                    for(size_t i=0;i<helpers.triangles.size();++i) {
                        const auto& t=helpers.triangles[i];
                        ImGui::Text("Triangle %zu: center (%.3f, %.3f, %.3f)",i+1,
                            t.offset.x,t.offset.y,t.offset.z);
                    }
                    ImGui::TreePop();
                }
            } else {
                ImGui::TextUnformatted("Source helper inspection incomplete");HoverHelp(helpers.error.c_str());
            }
        }
        ImGui::TextDisabled("Original map and library fields are not silently removed.");
    }
    if(VerifiedCollisionTemplates::Available(p.library,p.group,p.name) &&
       !map.HasVerifiedCollisionForProp(static_cast<size_t>(selected_))) {
        if(ImGui::Button("Repair saved wall collision")) {
            MapDocument before=map;
            if(map.AddVerifiedCollisionForProp(static_cast<size_t>(selected_))) {
                history_.PushSnapshot(std::move(before),map); RequestSceneRebuild(true);
                SetMessage("Original wall collision restored. Save a copy and test in ProTLVK.");
            } else SetMessage("Collision repair blocked: possible duplicate or shared geometry. Inspect Geometry view.",true);
        }
        HoverHelp("Repair uses the original verified 6-plane and 10-triangle template only. Save the map as a copy and test in ProTLVK.");
    }
    ImGui::Spacing(); if (ImGui::Button(ToolbarLabel("Delete object",Action::Delete).c_str())) DeleteSelected(map,scene);
    ImGui::End();
}

void EditorUi::DrawViewport(MapDocument& map, const AssetRegistry& assets, SceneRenderer& scene) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0}); ImGui::Begin("Viewport");
    if (browseLibraryOpen_) { ImGui::End(); ImGui::PopStyleVar(); return; }
    const ImVec2 origin=ImGui::GetCursorScreenPos();
    ImVec2 size=ImGui::GetContentRegionAvail(); size.x=std::max(size.x,1.0f); size.y=std::max(size.y,1.0f);
    viewportX_=origin.x; viewportY_=origin.y; viewportW_=size.x; viewportH_=size.y;
    scene.Resize(static_cast<unsigned>(size.x),static_cast<unsigned>(size.y));
    const ImVec2 mouse=ImGui::GetMousePos();
    const float lx=mouse.x-origin.x, ly=mouse.y-origin.y;
    const bool mouseInside=lx>=0 && ly>=0 && lx<size.x && ly<size.y;
    if (placementActive_ && mouseInside) UpdatePlacementGhost(scene,assets,lx,ly);
    else if (!placementActive_) scene.ClearGhost();
    if (functionalPlacement_!=FunctionalPlacement::None && mouseInside) {
        DirectX::XMFLOAT3 p{};
        functionalGhostValid_=scene.ScreenToLegacyPlane(lx,ly,placementZ_,p);
        if(functionalGhostValid_) {
            functionalGhostPosition_=functionalPasteActive_ ? DirectX::XMFLOAT3{
                GridStep::QuantizeAroundAnchor(p.x,gridSize_,functionalClipboardAnchor_.x),
                GridStep::QuantizeAroundAnchor(p.y,gridSize_,functionalClipboardAnchor_.y),placementZ_} :
                DirectX::XMFLOAT3{SnapPosition(p.x),SnapPosition(p.y),SnapPosition(placementZ_)};
            const unsigned kind=functionalPlacement_==FunctionalPlacement::RedFlag?1u:
                functionalPlacement_==FunctionalPlacement::BlueFlag?2u:
                functionalPlacement_==FunctionalPlacement::ControlPoint?3u:4u;
            scene.SetFunctionalGhost(functionalGhostPosition_,kind,true);
        } else scene.SetFunctionalGhost({},0,false);
    } else if (functionalPlacement_==FunctionalPlacement::None) scene.SetFunctionalGhost({},0,false);
    const unsigned layerMask=(showSpawns_?1u:0u)|(showFlags_?2u:0u)|(showPoints_?4u:0u)|(showBonuses_?8u:0u)|(showZones_?16u:0u);
    const unsigned modeMask=gameplayMode_<0?0u:gameplayMode_==0?15u:1u<<static_cast<unsigned>(gameplayMode_-1);
    scene.SetNativeLights(map.Lights());
    scene.SetLightPreviewScale(previewLightReach_);
    scene.Render(showGrid_ && !showCollision_,showBounds_,showGameplay_ && gameplayMode_>=0,
        showGameplay_ && showZones_ && !(functionalDragActive_ && functionalSelected_==FunctionalType::Zone),
        layerMask,modeMask,showCollision_,showLights_,previewNativeLighting_);
    if (scene.Output()) ImGui::Image((ImTextureID)scene.Output(),size);
    else { ImGui::InvisibleButton("##viewport_empty",size); ImGui::GetWindowDrawList()->AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},IM_COL32(8,10,12,255)); }

    if(showGameplay_ && showBonuses_ && gameplayMode_>=0 && !showCollision_) {
        ImDrawList* lines=ImGui::GetWindowDrawList();
        lines->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
        static constexpr int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for(size_t i=0;i<map.Bonuses().size();++i) {
            const auto& b=map.Bonuses()[i];
            if(gameplayMode_!=0) {
                static constexpr const char* modes[]={"dm","tdm","ctf","dom"};
                bool enabled=false;
                for(const auto& mode:b.modes)if(Lower(mode)==modes[gameplayMode_-1])enabled=true;
                if(!enabled)continue;
            }
            ImVec2 p[8]{};bool visible[8]{};
            for(int k=0;k<8;++k) {
                const bool right=(k==1||k==2||k==5||k==6),backSide=(k==2||k==3||k==6||k==7);
                float x{},y{};
                visible[k]=scene.ProjectLegacy({right?b.max.x:b.min.x,backSide?b.max.y:b.min.y,k>=4?b.max.z:b.min.z},x,y);
                p[k]={origin.x+x,origin.y+y};
            }
            const bool selected=functionalSelected_==FunctionalType::Bonus && functionalIndex_==i;
            const ImU32 color=selected?IM_COL32(255,223,105,255):IM_COL32(75,181,245,235);
            for(const auto& edge:edges)if(visible[edge[0]]&&visible[edge[1]])
                lines->AddLine(p[edge[0]],p[edge[1]],color,selected?3.4f:2.5f);
        }
        lines->PopClipRect();
    }
    if(showGameplay_ && showSpawns_ && functionalSelected_==FunctionalType::Spawn &&
       functionalIndex_<map.Spawns().size() && !showCollision_) {
        const auto& spawn=map.Spawns()[functionalIndex_];
        const auto from=spawn.position;
        constexpr float markerLength=260.f;
        const DirectX::XMFLOAT3 to{from.x+std::cos(spawn.rotationZ)*markerLength,
            from.y+std::sin(spawn.rotationZ)*markerLength,from.z};
        float ax{},ay{},bx{},by{};
        if(scene.ProjectLegacy(from,ax,ay) && scene.ProjectLegacy(to,bx,by)) {
            ImDrawList* draw=ImGui::GetWindowDrawList();
            draw->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
            const ImU32 color=IM_COL32(255,217,105,255);
            const ImVec2 head{origin.x+bx,origin.y+by};
            draw->AddLine({origin.x+ax,origin.y+ay},head,color,3.f);
            const float angle=std::atan2(by-ay,bx-ax);
            for(const float offset:{-0.55f,0.55f})
                draw->AddLine(head,{head.x-17.f*std::cos(angle+offset),head.y-17.f*std::sin(angle+offset)},color,2.5f);
            draw->PopClipRect();
        }
    }
    if(showLights_ && !showCollision_) {
        ImDrawList* draw=ImGui::GetWindowDrawList();
        draw->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
        for(size_t i=0;i<map.Lights().size();++i) {
            float x{},y{};
            if(!scene.ProjectLegacy(map.Lights()[i].position,x,y))continue;
            const bool active=functionalSelected_==FunctionalType::Light && functionalIndex_==i;
            const ImU32 c=active?IM_COL32(255,255,255,255):IM_COL32(255,221,107,230);
            draw->AddCircle({origin.x+x,origin.y+y},active?12.f:8.f,c,16,active?3.f:2.4f);
            draw->AddCircleFilled({origin.x+x,origin.y+y},2.5f,c);
        }
        draw->PopClipRect();
    }
    if (showCollision_) {
        const auto counts=scene.CollisionPreviewCounts();
        ImGui::GetWindowDrawList()->AddText({origin.x+12,origin.y+49},IM_COL32(255,255,255,255),
            ("GEOMETRY VIEW | planes " + std::to_string(counts[0]) + " | boxes " +
             std::to_string(counts[1]) + " | triangles " + std::to_string(counts[2]) +
             " | omitted " + std::to_string(counts[3])).c_str());
        ImGui::GetWindowDrawList()->AddText({origin.x+12,origin.y+67},IM_COL32(240,240,240,255),
            "GREEN: horizontal collider | RED: steep collider / box | not a passability guarantee");
    }
    const bool hovered=ImGui::IsItemHovered(); ImGuiIO& io=ImGui::GetIO(); auto& nav=CurrentNavigationSettings();
    // A short right click cancels selection. A right-button drag remains camera navigation.
    // Avoid acting while a placement preview is active (handled immediately below).
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
        !ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Right, 3.0f) &&
        !placementActive_ && !lightPlacementActive_ && functionalPlacement_==FunctionalPlacement::None &&
        !axConfirmRemove_) {
        functionalSelected_=FunctionalType::None;functionalDragActive_=false;
        functionalDragBefore_.reset();zoneDragBefore_.reset();
        SelectOnly(-1,scene);drag_={};dragBefore_.clear();dragIndices_.clear();
        scene.SetFunctionalGhost({},0,false);
    }
    if (functionalPlacement_!=FunctionalPlacement::None && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !axConfirmRemove_) {
        functionalPlacement_=FunctionalPlacement::None;functionalGhostValid_=false;
        scene.SetFunctionalGhost({},0,false);SetMessage("Functional placement cancelled.");
    }
    if(lightPlacementActive_ && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) lightPlacementActive_=false;
    if (placementActive_ && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !axConfirmRemove_) {
        placementActive_=false; placementCommitRequested_=false; ghostValid_=false;
        ghostProps_.clear(); scene.ClearGhost(); SetMessage("Placement cancelled.");
    }
    bool lightAddedThisFrame=false;
    if(lightPlacementActive_ && hovered && !showCollision_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        DirectX::XMFLOAT3 at{};
        if(scene.ScreenToLegacyPlane(lx,ly,placementZ_,at)) {
            auto light=pendingLight_;light.position=at;
            MapDocument before=map;
            const size_t index=map.AddLight(light);
            if(index<map.Lights().size()) {
                history_.PushSnapshot(std::move(before),map);
                functionalSelected_=FunctionalType::Light;functionalIndex_=index;
                selectedLight_=static_cast<int>(index);
                SelectOnly(-1,scene);RequestSceneRebuild(true);
                lightPlacementActive_=false;lightAddedThisFrame=true;
            }
        }
    }
    if (placementActive_ && hovered && ghostValid_ && !showCollision_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        CommitPlacement(map,assets,scene);
    if (functionalPlacement_!=FunctionalPlacement::None && hovered && functionalGhostValid_ && !showCollision_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        CommitFunctionalPlacement(map,scene);
    if (placementCommitRequested_) {
        if (hovered && ghostValid_ && !showCollision_) CommitPlacement(map,assets,scene);
        placementCommitRequested_=false;
    }
    if (functionalCommitRequested_) {
        if (hovered && functionalGhostValid_ && !showCollision_) CommitFunctionalPlacement(map,scene);
        functionalCommitRequested_=false;
    }
    if (hovered) {
        if (io.MouseWheel!=0.0f && !axTabHeld_) scene.Zoom(io.MouseWheel,nav.zoomSpeed);
        const float orbitY=nav.invertOrbitY?-io.MouseDelta.y:io.MouseDelta.y;
        // RMB has only one meaning during placement: cancel the preview, not orbit.
        if (!placementActive_ && functionalPlacement_==FunctionalPlacement::None) {
            if (navigationMode_==NavigationMode::Legacy) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right,0)) {
                    if (io.KeyShift) scene.Pan(io.MouseDelta.x*nav.panSensitivity,io.MouseDelta.y*nav.panSensitivity);
                    else scene.Orbit(io.MouseDelta.x*nav.orbitSensitivity,orbitY*nav.orbitSensitivity);
                }
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle,0)) scene.Pan(io.MouseDelta.x*nav.panSensitivity,io.MouseDelta.y*nav.panSensitivity);
            } else if (navigationMode_==NavigationMode::Adobe) {
                if (ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseDragging(ImGuiMouseButton_Left,0)) scene.Pan(io.MouseDelta.x*nav.panSensitivity,io.MouseDelta.y*nav.panSensitivity);
                else if (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left,0)) scene.Orbit(io.MouseDelta.x*nav.orbitSensitivity,orbitY*nav.orbitSensitivity);
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle,0)) scene.Pan(io.MouseDelta.x*nav.panSensitivity,io.MouseDelta.y*nav.panSensitivity);
            } else if (navigationMode_==NavigationMode::Custom) {
                if (GestureActive(customPan_,true)) scene.Pan(io.MouseDelta.x*nav.panSensitivity,io.MouseDelta.y*nav.panSensitivity);
                else if (GestureActive(customOrbit_,true)) scene.Orbit(io.MouseDelta.x*nav.orbitSensitivity,orbitY*nav.orbitSensitivity);
            } else {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right,0)) scene.Orbit(io.MouseDelta.x*nav.orbitSensitivity,orbitY*nav.orbitSensitivity);
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle,0)) scene.Pan(io.MouseDelta.x*nav.panSensitivity,io.MouseDelta.y*nav.panSensitivity);
            }
        }
        const bool adobeNav=navigationMode_==NavigationMode::Adobe && (io.KeyAlt || ImGui::IsKeyDown(ImGuiKey_Space));
        const bool customNav=navigationMode_==NavigationMode::Custom &&
            (((customPan_==MouseGesture::AltLeft || customOrbit_==MouseGesture::AltLeft) && io.KeyAlt) ||
             ((customPan_==MouseGesture::SpaceLeft || customOrbit_==MouseGesture::SpaceLeft) && ImGui::IsKeyDown(ImGuiKey_Space)));
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !showCollision_ && !adobeNav && !customNav && !placementActive_ && !lightPlacementActive_ && !lightAddedThisFrame &&
            functionalPlacement_==FunctionalPlacement::None && !axTabHeld_) {
            // Gameplay objects use the same pointer gesture as props. The closest visible
            // functional marker wins before static-geometry picking.
            bool foundFunctional=false;
            if(showLights_) {
                float distance2=18.f*18.f;
                for(size_t i=0;i<map.Lights().size();++i) {
                    float x{},y{};
                    if(!scene.ProjectLegacy(map.Lights()[i].position,x,y))continue;
                    const float d=(x-lx)*(x-lx)+(y-ly)*(y-ly);
                    if(d<distance2) {distance2=d;functionalSelected_=FunctionalType::Light;
                        functionalIndex_=i;selectedLight_=static_cast<int>(i);foundFunctional=true;}
                }
            }
            if(!foundFunctional && showGameplay_ && gameplayMode_>=0) {
                float distance2=27.0f*27.0f;
                auto test=[&](FunctionalType type,size_t index,const DirectX::XMFLOAT3& at) {
                    float x=0,y=0;
                    if(!scene.ProjectLegacy(at,x,y))return;
                    const float d=(x-lx)*(x-lx)+(y-ly)*(y-ly);
                    if(d<distance2){distance2=d;functionalSelected_=type;functionalIndex_=index;foundFunctional=true;}
                };
                if(showGameplay_ && showFlags_ && (gameplayMode_==0 || gameplayMode_==3))
                    for(size_t i=0;i<map.CtfFlags().size();++i)test(FunctionalType::Flag,i,map.CtfFlags()[i].position);
                if(showGameplay_ && showSpawns_)
                    for(size_t i=0;i<map.Spawns().size();++i) {
                        const auto& spawn=map.Spawns()[i];const auto kind=Lower(spawn.type);
                        const bool shown=gameplayMode_==0 ||
                            (gameplayMode_==1 && kind=="dm") ||
                            ((gameplayMode_==2 || gameplayMode_==3) && (kind=="red" || kind=="blue")) ||
                            (gameplayMode_==4 && (kind=="dom" || kind=="cp" || kind=="ctp"));
                        if(shown)test(FunctionalType::Spawn,i,spawn.position);
                    }
                if(showGameplay_ && showPoints_ && (gameplayMode_==0 || gameplayMode_==4))
                    for(size_t i=0;i<map.ControlPoints().size();++i)test(FunctionalType::Point,i,map.ControlPoints()[i].position);
                // Compute projected face coverage, not just a tiny center or thin wire.
                // Everything used here is taken from the saved legacy min/max bounds.
                auto testVolume=[&](FunctionalType type,size_t index,const DirectX::XMFLOAT3& lo,const DirectX::XMFLOAT3& hi) {
                    std::array<VolumePicking::Point,8> projected{};
                    std::array<bool,8> valid{};
                    for(int k=0;k<8;++k) {
                        const bool right=(k==1||k==2||k==5||k==6);
                        const bool backSide=(k==2||k==3||k==6||k==7);
                        const DirectX::XMFLOAT3 point{right?hi.x:lo.x,backSide?hi.y:lo.y,k>=4?hi.z:lo.z};
                        valid[k]=scene.ProjectLegacy(point,projected[k].x,projected[k].y);
                    }
                    const float distance=VolumePicking::Score({lx,ly},projected,valid);
                    if(distance<distance2 && distance<=20.f*20.f) {
                        distance2=distance;functionalSelected_=type;functionalIndex_=index;foundFunctional=true;
                    }
                };
                if(showGameplay_ && showBonuses_)
                    for(size_t i=0;i<map.Bonuses().size();++i) {
                        const auto& p=map.Bonuses()[i];
                        if(gameplayMode_!=0) {
                            static constexpr const char* modes[]={"dm","tdm","ctf","dom"};
                            bool active=false;
                            for(const auto& mode:p.modes)
                                if(Lower(mode)==modes[gameplayMode_-1])active=true;
                            if(!active)continue;
                        }
                        testVolume(FunctionalType::Bonus,i,p.min,p.max);test(FunctionalType::Bonus,i,{(p.min.x+p.max.x)*0.5f,(p.min.y+p.max.y)*0.5f,(p.min.z+p.max.z)*0.5f});
                        for(int k=0;k<4;++k)
                            test(FunctionalType::Bonus,i,{(k==1 || k==2)?p.max.x:p.min.x,k>=2?p.max.y:p.min.y,p.max.z});
                    }
                if(showZones_)
                    for(size_t i=0;i<map.SpecialBoxes().size();++i) {
                        const auto& p=map.SpecialBoxes()[i];
                        testVolume(FunctionalType::Zone,i,p.min,p.max);
                        test(FunctionalType::Zone,i,{(p.min.x+p.max.x)*0.5f,(p.min.y+p.max.y)*0.5f,(p.min.z+p.max.z)*0.5f});
                        const DirectX::XMFLOAT3 corners[8]={{p.min.x,p.min.y,p.min.z},{p.max.x,p.min.y,p.min.z},
                            {p.max.x,p.max.y,p.min.z},{p.min.x,p.max.y,p.min.z},
                            {p.min.x,p.min.y,p.max.z},{p.max.x,p.min.y,p.max.z},
                            {p.max.x,p.max.y,p.max.z},{p.min.x,p.max.y,p.max.z}};
                        const int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
                        for(const auto& edge:edges) {
                            float ax{},ay{},bx{},by{};
                            if(!scene.ProjectLegacy(corners[edge[0]],ax,ay) ||
                               !scene.ProjectLegacy(corners[edge[1]],bx,by))continue;
                            const float dx=bx-ax,dy=by-ay;
                            const float length2=dx*dx+dy*dy;
                            if(length2<0.01f)continue;
                            const float t=std::clamp(((lx-ax)*dx+(ly-ay)*dy)/length2,0.0f,1.0f);
                            const float d=(lx-ax-t*dx)*(lx-ax-t*dx)+(ly-ay-t*dy)*(ly-ay-t*dy);
                            if(d<distance2 && d<144.0f) {
                                distance2=d;functionalSelected_=FunctionalType::Zone;functionalIndex_=i;foundFunctional=true;
                            }
                        }
                    }
            }
            if(foundFunctional) {
                functionalPropertyBefore_.reset();zonePropertyBefore_.reset();SelectOnly(-1,scene);
                ImGui::SetWindowFocus("Properties");
                functionalResizeActive_=false;
                functionalResizeZoneBase_.reset();functionalResizeBonusBase_.reset();
                // Ctrl + upper corner handle resizes the actual native XML volume.
                // Normal LMB remains a translation. No generated collider is written.
                if(io.KeyCtrl && (functionalSelected_==FunctionalType::Zone || functionalSelected_==FunctionalType::Bonus)) {
                    DirectX::XMFLOAT3 lo{},hi{};bool valid=false;
                    if(functionalSelected_==FunctionalType::Zone && functionalIndex_<map.SpecialBoxes().size()) {
                        const auto& b=map.SpecialBoxes()[functionalIndex_];lo=b.min;hi=b.max;
                        functionalResizeZoneBase_=b;valid=true;
                    } else if(functionalSelected_==FunctionalType::Bonus && functionalIndex_<map.Bonuses().size()) {
                        const auto& b=map.Bonuses()[functionalIndex_];lo=b.min;hi=b.max;
                        functionalResizeBonusBase_=b;valid=true;
                    }
                    if(valid) {
                        float best=18.0f*18.0f;
                        for(int k=0;k<4;++k) {
                            const bool highX=(k==1 || k==2),highY=(k>=2);
                            float px{},py{};
                            if(!scene.ProjectLegacy({highX?hi.x:lo.x,highY?hi.y:lo.y,hi.z},px,py))continue;
                            const float d=(lx-px)*(lx-px)+(ly-py)*(ly-py);
                            if(d<best) {best=d;functionalResizeActive_=true;
                                functionalResizeMaxX_=highX;functionalResizeMaxY_=highY;}
                        }
                    }
                    if(!functionalResizeActive_) {functionalResizeZoneBase_.reset();functionalResizeBonusBase_.reset();}
                }
                functionalDragActive_=FunctionalPosition(map,functionalDragOriginal_); functionalDragPressedAt_=ImGui::GetTime();
                // A click is NOT a move: avoid copying the whole XML on every selection.
                functionalDragBefore_.reset();zoneDragBefore_.reset();
                if(functionalDragActive_)
                    scene.ScreenToLegacyPlane(lx,ly,functionalDragOriginal_.z,functionalDragPlaneStart_);
            } else {
            const int hit=scene.Pick(lx,ly);
            if (hit>=0) {
                if (io.KeyCtrl || io.KeyShift) {
                    const auto it=std::find(selectedItems_.begin(),selectedItems_.end(),hit);
                    if (it==selectedItems_.end()) selectedItems_.push_back(hit);
                    else if(io.KeyCtrl) selectedItems_.erase(it); // Shift never deselects.
                    selected_=selectedItems_.empty()?-1:selectedItems_.back();scene.SetSelection(selectedItems_);
                    functionalSelected_=FunctionalType::None;
                } else {
                    if (std::find(selectedItems_.begin(),selectedItems_.end(),hit)==selectedItems_.end()) SelectOnly(hit,scene);
                    if (nav.focusSelectionOnClick && selectedItems_.size()==1) scene.FocusSelectionKeepDistance(smoothCameraFocus_);
                    // LMB drag is always movement of the selected object/group, even in Select tool.
                    drag_={}; drag_.active=true; drag_.tool=tool_==ToolMode::Rotate?ToolMode::Rotate:ToolMode::Move;
                    drag_.propIndex=hit; drag_.mouseStartX=mouse.x; drag_.pressedAt=ImGui::GetTime();
                    dragIndices_=selectedItems_; dragBefore_.clear();
                    for (int index:dragIndices_) if(index>=0 && static_cast<size_t>(index)<map.Props().size()) dragBefore_.push_back(StateOf(map.Props()[static_cast<size_t>(index)]));
                    if (static_cast<size_t>(hit)<map.Props().size()) {
                        drag_.before=StateOf(map.Props()[static_cast<size_t>(hit)]);
                        scene.ScreenToLegacyPlane(lx,ly,drag_.before.position.z,drag_.planeStart);
                    }
                }
            } else {
                selectionBoxActive_=true; selectionStartX_=selectionEndX_=lx; selectionStartY_=selectionEndY_=ly;
                if (!io.KeyCtrl && !io.KeyShift) SelectOnly(-1,scene);
            }
            } // static prop selection only if no functional object was hit
        }
    }
    if(functionalDragActive_ && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
       ImGui::IsMouseDragging(ImGuiMouseButton_Left,2.0f)) {
        DirectX::XMFLOAT3 current{};
        if(scene.ScreenToLegacyPlane(lx,ly,functionalDragOriginal_.z,current)) {
            const DirectX::XMFLOAT3 at{
                functionalDragOriginal_.x+SnapDelta(current.x-functionalDragPlaneStart_.x),
                functionalDragOriginal_.y+SnapDelta(current.y-functionalDragPlaneStart_.y),
                functionalDragOriginal_.z};
            if(functionalSelected_==FunctionalType::Zone && functionalIndex_<map.SpecialBoxes().size()) {
                if(!zoneDragBefore_)zoneDragBefore_=map.SpecialBoxes()[functionalIndex_];
            } else if(!functionalDragBefore_)functionalDragBefore_=map;
            if(functionalResizeActive_) {
                const float dx=current.x-functionalDragPlaneStart_.x;
                const float dy=current.y-functionalDragPlaneStart_.y;
                if(functionalResizeZoneBase_ && functionalSelected_==FunctionalType::Zone && functionalIndex_<map.SpecialBoxes().size()) {
                    auto b=*functionalResizeZoneBase_;
                    if(functionalResizeMaxX_)b.max.x=std::max(b.min.x+1.0f,b.max.x+dx);
                    else b.min.x=std::min(b.max.x-1.0f,b.min.x+dx);
                    if(functionalResizeMaxY_)b.max.y=std::max(b.min.y+1.0f,b.max.y+dy);
                    else b.min.y=std::min(b.max.y-1.0f,b.min.y+dy);
                    map.SetSpecialBox(functionalIndex_,b);
                } else if(functionalResizeBonusBase_ && functionalSelected_==FunctionalType::Bonus && functionalIndex_<map.Bonuses().size()) {
                    auto b=*functionalResizeBonusBase_;
                    if(functionalResizeMaxX_)b.max.x=std::max(b.min.x+1.0f,b.max.x+dx);
                    else b.min.x=std::min(b.max.x-1.0f,b.min.x+dx);
                    if(functionalResizeMaxY_)b.max.y=std::max(b.min.y+1.0f,b.max.y+dy);
                    else b.min.y=std::min(b.max.y-1.0f,b.min.y+dy);
                    map.SetBonusRegion(functionalIndex_,b);
                }
            } else MoveFunctional(map,at);
            // A selected volume is represented by its actual live 3-D extent below,
            // not by a misleading circular floor ghost.
            scene.SetFunctionalGhost({},0,false);
        }
    }
    if(functionalDragActive_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        DirectX::XMFLOAT3 at{};
        // A symmetric resize preserves the center: comparing center only loses Undo.
        if(zoneDragBefore_ && functionalSelected_==FunctionalType::Zone && functionalIndex_<map.SpecialBoxes().size()) {
            const auto& now=map.SpecialBoxes()[functionalIndex_];
            const auto& was=*zoneDragBefore_;
            if(!Same3(now.min,was.min) || !Same3(now.max,was.max) || now.action!=was.action || now.free!=was.free)
                history_.PushZone(functionalIndex_,was,now);
        } else if(functionalDragBefore_) {
            if(!FunctionalPosition(map,at) || !Same3(at,functionalDragOriginal_) || functionalResizeActive_)
                history_.PushSnapshot(std::move(*functionalDragBefore_),map);
        }
        zoneDragBefore_.reset();functionalDragBefore_.reset(); functionalDragActive_=false;
        functionalResizeActive_=false;functionalResizeZoneBase_.reset();functionalResizeBonusBase_.reset();
        scene.SetFunctionalGhost({},0,false);RequestSceneRebuild(true);
    }
    if (selectionBoxActive_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        selectionEndX_=lx; selectionEndY_=ly;
    }
    if (selectionBoxActive_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        selectionBoxActive_=false;
        if (std::fabs(selectionEndX_-selectionStartX_)>=3 || std::fabs(selectionEndY_-selectionStartY_)>=3) {
            const auto found=scene.SelectInScreenRect(selectionStartX_,selectionStartY_,selectionEndX_,selectionEndY_);
            for (int index:found) if (std::find(selectedItems_.begin(),selectedItems_.end(),index)==selectedItems_.end()) selectedItems_.push_back(index);
            selected_=selectedItems_.empty()?-1:selectedItems_.back(); scene.SetSelection(selectedItems_);
            SetMessage("Selected "+std::to_string(selectedItems_.size())+" props. LMB drag, WASD or Ctrl+C.");
        }
    }
    if (drag_.active && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !dragBefore_.empty() &&
        ImGui::GetTime()-drag_.pressedAt>=0.30 && ImGui::IsMouseDragging(ImGuiMouseButton_Left,3.0f)) {
        const ImVec2 mp=ImGui::GetMousePos();
        DirectX::XMFLOAT3 delta{};
        if (drag_.tool==ToolMode::Move) {
            DirectX::XMFLOAT3 current{};
            if (scene.ScreenToLegacyPlane(mp.x-origin.x,mp.y-origin.y,drag_.before.position.z,current)) {
                delta.x=SnapDelta(current.x-drag_.planeStart.x);
                delta.y=SnapDelta(current.y-drag_.planeStart.y);
            }
        }
        const float angle=drag_.tool==ToolMode::Rotate?SnapRotation((mp.x-drag_.mouseStartX)*0.010f):0.0f;
        for (size_t k=0;k<dragBefore_.size() && k<dragIndices_.size();++k) {
            const int index=dragIndices_[k]; auto state=dragBefore_[k];
            state.position.x+=delta.x; state.position.y+=delta.y; state.rotation.z+=angle;
            if (!Same3(state.position,dragBefore_[k].position) || !Same3(state.rotation,dragBefore_[k].rotation)) drag_.moved=true;
            ApplyLiveTransform(map,scene,index,state);
        }
    }
    if (drag_.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (drag_.moved) {
            std::vector<TransformEdit> edits;
            for (size_t k=0;k<dragBefore_.size() && k<dragIndices_.size();++k) {
                const int index=dragIndices_[k]; if(index<0 || static_cast<size_t>(index)>=map.Props().size()) continue;
                edits.push_back({static_cast<size_t>(index),dragBefore_[k],StateOf(map.Props()[static_cast<size_t>(index)]),"Drag selection"});
            }
            history_.PushBatch(std::move(edits));
        }
        drag_={}; dragBefore_.clear(); dragIndices_.clear();
    }

    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && selected_>=0 && !placementActive_ && hovered) scene.FrameSelection();
    auto* dl=ImGui::GetWindowDrawList(); const auto& rs=scene.Stats(); char top[256];
    const char* toolName=tool_==ToolMode::Select?"SELECT":tool_==ToolMode::Move?"MOVE":"ROTATE";
    std::snprintf(top,sizeof(top),"%s | %zu selected | %zu props | %zu batches | %zu calls | %s",
        toolName,selectedItems_.size(),map.Props().size(),rs.meshBatches+rs.spriteBatches,rs.drawCalls,NavigationModeName());
    dl->AddText({origin.x+12,origin.y+10},IM_COL32(202,207,213,205),top);
    if (functionalPlacement_!=FunctionalPlacement::None) {
        dl->AddText({origin.x+12,origin.y+29},IM_COL32(245,184,91,230),
            "FUNCTIONAL GHOST | Space drop | RMB cancel | LMB select / drag after placing");
    } else if (placementActive_) {
        const std::string hint="GHOST "+std::to_string(placementItems_.size())+" | Space/LMB drop | X rotate | RMB cancel | Z="+std::to_string(static_cast<int>(placementZ_));
        dl->AddText({origin.x+12,origin.y+29},IM_COL32(112,190,244,230),hint.c_str());
    } else if (hovered) {
        dl->AddText({origin.x+12,origin.y+29},IM_COL32(135,143,151,185),
            "LMB select/drag | Drag empty space: box select | Ctrl+C/V copy/paste | WASD camera-relative | Q/E height");
    }
    if (selectionBoxActive_) {
        const ImVec2 a{origin.x+selectionStartX_,origin.y+selectionStartY_};
        const ImVec2 b{origin.x+selectionEndX_,origin.y+selectionEndY_};
        const ImVec2 mn{std::min(a.x,b.x),std::min(a.y,b.y)}, mx{std::max(a.x,b.x),std::max(a.y,b.y)};
        dl->AddRectFilled(mn,mx,IM_COL32(64,137,218,30)); dl->AddRect(mn,mx,IM_COL32(89,168,243,220));
    }
    // Project the ACTUAL current XML box every frame. No delayed scene rebuild,
    // stale circular proxy or guesswork while dragging a kill/kick zone.
    if(showGameplay_ && ((showZones_ && functionalSelected_==FunctionalType::Zone && functionalIndex_<map.SpecialBoxes().size()) ||
                         (showBonuses_ && functionalSelected_==FunctionalType::Bonus && functionalIndex_<map.Bonuses().size()))) {
        const DirectX::XMFLOAT3 zoneMin=functionalSelected_==FunctionalType::Zone?map.SpecialBoxes()[functionalIndex_].min:map.Bonuses()[functionalIndex_].min;
        const DirectX::XMFLOAT3 zoneMax=functionalSelected_==FunctionalType::Zone?map.SpecialBoxes()[functionalIndex_].max:map.Bonuses()[functionalIndex_].max;
        const DirectX::XMFLOAT3 corners[8]={{zoneMin.x,zoneMin.y,zoneMin.z},
            {zoneMax.x,zoneMin.y,zoneMin.z},{zoneMax.x,zoneMax.y,zoneMin.z},
            {zoneMin.x,zoneMax.y,zoneMin.z},{zoneMin.x,zoneMin.y,zoneMax.z},
            {zoneMax.x,zoneMin.y,zoneMax.z},{zoneMax.x,zoneMax.y,zoneMax.z},
            {zoneMin.x,zoneMax.y,zoneMax.z}};
        ImVec2 points[8]{};bool projected[8]{};
        for(size_t i=0;i<8;++i) {
            float px{},py{}; projected[i]=scene.ProjectLegacy(corners[i],px,py);
            points[i]={origin.x+px,origin.y+py};
        }
        auto* outline=ImGui::GetWindowDrawList();
        const ImU32 fill=IM_COL32(243,193,73,38);
        if(projected[4]&&projected[5]&&projected[6]&&projected[7])
            outline->AddQuadFilled(points[4],points[5],points[6],points[7],fill);
        const int edges[12][2]={{0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for(const auto& edge:edges)if(projected[edge[0]]&&projected[edge[1]])
            outline->AddLine(points[edge[0]],points[edge[1]],IM_COL32(255,209,69,255),2.5f);
        for(int k=4;k<8;++k)if(projected[k])
            outline->AddCircleFilled(points[k],5.5f,IM_COL32(255,239,140,255),12);
        if(projected[6])outline->AddText({points[6].x+5,points[6].y-10},IM_COL32(255,219,120,255),
            functionalResizeActive_?"Resizing volume - live XML":functionalDragActive_?"Moving volume - live XML":"Ctrl + drag a corner to resize");
    }
    // A tiny raised tab attached to the viewport's bottom-right corner. It does
    // not allocate a child/footer row or shift the 3D image or its parent dock.
    constexpr float helpWidth=218.0f, helpHeight=21.0f;
    const ImVec2 helpPos{origin.x+std::max(0.0f,size.x-helpWidth),origin.y+std::max(0.0f,size.y-helpHeight)};
    ImGui::SetCursorScreenPos(helpPos);
    if (ImGui::InvisibleButton("##discord_corner_tab",{std::min(helpWidth,size.x),helpHeight}))
        ShellExecuteW(nullptr,L"open",L"https://discord.gg/4GRZymqYG3",nullptr,nullptr,SW_SHOWNORMAL);
    const ImU32 helpColor=ImGui::IsItemHovered()?IM_COL32(30,40,51,248):IM_COL32(15,20,27,227);
    dl->AddRectFilled(helpPos,{helpPos.x+std::min(helpWidth,size.x),helpPos.y+helpHeight},helpColor);
    dl->AddText({helpPos.x+7,helpPos.y+3},IM_COL32(198,213,228,246),"Seek Help | ProTanki Discord");
    ImGui::End(); ImGui::PopStyleVar();
}

void EditorUi::DrawStatus(const MapDocument& map, const AssetRegistry& assets, const SceneRenderer& scene) {
    const auto& s=map.Stats(); const auto& r=scene.Stats(); ImGui::SetCursorPosY(ImGui::GetWindowHeight()-21); ImGui::Separator();
    if (map.Dirty()) { ImGui::Text("MODIFIED"); ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine(); }
    ImGui::TextDisabled("%zu props",s.props); ImGui::SameLine(); ImGui::TextDisabled("| %zu collision",s.collisionPlanes+s.collisionBoxes+s.collisionTriangles);
    ImGui::SameLine(); ImGui::TextDisabled("| %zu zones",s.specialBoxes); ImGui::SameLine(); ImGui::TextDisabled("| %zu flags",s.ctfFlags); ImGui::SameLine(); ImGui::TextDisabled("| %zu assets",assets.AssetCount());
    if (r.sourceProps) { ImGui::SameLine(); ImGui::TextDisabled("| %zu batches / %zu calls",r.meshBatches+r.spriteBatches,r.drawCalls); }
    ImGui::SameLine(); ImGui::TextDisabled("| %s",NavigationModeName()); ImGui::SameLine();     ImGui::SameLine(); ImGui::TextDisabled("| step %.0f",gridSize_);
    if (!message_.empty()) {
        ImGui::SameLine(); ImGui::TextDisabled("| %s",message_.substr(0,48).c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",message_.c_str());
    }
}

void EditorUi::RequestGuidedOpen(int kind) {
    if(kind<1 || kind>3)return;
    if(kind==3) {
        const auto saved=ControlsPath().parent_path()/L"external-tester-path.txt";
        std::error_code ec;
        if(testerExecutable_.empty()) {
            std::wifstream in(saved);
            std::wstring path;
            if(in && std::getline(in,path)) testerExecutable_=path;
        }
        if(std::filesystem::is_regular_file(testerExecutable_,ec) &&
           Lower(testerExecutable_.filename().string())=="protlvk32.exe")
            guidance_.RecordSuccess(GuideTarget::Tester);
    }
    if(!guidance_.NeedsExplanation(static_cast<GuideTarget>(kind-1))) {
        nativePickerRequest_=kind; return;
    }
    guideRequest_=kind; guideOpenRequested_=true; guideSkipChecked_=false;
}

void EditorUi::ProcessPendingNativeDialogs() {
    const int kind=nativePickerRequest_;
    nativePickerRequest_=0;
    if(kind>=1 && kind<=3) CompleteGuidedOpen(kind);
    const int objectKind=objectPickerRequest_;
    objectPickerRequest_=0;
    if (objectKind==4) {
        wchar_t chosen[32768]{};
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner=GetActiveWindow();ofn.lpstrFile=chosen;ofn.nMaxFile=ARRAYSIZE(chosen);
        ofn.lpstrFilter=L"Object draft manifest (*.txt)\0*.txt\0";
        ofn.lpstrTitle=L"Choose an object-draft.txt manifest";
        ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
        if(GetOpenFileNameW(&ofn)) {
            const std::filesystem::path manifest(chosen);
            if(manifest.filename()!=L"object-draft.txt")SetMessage("Select object-draft.txt in a saved draft folder.",true);
            else {
                ObjectDraft::Document loaded;std::string error;
                if(ObjectDraft::Load(manifest.parent_path(),loaded,error)) {
                    objectDraft_=std::move(loaded);
                    objectDraftOutputRoot_=manifest.parent_path().parent_path();SaveControls();
                    ResetObjectHistory();objectSelectedBox_=0;objectSelectedVertex_=-1;objectSceneNeedsBuild_=true;objectSceneHasModel_=false;
                    SetMessage("Existing draft opened. Save creates a NEW named copy, never overwrites.");
                } else SetMessage("Cannot open object draft: "+error,true);
            }
        }
    } else if (objectKind==2) {
        const auto chosen=pickFolder(GetActiveWindow(),objectDraftOutputRoot_,L"Choose a folder for your custom object drafts");
        if(!chosen.empty()) {objectDraftOutputRoot_=chosen;SaveControls();}
    } else if(objectKind==1 || objectKind==3) {
        wchar_t chosen[32768]{};
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner=GetActiveWindow();ofn.lpstrFile=chosen;ofn.nMaxFile=ARRAYSIZE(chosen);
        ofn.lpstrFilter=objectKind==1 ? L"Binary glTF (*.glb)\0*.glb\0" : L"Legacy 3DS (*.3ds)\0*.3ds\0";
        ofn.lpstrTitle=objectKind==1 ? L"Select GLB model" : L"Select an existing legacy 3DS model";
        ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
        if(GetOpenFileNameW(&ofn)) {
            const std::filesystem::path source(chosen);
            if(objectKind==1 && !ObjectDraft::ValidateGlbContainer(source)) {
                SetMessage("Invalid binary glTF 2.0 container; draft unchanged.",true);
            } else {
                objectDraft_.model=source;
                objectDraft_.meshVertices.clear();objectDraft_.meshIndices.clear();objectDraft_.scale=1.f;
                ResetObjectHistory();objectDirty_=true;objectSelectedVertex_=-1;objectFacePointCount_=0;
                const auto stem=source.stem().string();
                objectDraft_.name=stem.substr(0,127);
                objectSceneNeedsBuild_=true;
                objectSceneHasModel_=false;
            }
        }
    }
}

void EditorUi::CompleteGuidedOpen(int kind) {
    if(kind==1) {
        const auto chosen=openXml(GetActiveWindow(),lastMapDirectory_);
        // Do not mark an invalid XML as successfully loaded; App records a
        // successful map only after MapDocument::Load succeeds.
        if(!chosen.empty()) pendingMap_=chosen;
    } else if(kind==2) {
        const auto chosen=pickFolder(GetActiveWindow(),lastLibraryDirectory_);
        // Indexing may fail (invalid directory). App records success after Scan.
        if(!chosen.empty()) pendingLibrary_=chosen;
    } else if(kind==3) testRequested_=true;
}

void EditorUi::DrawFirstRunGuidance() {
    if(welcomePending_) ImGui::OpenPopup("Welcome to ProTanki Editor PRO");
    ImGui::SetNextWindowSize({620,0},ImGuiCond_Appearing);
    ImGui::SetNextWindowBgAlpha(0.78f);
    if(ImGui::BeginPopupModal("Welcome to ProTanki Editor PRO",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Quick controls (Simple navigation):"); ImGui::Separator();
        ImGui::TextUnformatted("W / A / S / D    Move the camera across the map");
        ImGui::TextUnformatted("Q / E                  Move the camera down / up");
        ImGui::TextUnformatted("Mouse wheel           Zoom in / out");
        ImGui::TextUnformatted("Left click            Select an object; drag to move");
        ImGui::TextUnformatted("Shift + left click    Add an object to the selection");
        ImGui::TextUnformatted("Right drag            Orbit camera; short right click clears selection");
        ImGui::TextUnformatted("Space                 Place the preview object");
        ImGui::TextUnformatted("Numpad 1..5           Set grid to 100..500; Numpad 0 edits grid size");
        ImGui::TextUnformatted("Tab                   Recent asset picker in Simple navigation");
        ImGui::TextUnformatted("Ctrl + Z / Ctrl + Y   Undo / redo");
        ImGui::Spacing(); ImGui::TextWrapped("You can open the complete controls manual from the menu. Geometry diagnostics are under Scene > Geometry.");
        ImGui::Checkbox("Do not show again##welcome",&welcomeSkipChecked_);
        if(ImGui::Button("I understand##welcome")) {
            welcomePending_=false;
            showBackgroundPopup_=true;
            if(welcomeSkipChecked_) SaveControls();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if(!welcomePending_ && guideOpenRequested_) {ImGui::OpenPopup("Choose original game files");guideOpenRequested_=false;}
    const ImGuiViewport* view=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({view->WorkPos.x+32.0f,view->WorkPos.y+70.0f},ImGuiCond_Appearing);
    ImGui::SetNextWindowSize({470,0},ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Choose original game files",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        const char* instruction = guideRequest_==1 ? "Select a ProTanki map (.xml) from the game's maps folder." :
            guideRequest_==2 ? "Select the original ProTanki library folder (named library)." :
            "Select the original ProTLVK32.exe application.";
        ImGui::TextWrapped("%s",instruction); ImGui::Spacing();
        ImGui::Checkbox("Do not show again##guide",&guideSkipChecked_);
        if(ImGui::Button("I understand##guide")) {
            const int kind=guideRequest_;guideRequest_=0;
            if(guideSkipChecked_) {
                guidance_.Suppress(static_cast<GuideTarget>(kind-1));SaveControls();
            }
            ImGui::CloseCurrentPopup();nativePickerRequest_=kind;
        }
        ImGui::SameLine();if(ImGui::Button("Cancel##guide")){guideRequest_=0;ImGui::CloseCurrentPopup();}
        ImGui::EndPopup();
    }
}

void EditorUi::DrawSupportPopup() {
    if (showAuthorPopup_) { ImGui::OpenPopup("Contact Author"); showAuthorPopup_=false; }
    ImGui::SetNextWindowSize({430,160},ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Contact Author",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Spacing();
        ImGui::TextDisabled("Created by Niss  (c) 2026. All rights reserved."); ImGui::Spacing();
        ImGui::TextUnformatted("Discord: .ashed"); ImGui::Spacing(); ImGui::Spacing();
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void EditorUi::DrawControlHelp() {
    if (showControlHelp_) { ImGui::OpenPopup("Controls manual"); showControlHelp_=false; }
    ImGui::SetNextWindowSize({790,650},ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Controls manual",nullptr,ImGuiWindowFlags_None)) return;
    ImGui::Text("Navigation mode: %s",NavigationModeName());
    ImGui::SameLine(); ImGui::TextDisabled("|  View, edit and configure the complete control scheme");
    if (ImGui::BeginTabBar("##manual_tabs")) {
        if (ImGui::BeginTabItem("Quick start")) {
            ImGui::BeginChild("##quick_help",{0,-42},false);
            ImGui::SeparatorText("What the editor does");
            ImGui::TextWrapped("Open a map XML and select the original external library folder. The central viewport draws the map. Scene lists placed props, Library contains available assets and a 3D preview, Properties edits the selected prop, and Gameplay inspects game-mode-specific legacy elements.");
            ImGui::SeparatorText("Selecting and editing objects");
            ImGui::BulletText("LMB selects a prop; drag it to move. Drag empty space for rectangle selection. Shift+LMB adds; Ctrl+LMB toggles.");
            ImGui::BulletText("Drag a selected object or entire selected group with LMB; releasing drops it. Rotate tool drags to rotate.");
            ImGui::BulletText("Simple/Custom: WASD moves relative to your CAMERA on the horizontal plane; Q/E moves vertically.");
            ImGui::BulletText("Arrow keys also move X/Y; PageUp/PageDown change Z in preset modes.");
            ImGui::BulletText("Simple: X rotates around legacy Z (Shift+X reverses); Properties shows radians.");
            ImGui::BulletText("Shift + a movement/rotation key uses a fine 0.1 step.");
            ImGui::BulletText("G toggles XML Geometry View; H toggles the editing grid. Numpad 1..5 sets 100..500; Numpad 0 focuses manual grid entry.");
            ImGui::BulletText("Double-click the selected prop or use Frame to frame its full bounds.");
            ImGui::BulletText("Ctrl+C copies a prop/group. Ctrl+V makes its cursor-following ghost; Space drops it; RMB cancels. Delete removes selection.");
            ImGui::SeparatorText("Library and placement");
            ImGui::BulletText("Single-click an asset to begin cursor-following 3D placement immediately.");
            ImGui::BulletText("In the preview, each LMB click rotates 45 degrees: eight clicks = 360 degrees.");
            ImGui::BulletText("RMB drags the preview freely; wheel zooms; the dropdown selects a texture variant.");
            ImGui::BulletText("Move ghost along grid, X rotates before placing; Space or LMB drops; RMB cancels. Space can stamp repeated copies.");
            ImGui::BulletText("Placement height is in Library > Settings. Tab (Simple mode) opens AX recents; wheel switches asset and Space places.");
            ImGui::SeparatorText("Game modes and editor overlays");
            ImGui::TextWrapped("Gameplay > Add gameplay element opens a persistent palette (X closes it). Create native flags, spawns, DOM points, bonus/drop regions and kill/kick zones on blank or existing maps; place with Space. Normal dragging moves an existing region; Ctrl + dragging an upper corner handle resizes its X/Y extent. Edit min/max Z, bonus type and modes in Properties. Visibility filters do not change game modes or native XML element types.");
            ImGui::SeparatorText("Fullscreen, preferences and safety");
            ImGui::TextWrapped("F11 or View > Fullscreen uses borderless fullscreen. Controls > Show shortcuts toggles the optional key labels (off by default). Custom bindings are stored in your LocalAppData GTanksNextEditor/controls.ini. The controls manual does not open automatically on application launch.");
            ImGui::EndChild(); ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Camera and mouse")) {
            ImGui::BeginChild("##camera_help",{0,-42},false);
            ImGui::SeparatorText("Navigation gestures");
            if (navigationMode_==NavigationMode::Legacy) {
                ImGui::TextWrapped("Legacy: RMB drag orbits the current target. Shift+RMB or MMB drag pans. Wheel zooms. Selecting an object does not automatically move the camera target unless you enable that option below.");
            } else if (navigationMode_==NavigationMode::Adobe) {
                ImGui::TextWrapped("Adobe-style: hold Space and drag LMB to pan, Alt+LMB to orbit, or MMB to pan. A normal LMB selects. Wheel zooms. Clicking a prop focuses the camera on its real rendered bounds when focus is enabled.");
            } else if (navigationMode_==NavigationMode::Custom) {
                ImGui::TextWrapped("Custom starts with Simple focus controls. Change the mouse gestures, sensitivity and every keyboard binding here. You can leave an action unassigned. The application detects duplicate keyboard combinations below.");
            } else {
                ImGui::TextWrapped("Simple focus: LMB selects and moves the camera target to the selected object's actual geometry bounds, RMB drags to orbit, MMB drags to pan and the wheel zooms. F frames the selected object or entire map.");
            }
            if (navigationMode_==NavigationMode::Custom) {
                const auto gestureCombo=[&](const char* name,MouseGesture& v) {
                    int value=static_cast<int>(v);
                    ImGui::SetNextItemWidth(240);
                    if (ImGui::Combo(name,&value,"Right mouse\0Middle mouse\0Alt + left mouse\0Space + left mouse\0Shift + right mouse\0")) {
                        v=static_cast<MouseGesture>(value); SaveControls();
                    }
                };
                gestureCombo("Orbit gesture",customOrbit_);
                gestureCombo("Pan gesture",customPan_);
                if (customOrbit_==customPan_) ImGui::TextColored({1.0f,0.56f,0.38f,1.0f},"Orbit and pan share a gesture: pan has priority.");
                ImGui::TextDisabled("Configured orbit: %s | pan: %s",GestureName(static_cast<int>(customOrbit_)),GestureName(static_cast<int>(customPan_)));
            }
            ImGui::Spacing(); ImGui::SeparatorText("Sensitivity (this mode)");
            auto& nav=CurrentNavigationSettings(); bool changed=false;
            changed |= ImGui::SliderFloat("Orbit sensitivity",&nav.orbitSensitivity,0.25f,2.5f,"%.2f");
            changed |= ImGui::SliderFloat("Pan sensitivity",&nav.panSensitivity,0.25f,2.5f,"%.2f");
            changed |= ImGui::SliderFloat("Zoom speed",&nav.zoomSpeed,0.25f,2.5f,"%.2f");
            changed |= ImGui::Checkbox("Invert vertical orbit",&nav.invertOrbitY);
            changed |= ImGui::Checkbox("Focus camera when selecting",&nav.focusSelectionOnClick);
            changed |= ImGui::Checkbox("Smooth camera focus",&smoothCameraFocus_);
            if (changed) SaveControls();
            ImGui::Spacing(); ImGui::SeparatorText("Camera actions");
            ImGui::BulletText("Orbit pivots around the current focus; Pan moves that focus laterally.");
            ImGui::BulletText("Wheel adjusts distance, while Frame fits the selected prop's bounds.");
            ImGui::BulletText("Click an item in Gameplay to focus its XML position without adding a prop.");
            ImGui::EndChild(); ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Keyboard bindings")) {
            ImGui::BeginChild("##keyboard_help",{0,-42},false);
            ImGui::TextWrapped("All editor actions are listed below. Preset modes show their shortcuts; switch to Custom to change keys and Ctrl/Shift/Alt modifiers. Plain movement bindings also accept Shift for fine steps.");
            if (navigationMode_==NavigationMode::Custom) {
                if (ImGui::Button("Reset Custom to Simple defaults")) { ResetCustomBindings(); SaveControls(); }
                ImGui::SameLine(); ImGui::TextDisabled("Changes save immediately.");
            }
            if (ImGui::BeginTable("##bindings",5,ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Action",ImGuiTableColumnFlags_WidthStretch,2.7f);
                ImGui::TableSetupColumn("Key",ImGuiTableColumnFlags_WidthStretch,1.35f);
                ImGui::TableSetupColumn("Ctrl",ImGuiTableColumnFlags_WidthFixed,44.0f);
                ImGui::TableSetupColumn("Shift",ImGuiTableColumnFlags_WidthFixed,48.0f);
                ImGui::TableSetupColumn("Alt",ImGuiTableColumnFlags_WidthFixed,40.0f);
                ImGui::TableHeadersRow();
                static constexpr ImGuiKey choices[]={
                    ImGuiKey_None,ImGuiKey_A,ImGuiKey_B,ImGuiKey_C,ImGuiKey_D,ImGuiKey_E,ImGuiKey_F,
                    ImGuiKey_G,ImGuiKey_H,ImGuiKey_I,ImGuiKey_J,ImGuiKey_K,ImGuiKey_L,ImGuiKey_M,
                    ImGuiKey_N,ImGuiKey_O,ImGuiKey_P,ImGuiKey_Q,ImGuiKey_R,ImGuiKey_S,ImGuiKey_T,
                    ImGuiKey_U,ImGuiKey_V,ImGuiKey_W,ImGuiKey_X,ImGuiKey_Y,ImGuiKey_Z,
                    ImGuiKey_0,ImGuiKey_1,ImGuiKey_2,ImGuiKey_3,ImGuiKey_4,ImGuiKey_5,
                    ImGuiKey_6,ImGuiKey_7,ImGuiKey_8,ImGuiKey_9,
                    ImGuiKey_Enter,ImGuiKey_Escape,ImGuiKey_Delete,ImGuiKey_Backspace,ImGuiKey_Space,
                    ImGuiKey_LeftArrow,ImGuiKey_RightArrow,ImGuiKey_UpArrow,ImGuiKey_DownArrow,
                    ImGuiKey_PageUp,ImGuiKey_PageDown,ImGuiKey_Home,ImGuiKey_End,
                    ImGuiKey_F1,ImGuiKey_F2,ImGuiKey_F3,ImGuiKey_F4,ImGuiKey_F5,ImGuiKey_F6,
                    ImGuiKey_F7,ImGuiKey_F8,ImGuiKey_F9,ImGuiKey_F10,ImGuiKey_F11,ImGuiKey_F12
                };
                for (size_t i=0;i<static_cast<size_t>(Action::Count);++i) {
                    const Action action=static_cast<Action>(i);
                    ImGui::PushID(static_cast<int>(i)); ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(ActionName(action));
                    ImGui::TableSetColumnIndex(1);
                    KeyBinding& custom=customKeys_[i]; const auto active=Binding(action);
                    const char* keyName=active.key?ImGui::GetKeyName(static_cast<ImGuiKey>(active.key)):"Unassigned";
                    if (!keyName || !*keyName) keyName="Unassigned";
                    if (navigationMode_==NavigationMode::Custom) {
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::BeginCombo("##key",keyName)) {
                            for (const auto k:choices) {
                                const char* name=k==ImGuiKey_None?"Unassigned":ImGui::GetKeyName(k);
                                if (ImGui::Selectable(name,custom.key==static_cast<int>(k))) { custom.key=k; SaveControls(); }
                            }
                            ImGui::EndCombo();
                        }
                    } else ImGui::TextDisabled("%s",keyName);
                    ImGui::TableSetColumnIndex(2);
                    if (navigationMode_==NavigationMode::Custom) { if (ImGui::Checkbox("##ctrl",&custom.ctrl)) SaveControls(); }
                    else ImGui::TextUnformatted(active.ctrl?"Yes":"-");
                    ImGui::TableSetColumnIndex(3);
                    if (navigationMode_==NavigationMode::Custom) { if (ImGui::Checkbox("##shift",&custom.shift)) SaveControls(); }
                    else ImGui::TextUnformatted(active.shift?"Yes":"-");
                    ImGui::TableSetColumnIndex(4);
                    if (navigationMode_==NavigationMode::Custom) { if (ImGui::Checkbox("##alt",&custom.alt)) SaveControls(); }
                    else ImGui::TextUnformatted(active.alt?"Yes":"-");
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (navigationMode_==NavigationMode::Custom) {
                int conflicts=0;
                for(size_t i=0;i<customKeys_.size();++i) for(size_t j=i+1;j<customKeys_.size();++j) {
                    const auto a=customKeys_[i],b=customKeys_[j];
                    if (a.key && a.key==b.key && a.ctrl==b.ctrl && a.shift==b.shift && a.alt==b.alt) ++conflicts;
                }
                if (conflicts) ImGui::TextColored({1.0f,0.55f,0.3f,1.0f},"Warning: %d overlapping key combinations. Rebind one of each pair.",conflicts);
            }
            ImGui::EndChild(); ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    if (ImGui::Button("Close",{120,0})) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorUi::DrawToast() {
    if (message_.empty()) return;
    ImGuiViewport* vp=ImGui::GetMainViewport(); ImGui::SetNextWindowPos({vp->WorkPos.x+vp->WorkSize.x-342,vp->WorkPos.y+vp->WorkSize.y-76},ImGuiCond_Always); ImGui::SetNextWindowSize({322,56}); ImGui::SetNextWindowBgAlpha(0.90f);
    ImGui::Begin("##toast",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNav);
    const ImU32 bar=messageError_?IM_COL32(207,78,78,230):IM_COL32(83,148,211,220); const ImVec2 mn=ImGui::GetWindowPos(),mx={mn.x+3,mn.y+ImGui::GetWindowHeight()};
    ImGui::GetWindowDrawList()->AddRectFilled(mn,mx,bar,1.0f); ImGui::SetCursorPosX(ImGui::GetCursorPosX()+6); ImGui::TextWrapped("%s",message_.c_str());
    if (ImGui::IsWindowHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)) message_.clear(); ImGui::End();
}


void EditorUi::CaptureRecentThumbnail(SceneRenderer& previewScene) {
    if (selectedAsset_<0 || recentAssets_.empty() || !previewScene.Output()) return;
    auto found=std::find_if(recentAssets_.begin(),recentAssets_.end(),[&](const RecentAsset& item){return item.index==static_cast<size_t>(selectedAsset_);});
    if (found==recentAssets_.end() || found->thumbnail) return;
    Microsoft::WRL::ComPtr<ID3D11Resource> raw;
    previewScene.Output()->GetResource(raw.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> original;
    if (!raw || FAILED(raw.As(&original))) return;
    D3D11_TEXTURE2D_DESC desc{}; original->GetDesc(&desc);
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags=0; desc.MiscFlags=0; desc.Usage=D3D11_USAGE_DEFAULT;
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    original->GetDevice(device.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> copy;
    if (!device || FAILED(device->CreateTexture2D(&desc,nullptr,copy.GetAddressOf()))) return;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.GetAddressOf());
    if (!context) return;
    context->CopyResource(copy.Get(),original.Get());
    device->CreateShaderResourceView(copy.Get(),nullptr,found->thumbnail.GetAddressOf());
}

void EditorUi::ActivateRecent(size_t recentPosition, const AssetRegistry& assets, SceneRenderer& previewScene) {
    if (recentPosition>=recentAssets_.size()) return;
    const size_t asset=recentAssets_[recentPosition].index;
    if (asset>=assets.Assets().size()) return;
    selectedAsset_=static_cast<int>(asset); selectedTextureVariant_=recentAssets_[recentPosition].textureVariant;
    RebuildAssetPreview(assets,previewScene);
    BeginPlacement(assets);
}

void EditorUi::DrawAxLibrary(const MapDocument& map, const AssetRegistry& assets, SceneRenderer& scene, SceneRenderer& previewScene) {
    const bool targetVisible=axPinned_ || axTabHeld_;
    const float speed=std::clamp(ImGui::GetIO().DeltaTime*9.0f,0.0f,1.0f);
    axReveal_ += ((targetVisible?1.0f:0.0f)-axReveal_)*speed;
    if (!targetVisible && axReveal_<0.015f) {axReveal_=0.0f;return;}
    ImGuiViewport* vp=ImGui::GetMainViewport();
    const float width=330.0f;
    ImGui::SetNextWindowPos({vp->WorkPos.x+vp->WorkSize.x-width*axReveal_,vp->WorkPos.y});
    ImGui::SetNextWindowSize({width,vp->WorkSize.y});
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoBackground|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
    const ImGuiWindowFlags activeFlags=targetVisible?flags:(flags|ImGuiWindowFlags_NoInputs);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{22,12});
    ImGui::Begin("AX Library##overlay",nullptr,activeFlags);
    const ImVec2 pos=ImGui::GetWindowPos(), sz=ImGui::GetWindowSize();
    auto* dl=ImGui::GetWindowDrawList();
    const int opacity=static_cast<int>(248.0f*axReveal_);
    dl->AddRectFilled({pos.x+22,pos.y},{pos.x+sz.x,pos.y+sz.y},IM_COL32(6,8,11,opacity));
    dl->AddRectFilledMultiColor({pos.x,pos.y},{pos.x+22,pos.y+sz.y},
        IM_COL32(6,8,11,0),IM_COL32(6,8,11,opacity),IM_COL32(6,8,11,opacity),IM_COL32(6,8,11,0));
    if (axPinned_ && !axTabHeld_) {
        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-35.0f);
        if (ImGui::SmallButton("X##close_ax")) axPinned_=false;
    } else ImGui::Dummy({0,20});
    std::vector<size_t> visible;
    for (size_t i=0;i<recentAssets_.size();++i) {
        const auto index=recentAssets_[i].index;
        if (index>=assets.Assets().size()) continue;
        const auto& asset=assets.Assets()[index];
        if (axOnlyUsed_ && std::none_of(map.Props().begin(),map.Props().end(),[&](const PropInstance& p){
            return p.library==asset.library && p.group==asset.group && p.name==asset.name;
        })) continue;
        visible.push_back(i);
    }
    if (!visible.empty()) {
        if (axCurrent_<0 || axCurrent_>=static_cast<int>(visible.size())) axCurrent_=0;
        const bool scroll=axTabHeld_;
        if (scroll && ImGui::GetIO().MouseWheel!=0.0f) {
            axCurrent_=(axCurrent_+(ImGui::GetIO().MouseWheel<0?1:static_cast<int>(visible.size())-1))%static_cast<int>(visible.size());
            const int currentRow=axCurrent_;
            ActivateRecent(visible[static_cast<size_t>(currentRow)],assets,previewScene);
            axCurrent_=currentRow;
        }
    }
    if (ImGui::BeginChild("##axlist",{0,-(axConfirmRemove_?67.0f:8.0f)},false,ImGuiWindowFlags_NoScrollWithMouse)) {
        for (size_t row=0;row<visible.size();++row) {
            const size_t at=visible[row]; const auto& recent=recentAssets_[at];
            const auto& asset=assets.Assets()[recent.index];
            ImGui::PushID(static_cast<int>(at));
            const bool active=static_cast<int>(row)==axCurrent_;
            if (recent.thumbnail) {
                ImGui::Image((ImTextureID)recent.thumbnail.Get(),{76,58});
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) { axRemovalIndex_=static_cast<int>(at); axConfirmRemove_=true; }
                ImGui::SameLine();
            }
            if (ImGui::Selectable((asset.name+"##axitem").c_str(),active,0,{0,58})) {
                axRemovalIndex_=static_cast<int>(at); axConfirmRemove_=true;
            }
            ImGui::PopID();
        }
        if (visible.empty()) ImGui::TextDisabled("No recent objects yet. Select an asset in Library.");
    }
    ImGui::EndChild();
    if (axConfirmRemove_ && axRemovalIndex_>=0 && axRemovalIndex_<static_cast<int>(recentAssets_.size())) {
        ImGui::Separator();
        ImGui::TextWrapped("Remove this entry from history? RMB confirms.");
        if (ImGui::Button("Cancel")) { axConfirmRemove_=false; axRemovalIndex_=-1; }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            recentAssets_.erase(recentAssets_.begin()+axRemovalIndex_);
            axConfirmRemove_=false; axRemovalIndex_=-1; axCurrent_=0;
        }
    }
    if (axPinned_ && !axTabHeld_ && ImGui::GetTime()-axPinnedOpenedAt_>0.12 &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) axPinned_=false;
    ImGui::End(); ImGui::PopStyleVar();
    (void)scene;
}
