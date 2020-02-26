// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "az_json_string_private.h"
#include <az_json.h>

#include <az_test.h>
#include <stdio.h>

#include <_az_cfg.h>

az_result read_write(az_span input, az_span* output, int32_t* o);
az_result read_write_token(az_span* output, int32_t* o, az_json_parser* state, az_json_token token);
az_result write_str(az_span span, az_span s, az_span* out);

static az_span const sample1 = AZ_SPAN_LITERAL_FROM_STR( //
    "{\n"
    "  \"parameters\": {\n"
    "    \"subscriptionId\": \"{subscription-id}\",\n"
    "      \"resourceGroupName\" : \"res4303\",\n"
    "      \"accountName\" : \"sto7280\",\n"
    "      \"containerName\" : \"container8723\",\n"
    "      \"api-version\" : \"2019-04-01\",\n"
    "      \"monitor\" : \"true\",\n"
    "      \"LegalHold\" : {\n"
    "      \"tags\": [\n"
    "        \"tag1\",\n"
    "          \"tag2\",\n"
    "          \"tag3\"\n"
    "      ]\n"
    "    }\n"
    "  },\n"
    "    \"responses\": {\n"
    "    \"200\": {\n"
    "      \"body\": {\n"
    "        \"hasLegalHold\": false,\n"
    "          \"tags\" : []\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "}\n");

