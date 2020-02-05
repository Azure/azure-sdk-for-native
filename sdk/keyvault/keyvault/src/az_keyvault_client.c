// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <az_http_pipeline_internal.h>
#include <az_json.h>
#include <az_keyvault.h>
#include <az_span.h>

#include <_az_cfg.h>

/**
 * @brief Maximum allowed URL size:
 * url is expected as : [https://]{account_id}[.vault.azure.net]{path}{query}
 * URL token                       max Len            Total
 * [https://]                       = 8                 8
 * {account_id}                     = 52               60
 * [.vault.azure.net]               = 16               76
 * {path}                           = 54               130
 * {query}                          = 70               ** 200 **
 */
enum { MAX_URL_SIZE = 200 };
enum { MAX_BODY_SIZE = 1024 };
static az_span const AZ_HTTP_HEADER_API_VERSION = AZ_SPAN_LITERAL_FROM_STR("api-version");

AZ_NODISCARD AZ_INLINE az_span az_keyvault_client_constant_for_keys() {
  return AZ_SPAN_FROM_STR("keys");
}
AZ_NODISCARD AZ_INLINE az_span az_keyvault_client_constant_for_create() {
  return AZ_SPAN_FROM_STR("create");
}

AZ_NODISCARD AZ_INLINE az_span az_keyvault_client_constant_for_content_type() {
  return AZ_SPAN_FROM_STR("Content-Type");
}
AZ_NODISCARD AZ_INLINE az_span az_keyvault_client_constant_for_application_json() {
  return AZ_SPAN_FROM_STR("application/json");
}

AZ_NODISCARD az_keyvault_keys_client_options
az_keyvault_keys_client_options_default(az_http_client http_client) {

  az_keyvault_keys_client_options options = (az_keyvault_keys_client_options){
    ._internal
    = { .http_client = http_client, .api_version = az_http_policy_apiversion_options_default() },
    .retry = az_http_policy_retry_options_default(),
  };

  options._internal.api_version.add_as_header = false;
  options._internal.api_version.name = AZ_HTTP_HEADER_API_VERSION;
  options._internal.api_version.version = AZ_KEYVAULT_API_VERSION;

  return options;
}

AZ_NODISCARD az_result az_keyvault_keys_client_init(
    az_keyvault_keys_client * self,
    az_span uri,
    void * credential,
    az_keyvault_keys_client_options * options) {
  AZ_CONTRACT_ARG_NOT_NULL(self);
  AZ_CONTRACT_ARG_NOT_NULL(options);

  *self
      = (az_keyvault_keys_client){ ._internal
                                   = { .uri = AZ_SPAN_FROM_BUFFER(self->_internal.url_buffer),
                                       .initial_url_length = az_span_length(uri),
                                       .options = *options,
                                       ._token = { 0 },
                                       ._token_context = { 0 },
                                       .pipeline = (az_http_pipeline){
                                           .p_policies = {
                                               { .process = az_http_pipeline_policy_apiversion,
                                                 .p_options
                                                 = &self->_internal.options._internal.api_version },
                                               { .process = az_http_pipeline_policy_uniquerequestid,
                                                 .p_options = NULL },
                                               { .process = az_http_pipeline_policy_telemetry,
                                                 .p_options = &self->_internal.options._internal
                                                                   ._telemetry_options },
                                               { .process = az_http_pipeline_policy_retry,
                                                 .p_options = &(self->_internal.options.retry) },
                                               { .process = az_http_pipeline_policy_credential,
                                                 .p_options = &(self->_internal._token_context) },
                                               { .process = az_http_pipeline_policy_logging,
                                                 .p_options = NULL },
                                               { .process = az_http_pipeline_policy_bufferresponse,
                                                 .p_options = NULL },
                                               { .process
                                                 = az_http_pipeline_policy_distributedtracing,
                                                 .p_options = NULL },
                                               { .process = az_http_pipeline_policy_transport,
                                                 .p_options
                                                 = &self->_internal.options._internal.http_client },
                                           } } } };

  // Copy url to client buffer so customer can re-use buffer on his/her side
  AZ_RETURN_IF_FAILED(az_span_copy(self->_internal.uri, uri, &self->_internal.uri));

  AZ_RETURN_IF_FAILED(az_identity_access_token_init(&(self->_internal._token)));
  AZ_RETURN_IF_FAILED(az_identity_access_token_context_init(
      &(self->_internal._token_context),
      credential,
      &(self->_internal._token),
      AZ_SPAN_FROM_STR("https://vault.azure.net/.default")));

  return AZ_OK;
}

