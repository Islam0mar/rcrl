#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../src/rcrl/rcrl.h"
#include "doctest/doctest/doctest.h"

TEST_CASE("single variables") {
  int exitcode = 0;

  rcrl::Plugin p;

  p.CompileCode("int a = 5;");
  while (!p.TryGetExitStatusFromCompile(exitcode))
    ;
  REQUIRE_FALSE(exitcode);
  p.CopyAndLoadNewPlugin();

  p.CompileCode("a++;");
  while (!p.TryGetExitStatusFromCompile(exitcode))
    ;
  REQUIRE_FALSE(exitcode);
  p.CopyAndLoadNewPlugin();
}

#ifndef __APPLE__

#ifdef _WIN32
#define RCRL_SYMBOL_EXPORT __declspec(dllexport)
#else
#define RCRL_SYMBOL_EXPORT __attribute__((visibility("default")))
#endif

std::vector<int> g_pushed_ints;
RCRL_SYMBOL_EXPORT void test_ctor_dtor_order(int num) {
  g_pushed_ints.push_back(num);
}

TEST_CASE("destructor order") {
  int exitcode = 0;

  // about to check destructor order
  rcrl::Plugin p;
  p.CompileCode(R"raw(
//vars
int num_instances = 0;

//global
RCRL_SYMBOL_IMPORT void test_ctor_dtor_order(int);
struct S {
	int instance;
	S() : instance(++num_instances) { test_ctor_dtor_order(instance); }
	~S() { test_ctor_dtor_order(instance); }
};

//vars
S a1;
S a2;
)raw");
  while (!p.TryGetExitStatusFromCompile(exitcode))
    ;
  REQUIRE_FALSE(exitcode);

  p.CopyAndLoadNewPlugin();

  REQUIRE(g_pushed_ints.size() == 2);
  REQUIRE(g_pushed_ints[0] == 1);
  REQUIRE(g_pushed_ints[1] == 2);

  REQUIRE(g_pushed_ints.size() == 4);
  REQUIRE(g_pushed_ints[2] == 2);
  REQUIRE(g_pushed_ints[3] == 1);
}

#endif
