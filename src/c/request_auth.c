/*
 * Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "service.h"
#include "request_auth.h"

#include <string.h>
#include <stdio.h>

#include <microhttpd.h>

bool request_is_authenticated (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  bool valid_jwt = false;

  // Calling request_is_authenticated requires that the Authorization header is present on the request
  // and that it is a Bearer token and that the JWT validates with Vault; otherwise authorization fails.

  printf("Authenticating request\n");
  if (req->authorization_header_value != NULL && 
    (strncasecmp("Bearer ", req->authorization_header_value, strlen("Bearer ")) == 0))
  {
    const char * jwt = req->authorization_header_value + strlen("Bearer ");
    printf("Checking JWT %s\n", jwt);
    valid_jwt = edgex_secrets_is_jwt_valid (svc->secretstore, jwt);
  }

  if (!valid_jwt)
  {
    printf("Unauthorized\n");
    reply->code = MHD_HTTP_UNAUTHORIZED;
    // Caller handler is expected to cease further processing
  }

  return valid_jwt;
}

