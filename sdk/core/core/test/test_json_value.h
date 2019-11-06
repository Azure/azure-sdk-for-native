// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <az_json_value.h>
#include <az_str.h>

#include "./az_test.h"

static void test_json_value() {
  az_json_value const boolean = az_json_value_create_boolean(true);
  az_json_value const number = az_json_value_create_number(-42.3);
  az_json_value const json_string = az_json_value_create_string(AZ_STR("Hello"));

  { 
    bool boolean_value = false;
    TEST_ASSERT(az_json_value_get_boolean(&json_boolean, &boolean_value) == AZ_OK);
    TEST_ASSERT(boolean_value == true);
  }
  {
    bool x = false;
    TEST_ASSERT(az_json_value_get_boolean(&json_number, &boolean_value) == AZ_ERROR_ITEM_NOT_FOUND);
  }

  {
    az_span x = { 0 };
    TEST_ASSERT(az_json_value_get_string(&s, &x) == AZ_OK);
    TEST_ASSERT(az_span_eq(x, AZ_STR("Hello")));
  }
  {
    az_span x = { 0 };
    TEST_ASSERT(az_json_value_get_string(&b, &x) == AZ_ERROR_ITEM_NOT_FOUND);
  }

  {
    double x = 0.79;
    TEST_ASSERT(az_json_value_get_number(&n, &x) == AZ_OK);
    TEST_ASSERT(x == -42.3);
  }
  {
    double x = 0.79;
    TEST_ASSERT(az_json_value_get_number(&s, &x) == AZ_ERROR_ITEM_NOT_FOUND);
  }
}