void test_json_parser()
{
  {
    az_json_parser parser = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&parser, AZ_SPAN_FROM_STR("    ")));
    TEST_ASSERT(az_json_parser_parse_token(&parser, NULL) == AZ_ERROR_ARG);
  }
  {
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(NULL, &token) == AZ_ERROR_ARG);
  }
  {
    az_json_parser parser = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&parser, AZ_SPAN_FROM_STR("    ")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&parser, &token) == AZ_ERROR_EOF);
  }
  {
    az_json_parser parser = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&parser, AZ_SPAN_FROM_STR("  null  ")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&parser, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_NULL);
    TEST_ASSERT(az_json_parser_done(&parser) == AZ_OK);
  }
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, AZ_SPAN_FROM_STR("  nul")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_ERROR_EOF);
  }
  {
    az_json_parser parser = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&parser, AZ_SPAN_FROM_STR("  false")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&parser, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_BOOLEAN);
    TEST_ASSERT(token.value.boolean == false);
    TEST_ASSERT(az_json_parser_done(&parser) == AZ_OK);
  }
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, AZ_SPAN_FROM_STR("  falsx  ")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_ERROR_PARSER_UNEXPECTED_CHAR);
  }
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, AZ_SPAN_FROM_STR("true ")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_BOOLEAN);
    TEST_ASSERT(token.value.boolean == true);
    TEST_ASSERT(az_json_parser_done(&state) == AZ_OK);
  }
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, AZ_SPAN_FROM_STR("  truem")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_ERROR_PARSER_UNEXPECTED_CHAR);
  }
  {
    az_span const s = AZ_SPAN_FROM_STR(" \"tr\\\"ue\\t\" ");
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, s));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_STRING);
    TEST_ASSERT(az_span_ptr(token.value.string) == (az_span_ptr(s) + 2));
    TEST_ASSERT(az_span_length(token.value.string) == 8);
    TEST_ASSERT(az_json_parser_done(&state) == AZ_OK);
  }
  {
    az_span const s = AZ_SPAN_FROM_STR("\"\\uFf0F\"");
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, s));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_STRING);
    TEST_ASSERT(az_span_ptr(token.value.string) == az_span_ptr(s) + 1);
    TEST_ASSERT(az_span_length(token.value.string) == 6);
    TEST_ASSERT(az_json_parser_done(&state) == AZ_OK);
  }
  {
    az_span const s = AZ_SPAN_FROM_STR("\"\\uFf0\"");
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, s));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_ERROR_PARSER_UNEXPECTED_CHAR);
  }
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, AZ_SPAN_FROM_STR(" 23 ")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_NUMBER);
    TEST_ASSERT(token.value.number == 23);
    TEST_ASSERT(az_json_parser_done(&state) == AZ_OK);
  }
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, AZ_SPAN_FROM_STR(" -23.56")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_NUMBER);
    TEST_ASSERT(token.value.number == -23.56);
    TEST_ASSERT(az_json_parser_done(&state) == AZ_OK);
  }
  /* Testing parsing number and converting to double (az_json_number_to_double) */
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, AZ_SPAN_FROM_STR(" -23.56e-3")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_NUMBER);
    TEST_ASSERT(token.value.number == -0.02356);
    TEST_ASSERT(az_json_parser_done(&state) == AZ_OK);
  }
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, AZ_SPAN_FROM_STR("1e19")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_ERROR_BUFFER_OVERFLOW);
  }
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(
        az_json_parser_init(&state, AZ_SPAN_FROM_STR("10000000000000000000000e17")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_ERROR_BUFFER_OVERFLOW);
  }
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, AZ_SPAN_FROM_STR("1e18")));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_NUMBER);
    TEST_ASSERT(token.value.number == 1000000000000000000);
    TEST_ASSERT(az_json_parser_done(&state) == AZ_OK);
  }
  /* end of Testing parsing number and converting to double */
  {
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, AZ_SPAN_FROM_STR(" [ true, 0.3 ]")));
    az_json_token token = { 0 };
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_ARRAY);
    TEST_ASSERT(az_json_parser_parse_array_item(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_BOOLEAN);
    TEST_ASSERT(token.value.boolean == true);
    TEST_ASSERT(az_json_parser_parse_array_item(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_NUMBER);
    // TEST_ASSERT(token.value.number == 0.3);  TODO:  why do we get 0.30000004 ??
    TEST_ASSERT(az_json_parser_parse_array_item(&state, &token) == AZ_ERROR_ITEM_NOT_FOUND);
    TEST_ASSERT(az_json_parser_done(&state) == AZ_OK);
  }
  {
    az_span const json = AZ_SPAN_FROM_STR("{\"a\":\"Hello world!\"}");
    az_json_parser state = { 0 };
    TEST_EXPECT_SUCCESS(az_json_parser_init(&state, json));
    az_json_token token;
    TEST_ASSERT(az_json_parser_parse_token(&state, &token) == AZ_OK);
    TEST_ASSERT(token.kind == AZ_JSON_TOKEN_OBJECT);
    az_json_token_member token_member;
    TEST_ASSERT(az_json_parser_parse_token_member(&state, &token_member) == AZ_OK);
    TEST_ASSERT(az_span_ptr(token_member.name) == az_span_ptr(json) + 2);
    TEST_ASSERT(az_span_length(token_member.name) == 1);
    TEST_ASSERT(token_member.token.kind == AZ_JSON_TOKEN_STRING);
    TEST_ASSERT(az_span_ptr(token_member.token.value.string) == az_span_ptr(json) + 6);
    TEST_ASSERT(az_span_length(token_member.token.value.string) == 12);
    TEST_ASSERT(
        az_json_parser_parse_token_member(&state, &token_member) == AZ_ERROR_ITEM_NOT_FOUND);
    TEST_ASSERT(az_json_parser_done(&state) == AZ_OK);
  }
  {
    uint8_t buffer[1000];
    az_span output = AZ_SPAN_FROM_BUFFER(buffer);
    {
      int32_t o = 0;
      TEST_ASSERT(
          read_write(AZ_SPAN_FROM_STR("{ \"a\" : [ true, { \"b\": [{}]}, 15 ] }"), &output, &o)
          == AZ_OK);

      TEST_ASSERT(az_span_is_equal(output, AZ_SPAN_FROM_STR("{\"a\":[true,{\"b\":[{}]},0]}")));
    }
    {
      int32_t o = 0;
      output = AZ_SPAN_FROM_BUFFER(buffer);
      az_span const json = AZ_SPAN_FROM_STR(
          // 0           1           2           3           4           5 6
          // 01234 56789 01234 56678 01234 56789 01234 56789 01234 56789 01234
          // 56789 0123
          "[[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ "
          "[[[[[ [[[[");
      az_result const result = read_write(json, &output, &o);
      TEST_ASSERT(result == AZ_ERROR_JSON_NESTING_OVERFLOW);
    }
    {
      int32_t o = 0;
      output = AZ_SPAN_FROM_BUFFER(buffer);
      az_span const json = AZ_SPAN_FROM_STR(
          // 0           1           2           3           4           5 6 01234
          // 56789 01234 56678 01234 56789 01234 56789 01234 56789 01234 56789 012
          "[[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ "
          "[[[[[ [[[");
      az_result const result = read_write(json, &output, &o);
      TEST_ASSERT(result == AZ_ERROR_EOF);
    }
    {
      int32_t o = 0;
      output = AZ_SPAN_FROM_BUFFER(buffer);
      az_span const json = AZ_SPAN_FROM_STR(
          // 0           1           2           3           4           5 6 01234
          // 56789 01234 56678 01234 56789 01234 56789 01234 56789 01234 56789 012
          "[[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ [[[[[ "
          "[[[[[ [[{"
          "   \"\\t\\n\": \"\\u0abc\"   "
          "}]]]] ]]]]] ]]]]] ]]]]] ]]]]] ]]]]] ]]]]] ]]]]] ]]]]] ]]]]] ]]]]] "
          "]]]]] ]]]");
      output._internal.length = 0;
      output = AZ_SPAN_FROM_BUFFER(buffer);
      az_result const result = read_write(json, &output, &o);
      TEST_ASSERT(result == AZ_OK);

      TEST_ASSERT(az_span_is_equal(
          output,
          AZ_SPAN_FROM_STR( //
              "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[{"
              "\"\\t\\n\":\"\\u0abc\""
              "}]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]"
              "]")));
    }
    //
    {
      int32_t o = 0;
      output = AZ_SPAN_FROM_BUFFER(buffer);
      az_result const result = read_write(sample1, &output, &o);
      TEST_ASSERT(result == AZ_OK);
    }
  }
}

