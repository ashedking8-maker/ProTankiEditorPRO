#pragma once
// CTest regressions run in Release with NDEBUG defined. Unlike assert(), this
// helper always evaluates its expression, including SaveNew/Load side effects.
// A failed check returns a nonzero exit code with the exact source location.
#include <iostream>
#define PT_REQUIRE(expression) do { \
    if (!(expression)) { \
        std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": " \
                  << #expression << '\n'; \
        return 1; \
    } \
} while (false)
