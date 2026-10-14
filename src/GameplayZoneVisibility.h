#pragma once
#include <cctype>
#include <string>
namespace GameplayZoneVisibility {
inline unsigned Layer(std::string action) {
    for(auto& c:action)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return action=="kill"?16u:action=="kick"?64u:80u;
}
inline bool LayerVisible(unsigned layer,unsigned mask) {
    return layer==80u?(mask&80u)==80u:(mask&layer)!=0;
}
inline bool Visible(const std::string& action,bool kill,bool kick) {
    return LayerVisible(Layer(action),(kill?16u:0u)|(kick?64u:0u));
}
}
