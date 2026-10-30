// Автотесты модуля варианта 5 (20 проверок).
// Сборка: g++ -std=c++17 tests/variant_tests.cpp -o variant_tests
#include <iostream>
#include "../src/modules/individual/variant_tests.hpp"

int main() {
    auto tests = VariantTests::run();
    int passed = 0;
    for (size_t i = 0; i < tests.size(); ++i) {
        if (tests[i].second) ++passed;
        std::cout << (tests[i].second ? "[PASSED] " : "[FAILED] ") << (i + 1) << ". " << tests[i].first << "\n";
    }
    std::cout << "\nИтого: " << passed << "/" << tests.size() << "\n";
    return passed == (int)tests.size() ? 0 : 1;
}
