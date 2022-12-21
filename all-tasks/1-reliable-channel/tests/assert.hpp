#pragma once

#include <wheels/support/panic.hpp>

#define TEST_ASSERT(cond)                               \
  do {                                                  \
    if (!(cond)) {                                      \
      WHEELS_PANIC("Test assertion failed: " << #cond); \
    }                                                   \
  } while (false)
