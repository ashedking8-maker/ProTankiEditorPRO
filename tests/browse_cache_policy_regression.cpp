#include "BrowseCachePolicy.h"
#include "ReleaseTestCheck.h"
#include <iostream>
int main() {
    using namespace BrowseCachePolicy;
    PT_REQUIRE(GpuBudget(0)==64u*MiB);
    PT_REQUIRE(GpuBudget(1u*1024*MiB)==64u*MiB);
    PT_REQUIRE(GpuBudget(2u*1024*MiB)==128u*MiB);
    PT_REQUIRE(GpuBudget(4ull*1024*MiB)==256u*MiB);
    PT_REQUIRE(GpuBudget(12ull*1024*MiB)==256u*MiB);
    PT_REQUIRE(MaxCpuBytes==128u*MiB);
    PT_REQUIRE(RawBytes(194,146)==113296u);
    std::cout<<"PASS: byte-budgeted adaptive GPU and bounded CPU thumbnail cache policy\n";
}
