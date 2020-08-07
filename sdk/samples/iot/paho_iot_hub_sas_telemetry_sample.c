// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "sample.h"

#define SAMPLE_TYPE PAHO_IOT_HUB
#define SAMPLE_NAME PAHO_IOT_HUB_SAS_TELEMETRY_SAMPLE

#define TELEMETRY_SEND_INTERVAL 1
#define TELEMETRY_NUMBER_OF_MESSAGES 5

// Environment variables
static sample_environment_variables env_vars;

// Clients
static az_iot_hub_client hub_client;
static MQTTClient mqtt_client;
static char mqtt_client_username_buffer[128];

// Generate SAS key variables
static char sas_signature_buffer[128];
static char sas_b64_decoded_key_buffer[32];
static char sas_encoded_hmac256_signed_signature_buffer[128];
static char sas_b64_encoded_hmac256_signed_signature_buffer[128];
static char mqtt_password_buffer[256];

// Topics
char telemetry_topic_buffer[128];

// Functions
void create_and_configure_client();
void connect_client_to_iot_hub();
void send_telemetry_messages_to_iot_hub();
void disconnect_client_from_iot_hub();

void generate_sas_key();

int main()
{
  create_and_configure_client();
  LOG_SUCCESS("Client created and configured.");

  connect_client_to_iot_hub();
  LOG_SUCCESS("Client connected to IoT Hub.");

  send_telemetry_messages_to_iot_hub();
  LOG_SUCCESS("Client sent telemetry messages to IoT Hub.");

  disconnect_client_from_iot_hub();
  LOG_SUCCESS("Client disconnected from IoT Hub.");

  return 0;
}

void create_and_configure_client()
{
  int rc;

  // Reads in environment variables set by user for purposes of running sample
  if (az_failed(rc = read_environment_variables(SAMPLE_TYPE, SAMPLE_NAME, &env_vars)))
  {
    LOG_ERROR(
        "Failed to read configuration from environment variables: az_result return code 0x%04x.",
        rc);
    exit(rc);
  }

  // Set mqtt endpoint as null terminated in buffer
  char hub_mqtt_endpoint_buffer[128];
  if (az_failed(
          rc = create_mqtt_endpoint(
              SAMPLE_TYPE, hub_mqtt_endpoint_buffer, sizeof(hub_mqtt_endpoint_buffer))))
  {
    LOG_ERROR("Failed to create MQTT endpoint: az_result return code 0x%04x.", rc);
    exit(rc);
  }

  // Initialize the hub client with the default connection options
  if (az_failed(
          rc = az_iot_hub_client_init(
              &hub_client, env_vars.hub_hostname, env_vars.hub_device_id, NULL)))
  {
    LOG_ERROR("Failed to initialize hub client: az_result return code 0x%04x.", rc);
    exit(rc);
  }

  // Get the MQTT client id used for the MQTT connection
  char mqtt_client_id_buffer[128];
  if (az_failed(
          rc = az_iot_hub_client_get_client_id(
              &hub_client, mqtt_client_id_buffer, sizeof(mqtt_client_id_buffer), NULL)))
  {
    LOG_ERROR("Failed to get MQTT client id: az_result return code 0x%04x.", rc);
    exit(rc);
  }

  // Create the Paho MQTT client
  if ((rc = MQTTClient_create(
           &mqtt_client,
           hub_mqtt_endpoint_buffer,
           mqtt_client_id_buffer,
           MQTTCLIENT_PERSISTENCE_NONE,
           NULL))
      != MQTTCLIENT_SUCCESS)
  {
    LOG_ERROR("Failed to create MQTT client: MQTTClient return code %d.", rc);
    exit(rc);
  }

  generate_sas_key();
  LOG_SUCCESS("Client generated SAS Key.");

  return;
}

void connect_client_to_iot_hub()
{
  int rc;

  if (az_failed(
          rc = az_iot_hub_client_get_user_name(
              &hub_client, mqtt_client_username_buffer, sizeof(mqtt_client_username_buffer), NULL)))
  {
    LOG_ERROR("Failed to get MQTT username: az_result return code 0x%04x.", rc);
    exit(rc);
  }

  MQTTClient_connectOptions mqtt_connect_options = MQTTClient_connectOptions_initializer;
  mqtt_connect_options.username = mqtt_client_username_buffer;
  mqtt_connect_options.password = mqtt_password_buffer;
  mqtt_connect_options.cleansession = false; // Set to false so can receive any pending messages.
  mqtt_connect_options.keepAliveInterval = AZ_IOT_DEFAULT_MQTT_CONNECT_KEEPALIVE_SECONDS;

  MQTTClient_SSLOptions mqtt_ssl_options = MQTTClient_SSLOptions_initializer;
  if (*az_span_ptr(env_vars.x509_trust_pem_file_path) != '\0')
  {
    mqtt_ssl_options.trustStore = (char*)x509_trust_pem_file_path_buffer;
  }
  mqtt_connect_options.ssl = &mqtt_ssl_options;

  if ((rc = MQTTClient_connect(mqtt_client, &mqtt_connect_options)) != MQTTCLIENT_SUCCESS)
  {
    LOG_ERROR(
        "Failed to connect: MQTTClient return code %d.\n"
        "If on Windows, confirm the AZ_IOT_DEVICE_X509_TRUST_PEM_FILE environment variable is set "
        "correctly.",
        rc);
    exit(rc);
  }

  return;
}

