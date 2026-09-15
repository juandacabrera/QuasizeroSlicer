// Quasizero Slicer — minimal dependency-free test harness.
// Used both by CI (ctest) and constrained environments without the full
// OrcaSlicer dependency tree. GNU AGPLv3.
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace qztest {
struct Registry {
    struct Case { std::string name; std::function<void()> fn; };
    static Registry& inst() { static Registry r; return r; }
    std::vector<Case> cases;
    int failures = 0;
    std::string current;
};
struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        Registry::inst().cases.push_back({name, std::move(fn)});
    }
};
inline void fail(const char* file, int line, const std::string& msg) {
    std::printf("  FAIL %s:%d [%s] %s\n", file, line, Registry::inst().current.c_str(), msg.c_str());
    ++Registry::inst().failures;
}
inline int run_all() {
    auto& r = Registry::inst();
    for (auto& c : r.cases) {
        r.current = c.name;
        int before = r.failures;
        c.fn();
        std::printf("%s %s\n", (r.failures == before) ? "PASS" : "FAIL", c.name.c_str());
    }
    std::printf("== %zu tests, %d failures ==\n", r.cases.size(), r.failures);
    return r.failures == 0 ? 0 : 1;
}
} // namespace qztest

#define QZ_TEST(name) \
    static void qz_test_##name(); \
    static qztest::Registrar qz_reg_##name(#name, qz_test_##name); \
    static void qz_test_##name()

#define QZ_CHECK(cond) do { if (!(cond)) qztest::fail(__FILE__, __LINE__, #cond); } while (0)
#define QZ_CHECK_NEAR(a, b, tol) do { \
    double _va=(a), _vb=(b); \
    if (std::fabs(_va-_vb) > (tol)) { char _m[256]; \
        std::snprintf(_m, sizeof(_m), "%s ≈ %s: %.6f vs %.6f (tol %.6f)", #a, #b, _va, _vb, (double)(tol)); \
        qztest::fail(__FILE__, __LINE__, _m); } } while (0)
#define QZ_TEST_MAIN int main() { return qztest::run_all(); }