az_result read_write(az_span input, az_span* output, int32_t* o)
{
  az_json_parser parser = { 0 };
  TEST_EXPECT_SUCCESS(az_json_parser_init(&parser, input));
  az_json_token token;
  AZ_RETURN_IF_FAILED(az_json_parser_parse_token(&parser, &token));
  AZ_RETURN_IF_FAILED(read_write_token(output, o, &parser, token));
  return az_json_parser_done(&parser);
}

az_result read_write_token(az_span* output, int32_t* o, az_json_parser* state, az_json_token token)
{
  switch (token.kind)
  {
    case AZ_JSON_TOKEN_NULL:
      return az_span_append(*output, AZ_SPAN_FROM_STR("null"), output);
    case AZ_JSON_TOKEN_BOOLEAN:
      return az_span_append(
          *output,
          token.value.boolean ? AZ_SPAN_FROM_STR("true") : AZ_SPAN_FROM_STR("false"),
          output);
    case AZ_JSON_TOKEN_NUMBER:
      return az_span_append(*output, AZ_SPAN_FROM_STR("0"), output);
    case AZ_JSON_TOKEN_STRING:
      return write_str(*output, token.value.string, output);
    case AZ_JSON_TOKEN_OBJECT:
    {
      AZ_RETURN_IF_FAILED(az_span_append(*output, AZ_SPAN_FROM_STR("{"), output));
      bool need_comma = false;
      while (true)
      {
        az_json_token_member member;
        az_result const result = az_json_parser_parse_token_member(state, &member);
        if (result == AZ_ERROR_ITEM_NOT_FOUND)
        {
          break;
        }
        AZ_RETURN_IF_FAILED(result);
        if (need_comma)
        {
          AZ_RETURN_IF_FAILED(az_span_append(*output, AZ_SPAN_FROM_STR(","), output));
        }
        else
        {
          need_comma = true;
        }
        AZ_RETURN_IF_FAILED(write_str(*output, member.name, output));
        AZ_RETURN_IF_FAILED(az_span_append(*output, AZ_SPAN_FROM_STR(":"), output));
        AZ_RETURN_IF_FAILED(read_write_token(output, o, state, member.token));
      }
      return az_span_append(*output, AZ_SPAN_FROM_STR("}"), output);
    }
    case AZ_JSON_TOKEN_ARRAY:
    {
      AZ_RETURN_IF_FAILED(az_span_append(*output, AZ_SPAN_FROM_STR("["), output));
      bool need_comma = false;
      while (true)
      {
        az_json_token element;
        az_result const result = az_json_parser_parse_array_item(state, &element);
        if (result == AZ_ERROR_ITEM_NOT_FOUND)
        {
          break;
        }
        AZ_RETURN_IF_FAILED(result);
        if (need_comma)
        {
          AZ_RETURN_IF_FAILED(az_span_append(*output, AZ_SPAN_FROM_STR(","), output));
        }
        else
        {
          need_comma = true;
        }
        AZ_RETURN_IF_FAILED(read_write_token(output, o, state, element));
      }
      return az_span_append(*output, AZ_SPAN_FROM_STR("]"), output);
    }
    default:
      break;
  }
  return AZ_ERROR_JSON_INVALID_STATE;
}

az_result write_str(az_span span, az_span s, az_span* out)
{
  *out = span;
  AZ_RETURN_IF_FAILED(az_span_append(*out, AZ_SPAN_FROM_STR("\""), out));
  AZ_RETURN_IF_FAILED(az_span_append(*out, s, out));
  AZ_RETURN_IF_FAILED(az_span_append(*out, AZ_SPAN_FROM_STR("\""), out));
  return AZ_OK;
}
