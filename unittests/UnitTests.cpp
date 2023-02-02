#include "RuntimeTest.h"
#include "ToolsTest.h"

static MunitSuite suites[] = {
  RuntimeTests::suite,
  ToolsTests::suite,
  nullptr};

static const MunitSuite mainsuite = {
  .prefix = "/hydra",
  .suites = suites,
  .iterations = 1,
};

int main(int argc, char* argv[])
{
  return munit_suite_main(&mainsuite, NULL, argc, argv);
}
