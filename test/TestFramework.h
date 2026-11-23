// Tiny dependency-free test framework for the PC-side unit tests.
//
//   TEST(name) { CHECK(x == 1); CHECK_EQ(a, b); }
//
// Tests register themselves; test_main.cpp runs them all and reports failures.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testfw {

struct TestCase {
  const char* name;
  std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

inline int& failures() {
  static int count = 0;
  return count;
}

struct Registrar {
  Registrar(const char* name, std::function<void()> body) { registry().push_back({name, body}); }
};

inline void fail(const char* file, int line, const std::string& what) {
  ++failures();
  std::printf("    FAIL %s:%d: %s\n", file, line, what.c_str());
}

}  // namespace testfw

#define TEST(name)                                                    \
  static void test_##name();                                          \
  static testfw::Registrar registrar_##name(#name, test_##name);      \
  static void test_##name()

#define CHECK(cond) \
  do { if (!(cond)) testfw::fail(__FILE__, __LINE__, #cond); } while (0)

#define CHECK_EQ(a, b)                                                                     \
  do {                                                                                     \
    const auto va = (a);                                                                   \
    const auto vb = (b);                                                                   \
    if (!(va == vb))                                                                       \
      testfw::fail(__FILE__, __LINE__, std::string(#a " == " #b " (") +                    \
                                          std::to_string(+va) + " vs " + std::to_string(+vb) + ")"); \
  } while (0)
