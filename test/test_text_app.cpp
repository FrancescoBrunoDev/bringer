/*
 * test_text_app.cpp
 *
 * PlatformIO unit tests for the Text App.
 *
 * - Verifies there is at least one selectable text option
 * - Verifies the first option is non-empty
 * - Verifies out-of-range index returns nullptr
 */

#include <Arduino.h>
#include <unity.h>
#include <string.h>

#include "app/routes/text_app/text_app.h"

void test_text_app_has_options(void) {
  size_t cnt = text_app_get_count();
  TEST_ASSERT_TRUE_MESSAGE(cnt > 0, "Expected at least one text option");

  const char *t = text_app_get_text(0);
  TEST_ASSERT_NOT_NULL_MESSAGE(t, "First text option should not be NULL");
  TEST_ASSERT_TRUE_MESSAGE(strlen(t) > 0, "First text option should not be empty");
}

void test_text_app_out_of_range(void) {
  size_t cnt = text_app_get_count();
  const char *t = text_app_get_text(cnt); // index == count is out of range
  TEST_ASSERT_NULL_MESSAGE(t, "Out-of-range index should return nullptr");
}

void setup() {
  // Small delay so serial/driver stacks can settle on device test runs
  delay(200);
  UNITY_BEGIN();
  RUN_TEST(test_text_app_has_options);
  RUN_TEST(test_text_app_out_of_range);
  UNITY_END();
}

void loop() {
  // Not used in unit tests
}