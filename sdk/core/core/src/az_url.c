// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <az_url.h>

#include <az_http_query.h>

#include <_az_cfg.h>


AZ_NODISCARD az_result az_url_parse(az_span const url, az_url* const out) {
  AZ_CONTRACT_ARG_NOT_NULL(out);

  (void)url;

  return AZ_ERROR_NOT_IMPLEMENTED;
}

/**
 * Read backwards (from right to left) from top-level domain (eg. `.com`) to the lowest subdomain
 * (eg. `www`).
 */
AZ_NODISCARD az_result az_dns_read_domain(az_span* const dns, az_span* const subdomain) {
  AZ_CONTRACT_ARG_NOT_NULL(dns);
  AZ_CONTRACT_ARG_NOT_NULL(subdomain);

  return AZ_ERROR_NOT_IMPLEMENTED;
}
