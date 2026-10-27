// tests/mini_test.h
//
// PROVIDED. A tiny, dependency-free test framework.
//
//   TEST(reg_add_then_find) {
//       ToolRegistry r;
//       REQUIRE(r.add(std::make_unique<NamedTool>("a")));   // stop this test on failure
//       CHECK(r.find("a") != nullptr);                     // record failure, keep going
//       CHECK_EQ(r.size(), std::size_t{1});
//   }
//
//   int main(int argc, char** argv) { return mt::run_all(argc, argv); }
//
// Unlike assert(), CHECK/REQUIRE are never compiled out by NDEBUG, a failing
// test does not stop the other tests, and an exception escaping a test is
// reported as a failure of that test.
//
// Run a subset:  ./test_p3 reg_       (runs tests whose names start with "reg_")
// Run one test:   ./test_p3 =reg_add_and_find   (exact name)
// List tests:     ./test_p3 --list
// The summary groups results by the prefix before the first '_' in each
// test name, so name tests <area>_<what>, e.g. reg_resizes.

#pragma once
#include <cstdio>
#include <exception>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace mt {

struct TestCase {
    const char*           name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

struct AbortTest {};   // thrown by REQUIRE to leave the current test

inline int& failures_in_current() {
    static int n = 0;
    return n;
}

inline void report(const char* file, int line, const std::string& what) {
    ++failures_in_current();
    std::printf("    %s:%d: failed: %s\n", file, line, what.c_str());
}

template <typename A, typename B>
std::string describe_eq(const char* ea, const char* eb, const A& a, const B& b) {
    std::ostringstream os;
    os << ea << " == " << eb << "\n        left:  " << a << "\n        right: " << b;
    return os.str();
}

inline int run_all(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    if (filter == "--list") {
        for (const TestCase& t : registry()) std::printf("%s\n", t.name);
        return 0;
    }
    const bool exact = !filter.empty() && filter[0] == '=';
    std::map<std::string, std::pair<int, int>> by_area;   // area -> (passed, total)
    int passed = 0, total = 0;

    for (const TestCase& t : registry()) {
        const std::string name = t.name;
        if (exact ? name != filter.substr(1)
                  : (!filter.empty() && name.compare(0, filter.size(), filter) != 0))
            continue;
        ++total;
        failures_in_current() = 0;
        std::string crash;
        try {
            t.fn();
        } catch (const AbortTest&) {
        } catch (const std::exception& e) {
            crash = std::string("uncaught exception: ") + e.what();
        } catch (...) {
            crash = "uncaught non-std exception";
        }
        if (!crash.empty()) {
            ++failures_in_current();
            std::printf("    %s\n", crash.c_str());
        }
        const bool ok = failures_in_current() == 0;
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", t.name);
        std::fflush(stdout);

        const std::string area = name.substr(0, name.find('_'));
        auto& slot = by_area[area];
        slot.second += 1;
        if (ok) { ++passed; slot.first += 1; }
    }

    std::printf("\n---- summary ----\n");
    for (const auto& kv : by_area)
        std::printf("  %-12s %3d / %3d\n", kv.first.c_str(), kv.second.first, kv.second.second);
    std::printf("  %-12s %3d / %3d\n", "TOTAL", passed, total);
    if (total == 0) {
        std::printf("no tests matched '%s'\n", filter.c_str());
        return 1;
    }
    return passed == total ? 0 : 1;
}

}  // namespace mt

#define MT_CAT2(a, b) a##b
#define MT_CAT(a, b) MT_CAT2(a, b)

#define TEST(name)                                                    \
    static void name();                                               \
    static ::mt::Registrar MT_CAT(mt_reg_, name)(#name, &name);       \
    static void name()

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) ::mt::report(__FILE__, __LINE__, #cond);         \
    } while (0)

#define REQUIRE(cond)                                                 \
    do {                                                              \
        if (!(cond)) {                                                \
            ::mt::report(__FILE__, __LINE__, #cond);                  \
            throw ::mt::AbortTest{};                                  \
        }                                                             \
    } while (0)

#define CHECK_EQ(a, b)                                                \
    do {                                                              \
        const auto& mt_a_ = (a);                                      \
        const auto& mt_b_ = (b);                                      \
        if (!(mt_a_ == mt_b_))                                        \
            ::mt::report(__FILE__, __LINE__,                          \
                         ::mt::describe_eq(#a, #b, mt_a_, mt_b_));    \
    } while (0)

// Passes if `expr` throws an exception of type `ex` (or a subclass).
#define CHECK_THROWS(expr, ex)                                        \
    do {                                                              \
        bool mt_threw_ = false;                                       \
        try { (void)(expr); } catch (const ex&) { mt_threw_ = true; } \
        catch (...) {}                                                \
        if (!mt_threw_)                                               \
            ::mt::report(__FILE__, __LINE__, #expr " throws " #ex);   \
    } while (0)