void send_telemetry_messages_to_iot_hub()
{
  int rc;

  if (az_failed(
          rc = az_iot_hub_client_telemetry_get_publish_topic(
              &hub_client, NULL, telemetry_topic_buffer, sizeof(telemetry_topic_buffer), NULL)))
  {
    LOG_ERROR("Failed to get telemetry publish topic: az_result return code 0x%04x.", rc);
    exit(rc);
  }

  const char* telemetry_message_payloads[TELEMETRY_NUMBER_OF_MESSAGES] = {
    "Message One", "Message Two", "Message Three", "Message Four", "Message Five",
  };

  for (int i = 0; i < TELEMETRY_NUMBER_OF_MESSAGES; ++i)
  {
    LOG("Sending Message %d", i + 1);
    if ((rc = MQTTClient_publish(
             mqtt_client,
             telemetry_topic_buffer,
             (int)strlen(telemetry_message_payloads[i]),
             telemetry_message_payloads[i],
             0,
             0,
             NULL))
        != MQTTCLIENT_SUCCESS)
    {
      LOG_ERROR("Failed to publish telemetry message %d, MQTTClient return code %d\n", i + 1, rc);
      exit(rc);
    }
    sleep_for_seconds(TELEMETRY_SEND_INTERVAL);
  }

  return;
}

void disconnect_client_from_iot_hub()
{
  int rc;

  if ((rc = MQTTClient_disconnect(mqtt_client, TIMEOUT_MQTT_DISCONNECT_MS)) != MQTTCLIENT_SUCCESS)
  {
    LOG_ERROR("Failed to disconnect MQTT client: MQTTClient return code %d.", rc);
    exit(rc);
  }

  MQTTClient_destroy(&mqtt_client);

  return;
}

void generate_sas_key()
{
  az_result rc;

  // Create the POSIX expiration time from input hours
  uint64_t sas_duration = get_epoch_expiration_time_from_hours(env_vars.sas_key_duration_minutes);

  // Get the signature which will be signed with the decoded key
  az_span sas_signature = AZ_SPAN_FROM_BUFFER(sas_signature_buffer);
  if (az_failed(
          rc = az_iot_hub_client_sas_get_signature(
              &hub_client, sas_duration, sas_signature, &sas_signature)))
  {
    LOG_ERROR("Could not get the signature for SAS key: az_result return code 0x%04x.", rc);
    exit(rc);
  }

  // Decode the base64 encoded SAS key to use for HMAC signing
  az_span sas_b64_decoded_key = AZ_SPAN_FROM_BUFFER(sas_b64_decoded_key_buffer);
  if (az_failed(
          rc
          = sample_base64_decode(env_vars.hub_sas_key, sas_b64_decoded_key, &sas_b64_decoded_key)))
  {
    LOG_ERROR("Could not decode the SAS key: az_result return code 0x%04x.", rc);
    exit(rc);
  }

  // HMAC-SHA256 sign the signature with the decoded key
  az_span sas_encoded_hmac256_signed_signature
      = AZ_SPAN_FROM_BUFFER(sas_encoded_hmac256_signed_signature_buffer);
  if (az_failed(
          rc = sample_hmac_sha256_sign(
              sas_b64_decoded_key,
              sas_signature,
              sas_encoded_hmac256_signed_signature,
              &sas_encoded_hmac256_signed_signature)))
  {
    LOG_ERROR("Could not sign the signature: az_result return code 0x%04x.", rc);
    exit(rc);
  }

  // base64 encode the result of the HMAC signing
  az_span sas_b64_encoded_hmac256_signed_signature
      = AZ_SPAN_FROM_BUFFER(sas_b64_encoded_hmac256_signed_signature_buffer);
  if (az_failed(
          rc = sample_base64_encode(
              sas_encoded_hmac256_signed_signature,
              sas_b64_encoded_hmac256_signed_signature,
              &sas_b64_encoded_hmac256_signed_signature)))
  {
    LOG_ERROR("Could not base64 encode the password: az_result return code 0x%04x.", rc);
    exit(rc);
  }

  // Get the resulting password, passing the base64 encoded, HMAC signed bytes
  size_t mqtt_password_length;
  if (az_failed(
          rc = az_iot_hub_client_sas_get_password(
              &hub_client,
              sas_b64_encoded_hmac256_signed_signature,
              sas_duration,
              AZ_SPAN_NULL,
              mqtt_password_buffer,
              sizeof(mqtt_password_buffer),
              &mqtt_password_length)))
  {
    LOG_ERROR("Could not get the password: az_result return code 0x%04x.", rc);
    exit(rc);
  }

  return;
}
