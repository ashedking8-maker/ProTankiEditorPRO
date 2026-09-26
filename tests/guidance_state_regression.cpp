#include "GuidanceState.h"
#include <iostream>

int main() {
    auto require=[](bool okay,const char* why) {
        if(!okay) std::cerr<<"FAIL guidance: "<<why<<'\n';
        return okay;
    };
    GuidanceState state;
    if(!require(state.NeedsExplanation(GuideTarget::Map) &&
                state.NeedsExplanation(GuideTarget::Library) &&
                state.NeedsExplanation(GuideTarget::Tester),"all three guide types initially independent"))return 1;
    // A first successful map open must not hide library or tester guidance.
    state.RecordSuccess(GuideTarget::Map);
    if(!require(!state.NeedsExplanation(GuideTarget::Map) &&
                state.NeedsExplanation(GuideTarget::Library) &&
                state.NeedsExplanation(GuideTarget::Tester),"map cannot suppress unrelated guides"))return 2;
    // Cancel leaves state unchanged; the caller only records successful loads.
    if(!require(state.NeedsExplanation(GuideTarget::Tester),"cancel does not suppress tester"))return 3;
    state.RecordSuccess(GuideTarget::Library);
    state.Suppress(GuideTarget::Tester);
    for(auto kind : {GuideTarget::Map,GuideTarget::Library,GuideTarget::Tester})
        if(!require(state.Seen(kind),"all used/suppressed guides can be persisted"))return 4;
    GuidanceState loaded;
    loaded.Restore(GuideTarget::Map,true);
    loaded.Restore(GuideTarget::Library,false);
    loaded.Restore(GuideTarget::Tester,true);
    if(!require(!loaded.NeedsExplanation(GuideTarget::Map) &&
                loaded.NeedsExplanation(GuideTarget::Library) &&
                !loaded.NeedsExplanation(GuideTarget::Tester),"backward compatible independent guideSeen flags"))return 5;
    std::cout<<"PASS: guidance preferences independently persist map, library and tester explanations\n";
}
