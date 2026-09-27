#include "NativeExportCollisionPolicy.h"
#include "ReleaseTestCheck.h"
#include <string>
#include <iostream>
int main() {
    using NativeExportCollisionPolicy::Validate;
    std::string why;
    PT_REQUIRE(Validate(false,true,0,0,136,false,1,why)); // verified Land01 route
    PT_REQUIRE(Validate(false,false,0,0,0,true,1,why));  // explicit helperless box
    PT_REQUIRE(Validate(false,true,0,2,0,false,1,why));  // explicit box reauthoring
    PT_REQUIRE(!Validate(false,true,1,0,0,false,1,why)); // no plane-to-box guess
    PT_REQUIRE(!Validate(false,true,1,0,1,false,1,why)); // no mixed helper loss
    PT_REQUIRE(!Validate(false,true,0,1,1,false,1,why));
    PT_REQUIRE(!Validate(false,true,0,0,0,false,0,why));
    PT_REQUIRE(!Validate(false,false,0,0,0,false,1,why));
    PT_REQUIRE(Validate(true,true,1,0,0,false,0,why)); // explicit visual only
    std::cout<<"PASS: Land01 terrain, explicit native box, no mixed/plane silent conversion\n";
}
