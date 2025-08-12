#include "engine_options.h"
#include "tablebase.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

int main() {
    using namespace nikola;
    std::vector<std::string> tokens = {"setoption", "name", "SyzygyPath", "value", "/foo/bar"};
    set_option_from_tokens(tokens);
    assert(currentTablebasePath() == "/foo/bar");
    assert(tablebasePathUpdateCount() == 1);
    std::cout << "syzygy path option test passed\n";
    return 0;
}
