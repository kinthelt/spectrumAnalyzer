#include "TestFramework.h"

int main() {
  int failedTests = 0;
  for (const auto& test : testfw::registry()) {
    const int before = testfw::failures();
    test.body();
    const bool passed = testfw::failures() == before;
    if (!passed) ++failedTests;
    std::printf("%s %s\n", passed ? "  ok  " : "  FAIL", test.name);
  }
  std::printf("\n%zu tests, %d failed\n", testfw::registry().size(), failedTests);
  return failedTests == 0 ? 0 : 1;
}