AZ_NODISCARD az_keyvault_create_key_options az_keyvault_create_key_options_default() {
  return (az_keyvault_create_key_options){ .enabled = false, .operations = NULL, .tags = NULL };
}

/**
 * @brief Internal inline function in charge of building json payload for creating a new key
 *
 * @param json_web_key_type type of the key. It will be always added to json payload
 * @param options all optional settings that can be inside create key options
 * @param http_body action used by json builder to be called while building
 * @return AZ_NODISCARD _az_keyvault_keys_key_create_build_json_body
 */
AZ_NODISCARD az_result _az_keyvault_keys_key_create_build_json_body(
    az_span json_web_key_type,
    az_keyvault_create_key_options * options,
    az_span * http_body) {

  az_json_builder builder = { 0 };

  AZ_RETURN_IF_FAILED(az_json_builder_init(&builder, *http_body));

  AZ_RETURN_IF_FAILED(az_json_builder_append_token(&builder, az_json_token_object()));
  // Required fields
  AZ_RETURN_IF_FAILED(az_json_builder_append_object(
      &builder, AZ_SPAN_FROM_STR("kty"), az_json_token_string(json_web_key_type)));

  /**************** Non-Required fields ************/
  if (options != NULL) {
    // Attributes
    {
      az_optional_bool const enabled_field = options->enabled;
      if (enabled_field.is_present) {
        AZ_RETURN_IF_FAILED(az_json_builder_append_object(
            &builder, AZ_SPAN_FROM_STR("attributes"), az_json_token_object()));
        AZ_RETURN_IF_FAILED(az_json_builder_append_object(
            &builder, AZ_SPAN_FROM_STR("enabled"), az_json_token_boolean(enabled_field.data)));
        AZ_RETURN_IF_FAILED(az_json_builder_append_object_close(&builder));
      }
      // operations
      if (options->operations != NULL) {
        AZ_RETURN_IF_FAILED(az_json_builder_append_object(
            &builder, AZ_SPAN_FROM_STR("key_ops"), az_json_token_array()));
        for (size_t op = 0; true; ++op) {
          az_span s = options->operations[op];
          if (az_span_is_equal(s, az_span_null())) {
            break;
          }
          AZ_RETURN_IF_FAILED(az_json_builder_append_array_item(&builder, az_json_token_string(s)));
        }
        AZ_RETURN_IF_FAILED(az_json_builder_append_array_close(&builder));
      }
      // tags
      if (options->tags != NULL) {
        AZ_RETURN_IF_FAILED(az_json_builder_append_object(
            &builder, AZ_SPAN_FROM_STR("tags"), az_json_token_object()));
        for (size_t tag_index = 0; true; ++tag_index) {
          az_pair const tag = options->tags[tag_index];
          if (az_span_is_equal(tag.key, az_span_null())) {
            break;
          }
          AZ_RETURN_IF_FAILED(
              az_json_builder_append_object(&builder, tag.key, az_json_token_string(tag.value)));
        }
        AZ_RETURN_IF_FAILED(az_json_builder_append_object_close(&builder));
      }
    }
  }

  AZ_RETURN_IF_FAILED(az_json_builder_append_object_close(&builder));
  *http_body = builder._internal.json;

  return AZ_OK;
}

AZ_NODISCARD static az_result _az_reset_url_to_initial_state(az_keyvault_keys_client * client) {
  if (client->_internal.initial_url_length != az_span_length(client->_internal.uri)) {
    // Can't use slice here because we would lost original capacity
    client->_internal.uri = az_span_init(
        az_span_ptr(client->_internal.uri),
        client->_internal.initial_url_length,
        az_span_capacity(client->_internal.uri));
  }
  return AZ_OK;
}

