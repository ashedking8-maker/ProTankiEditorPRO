#pragma once
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

// Supplied examples establish these native XML identifiers, not an exhaustive
// catalog for every legacy game build. Other tokens in opened maps are preserved.
namespace GameplayAuthoring {
inline constexpr std::array<const char*,7> KnownBonusTypes{
    "armorup", "damageup", "nitro", "crystal", "crystal_100", "medkit", "crystal_500"
};
inline constexpr std::array<const char*,5> Modes{"dm", "tdm", "ctf", "dom", "as"};
inline constexpr float SpawnTurnRadians = 0.78539816339744830962f; // 45 degrees
inline float RotateSpawn(float yaw, bool reverse) {
    constexpr float fullTurn=6.2831853071795864769f;
    return std::remainder(yaw+(reverse?-SpawnTurnRadians:SpawnTurnRadians),fullTurn);
}
inline bool ValidBonusType(std::string_view type) {
    if(type.empty()||type.size()>64)return false;
    for(const unsigned char c:type)
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
    return true;
}
inline bool HasExplicitModes(int mask) {return (mask&31)!=0;}
inline std::vector<std::string> SelectedModes(int mask) {
    std::vector<std::string> result;
    for(int bit=0;bit<5;++bit)if(mask&(1<<bit))result.emplace_back(Modes[bit]);
    return result;
}
} // namespace GameplayAuthoring
