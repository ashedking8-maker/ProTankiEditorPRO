#include "NativeCloneEditPolicy.h"
#include <iostream>
using namespace NativeCloneEditPolicy;
static_assert(MayEdit(false,true,true));
static_assert(!MayEdit(false,false,true));
static_assert(!MayEdit(true,true,true));
static_assert(MayEdit(false,false,false));
static_assert(!MayEdit(true,false,false));
static_assert(!MayEraseOpaque(false));
static_assert(MayEraseOpaque(true));
static_assert(!MayEraseUnresolved(false,false));
static_assert(!MayEraseUnresolved(true,false));
static_assert(!MayEraseUnresolved(false,true));
static_assert(MayEraseUnresolved(true,true));
int main() {
    for (bool opaque : {false,true}) {
        for (bool owned : {false,true}) {
            for (bool unresolved : {false,true}) {
                const bool expected=!unresolved && (!opaque || owned);
                if (MayEdit(unresolved,owned,opaque)!=expected)return 1;
            }
        }
    }
    if(MayEraseOpaque(false)||!MayEraseOpaque(true))return 2;
    for(bool complete : {false,true})for(bool opaque : {false,true}) {
        if(MayEraseUnresolved(complete,opaque)!=(complete&&opaque))return 3;
    }
    std::cout<<"PASS: clone edit policy, guarded unresolved ownership, opaque whole-batch cleanup.\n";
    return 0;
}