AZ_NODISCARD az_result az_keyvault_keys_key_create(
    az_keyvault_keys_client * client,
    az_span key_name,
    json_web_key_type json_web_key_type,
    az_keyvault_create_key_options * options,
    az_http_response * response) {

  // Headers buffer
  uint8_t headers_buffer[4 * sizeof(az_pair)];
  az_span request_headers_span = AZ_SPAN_FROM_BUFFER(headers_buffer);

  // check if url needs to be reset to initial state
  AZ_RETURN_IF_FAILED(_az_reset_url_to_initial_state(client));

  // Allocate buffer in stack to hold body request
  uint8_t body_buffer[MAX_BODY_SIZE];
  az_span json_builder = AZ_SPAN_FROM_BUFFER(body_buffer);
  AZ_RETURN_IF_FAILED(
      _az_keyvault_keys_key_create_build_json_body(json_web_key_type, options, &json_builder));
  az_span const created_body = json_builder;

  // create request
  az_http_request hrb;
  AZ_RETURN_IF_FAILED(az_http_request_init(
      &hrb, az_http_method_post(), client->_internal.uri, request_headers_span, created_body));

  // add path to request
  AZ_RETURN_IF_FAILED(az_http_request_append_path(&hrb, az_keyvault_client_constant_for_keys()));

  AZ_RETURN_IF_FAILED(az_http_request_append_path(&hrb, key_name));

  AZ_RETURN_IF_FAILED(az_http_request_append_path(&hrb, az_keyvault_client_constant_for_create()));

  // Adding header content-type json
  AZ_RETURN_IF_FAILED(az_http_request_append_header(
      &hrb,
      az_keyvault_client_constant_for_content_type(),
      az_keyvault_client_constant_for_application_json()));

  // start pipeline
  return az_http_pipeline_process(&client->_internal.pipeline, &hrb, response);
}

/**
 * @brief Currently returning last key version. Need to update to get version key
 *
 * @param client
 * @param key_name
 * @param key_type
 * @param response
 * @return AZ_NODISCARD az_keyvault_keys_key_get
 */
AZ_NODISCARD az_result az_keyvault_keys_key_get(
    az_keyvault_keys_client * client,
    az_span key_name,
    az_span key_version,
    az_http_response * response) {
  // create request buffer TODO: define size for a getKey Request

  uint8_t headers_buffer[4 * sizeof(az_pair)];
  az_span request_headers_span = AZ_SPAN_FROM_BUFFER(headers_buffer);

  // check if url needs to be reset to initial state
  AZ_RETURN_IF_FAILED(_az_reset_url_to_initial_state(client));

  // create request
  az_http_request hrb;
  AZ_RETURN_IF_FAILED(az_http_request_init(
      &hrb, az_http_method_get(), client->_internal.uri, request_headers_span, az_span_null()));

  // Add path to request
  AZ_RETURN_IF_FAILED(az_http_request_append_path(&hrb, az_keyvault_client_constant_for_keys()));

  // Add path to request after adding query parameter
  AZ_RETURN_IF_FAILED(az_http_request_append_path(&hrb, key_name));

  // Add key_version if requested
  if (az_span_length(key_version) > 0) {
    AZ_RETURN_IF_FAILED(az_http_request_append_path(&hrb, key_version));
  }

  // start pipeline
  return az_http_pipeline_process(&client->_internal.pipeline, &hrb, response);
}

AZ_NODISCARD az_result az_keyvault_keys_key_delete(
    az_keyvault_keys_client * client,
    az_span key_name,
    az_http_response * response) {

  // create request buffer TODO: define size for a getKey Request
  uint8_t url_buffer[1024 * 4];
  az_span request_url_span = AZ_SPAN_FROM_BUFFER(url_buffer);
  uint8_t headers_buffer[4 * sizeof(az_pair)];
  az_span request_headers_span = AZ_SPAN_FROM_BUFFER(headers_buffer);

  // check if url needs to be reset to initial state
  AZ_RETURN_IF_FAILED(_az_reset_url_to_initial_state(client));

  // create request
  // TODO: define max URL size
  az_http_request hrb;
  AZ_RETURN_IF_FAILED(az_http_request_init(
      &hrb, az_http_method_get(), request_url_span, request_headers_span, az_span_null()));

  // Add path to request
  AZ_RETURN_IF_FAILED(az_http_request_append_path(&hrb, az_keyvault_client_constant_for_keys()));
  AZ_RETURN_IF_FAILED(az_http_request_append_path(&hrb, key_name));

  // start pipeline
  return az_http_pipeline_process(&client->_internal.pipeline, &hrb, response);
}
