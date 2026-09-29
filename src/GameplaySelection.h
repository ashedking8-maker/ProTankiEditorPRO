#pragma once
#include "MapDocument.h"
#include <algorithm>
#include <cctype>
#include <limits>
#include <vector>
namespace GameplaySelection {
enum class Kind { None, Flag, Spawn, Point, Bonus, Zone, Light };
struct Item {Kind kind{Kind::None};size_t index{};bool operator==(const Item&)const=default;};
struct Visibility {bool gameplay{},flags{},spawns{},points{},bonuses{},zones{},lights{};int mode{-1};};
struct Candidate {Item item;DirectX::XMFLOAT3 min{},max{};};
inline std::string Lower(std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
inline bool SpawnVisible(int mode,const std::string& type){const auto s=Lower(type);return mode==0||(mode==1&&s=="dm")||((mode==2||mode==3)&&(s=="red"||s=="blue"))||(mode==4&&(s=="dom"||s=="cp"||s=="ctp"));}
inline bool BonusVisible(int mode,const std::vector<std::string>& modes){
    if(mode==0)return true;
    if(mode<1||mode>4)return false;
    constexpr const char* names[]={"dm","tdm","ctf","dom"};
    return std::any_of(modes.begin(),modes.end(),[&](const auto& m){return Lower(m)==names[mode-1];});
}
inline std::vector<Candidate> Visible(const MapDocument& map,Visibility v){
    std::vector<Candidate> result;
    auto point=[&](Kind kind,size_t i,DirectX::XMFLOAT3 p){result.push_back({{kind,i},p,p});};
    if(v.lights)for(size_t i=0;i<map.Lights().size();++i)point(Kind::Light,i,map.Lights()[i].position);
    if(!v.gameplay||v.mode<0)return result;
    if(v.flags&&(v.mode==0||v.mode==3))for(size_t i=0;i<map.CtfFlags().size();++i)point(Kind::Flag,i,map.CtfFlags()[i].position);
    if(v.spawns)for(size_t i=0;i<map.Spawns().size();++i)if(SpawnVisible(v.mode,map.Spawns()[i].type))point(Kind::Spawn,i,map.Spawns()[i].position);
    if(v.points&&(v.mode==0||v.mode==4))for(size_t i=0;i<map.ControlPoints().size();++i)point(Kind::Point,i,map.ControlPoints()[i].position);
    if(v.bonuses)for(size_t i=0;i<map.Bonuses().size();++i){const auto& b=map.Bonuses()[i];if(BonusVisible(v.mode,b.modes))result.push_back({{Kind::Bonus,i},b.min,b.max});}
    if(v.zones)for(size_t i=0;i<map.SpecialBoxes().size();++i){const auto& b=map.SpecialBoxes()[i];result.push_back({{Kind::Zone,i},b.min,b.max});}
    return result;
}
inline void Add(std::vector<Item>& items,Item value,bool toggle=false){
    auto it=std::find(items.begin(),items.end(),value);
    if(it==items.end())items.push_back(value);else if(toggle)items.erase(it);
}
// Same conservative projected-bound intersection used by static prop selection.
template<class Project> bool Intersects(const Candidate& c,float x0,float y0,float x1,float y1,Project project){
    float left=std::numeric_limits<float>::max(),top=left,right=-left,bottom=-left;bool visible=false;
    for(int k=0;k<8;++k){float x{},y{};DirectX::XMFLOAT3 p{(k&1)?c.max.x:c.min.x,(k&2)?c.max.y:c.min.y,(k&4)?c.max.z:c.min.z};
        if(project(p,x,y)){visible=true;left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}}
    return visible&&left<=std::max(x0,x1)&&right>=std::min(x0,x1)&&top<=std::max(y0,y1)&&bottom>=std::min(y0,y1);
}
inline bool Delete(MapDocument& map,std::vector<Item> items){
    std::sort(items.begin(),items.end(),[](Item a,Item b){return a.kind!=b.kind?a.kind>b.kind:a.index>b.index;});
    items.erase(std::unique(items.begin(),items.end()),items.end());
    for(auto i:items){bool ok=false;switch(i.kind){
    case Kind::Flag:ok=map.DeleteFlag(i.index);break;case Kind::Spawn:ok=map.DeleteSpawn(i.index);break;
    case Kind::Point:ok=map.DeleteControlPoint(i.index);break;case Kind::Bonus:ok=map.DeleteBonusRegion(i.index);break;
    case Kind::Zone:ok=map.DeleteSpecialBox(i.index);break;case Kind::Light:ok=map.DeleteLight(i.index);break;default:break;
    }if(!ok)return false;}return true;
}
}
