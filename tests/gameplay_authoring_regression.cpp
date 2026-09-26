#include "GameplayAuthoring.h"
#include <cmath>
#include <iostream>
int main() {
    using namespace GameplayAuthoring;
    if(KnownBonusTypes.size()!=7||std::string(KnownBonusTypes[1])!="damageup"||
       std::string(KnownBonusTypes[2])!="nitro")return 1;
    if(!ValidBonusType("crystal_100")||!ValidBonusType("native-variant")||
       ValidBonusType("")||ValidBonusType("bad token")||ValidBonusType("<xml>"))return 2;
    if(HasExplicitModes(0)||!SelectedModes(0).empty())return 3;
    const auto selected=SelectedModes((1<<1)|(1<<2));
    if(selected.size()!=2||selected[0]!="tdm"||selected[1]!="ctf"||
       !HasExplicitModes(6))return 4;
    if(std::string(KnownBonusTypes[6])!="crystal_500" || Modes[4]!=std::string("as") ||
       !HasExplicitModes(16) || SelectedModes(16)!=std::vector<std::string>{"as"})return 8;
    float yaw=0;
    for(int i=0;i<8;++i) {
        yaw=RotateSpawn(yaw,false);
        if(i==0&&std::abs(yaw-SpawnTurnRadians)>0.0001f)return 5;
    }
    if(std::abs(yaw)>0.0001f)return 6;
    if(std::abs(RotateSpawn(RotateSpawn(0,false),true))>0.0001f)return 7;
    std::cout<<"PASS: 8 headings, reverse rotation, explicit modes, and native bonus types\n";
    return 0;
}
