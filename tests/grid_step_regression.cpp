#include "GridStep.h"
#include <cmath>
#include <initializer_list>
#include <iostream>
int main() {
    for (float step : {100.f,200.f,300.f,400.f,500.f}) {
        if(GridStep::Quantize(step*0.49f,step)!=0.f || GridStep::Quantize(step*0.51f,step)!=step)return 1;
        if(GridStep::Quantize(-step*1.4f,step)!=-step)return 2;
        if(GridStep::KeyboardStep(step,false)!=step || std::fabs(GridStep::KeyboardStep(step,true)-step*0.1f)>0.0001f)return 3;
    }
    // A legacy 500-wide tile is centred at 250 rather than a 100-grid origin.
    // Copying one or a group must retain this grid phase at the target position.
    if(GridStep::QuantizeAroundAnchor(749.f,100.f,250.f)!=750.f)return 4;
    if(GridStep::QuantizeAroundAnchor(1248.f,100.f,250.f)!=1250.f)return 5;
    if(GridStep::QuantizeAroundAnchor(-251.f,500.f,250.f)!=-250.f)return 6;
    std::cout<<"Grid 100-500: keyboard, mouse and legacy tile-anchor snapping PASS\n";
}
