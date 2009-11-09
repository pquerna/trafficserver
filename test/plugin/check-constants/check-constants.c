/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

/*****************************************************************************
 * check-constants
 *
 * Description:	This sample plugins simply compares all the constants 
 *				values in the curent InkAPI.h against the values as they 
 *				exist in the SDK 2.0 FCS InkAPI.h, and prints them out if they
 *				happen to differ.
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>

#if !defined (_WIN32)
#	include <unistd.h>
#else
#	include <windows.h>
#endif

#include "InkAPI.h"

#define PRINT_DIFF(const_name, current_const, orig_const ) \
{ \
	if (current_const != orig_const ) { \
		file_changed = 1; \
		printf ("%s: Original Value = %d; New Value = %d \n", const_name, orig_const, current_const); \
	} \
}

#define ORIG_INK_MAX_USER_NAME_LEN 256

int file_changed = 0;

typedef enum
{
  ORIG_INK_PARSE_ERROR = -1,
  ORIG_INK_PARSE_DONE = 0,
  ORIG_INK_PARSE_OK = 1,
  ORIG_INK_PARSE_CONT = 2
} ORIG_INKParseResult;

typedef enum
{
  ORIG_INK_HTTP_TYPE_UNKNOWN,
  ORIG_INK_HTTP_TYPE_REQUEST,
  ORIG_INK_HTTP_TYPE_RESPONSE
} ORIG_INKHttpType;

typedef enum
{
  ORIG_INK_HTTP_STATUS_NONE = 0,

  ORIG_INK_HTTP_STATUS_CONTINUE = 100,
  ORIG_INK_HTTP_STATUS_SWITCHING_PROTOCOL = 101,

  ORIG_INK_HTTP_STATUS_OK = 200,
  ORIG_INK_HTTP_STATUS_CREATED = 201,
  ORIG_INK_HTTP_STATUS_ACCEPTED = 202,
  ORIG_INK_HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION = 203,
  ORIG_INK_HTTP_STATUS_NO_CONTENT = 204,
  ORIG_INK_HTTP_STATUS_RESET_CONTENT = 205,
  ORIG_INK_HTTP_STATUS_PARTIAL_CONTENT = 206,

  ORIG_INK_HTTP_STATUS_MULTIPLE_CHOICES = 300,
  ORIG_INK_HTTP_STATUS_MOVED_PERMANENTLY = 301,
  ORIG_INK_HTTP_STATUS_MOVED_TEMPORARILY = 302,
  ORIG_INK_HTTP_STATUS_SEE_OTHER = 303,
  ORIG_INK_HTTP_STATUS_NOT_MODIFIED = 304,
  ORIG_INK_HTTP_STATUS_USE_PROXY = 305,

  ORIG_INK_HTTP_STATUS_BAD_REQUEST = 400,
  ORIG_INK_HTTP_STATUS_UNAUTHORIZED = 401,
  ORIG_INK_HTTP_STATUS_PAYMENT_REQUIRED = 402,
  ORIG_INK_HTTP_STATUS_FORBIDDEN = 403,
  ORIG_INK_HTTP_STATUS_NOT_FOUND = 404,
  ORIG_INK_HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
  ORIG_INK_HTTP_STATUS_NOT_ACCEPTABLE = 406,
  ORIG_INK_HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED = 407,
  ORIG_INK_HTTP_STATUS_REQUEST_TIMEOUT = 408,
  ORIG_INK_HTTP_STATUS_CONFLICT = 409,
  ORIG_INK_HTTP_STATUS_GONE = 410,
  ORIG_INK_HTTP_STATUS_LENGTH_REQUIRED = 411,
  ORIG_INK_HTTP_STATUS_PRECONDITION_FAILED = 412,
  ORIG_INK_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE = 413,
  ORIG_INK_HTTP_STATUS_REQUEST_URI_TOO_LONG = 414,
  ORIG_INK_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE = 415,

  ORIG_INK_HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
  ORIG_INK_HTTP_STATUS_NOT_IMPLEMENTED = 501,
  ORIG_INK_HTTP_STATUS_BAD_GATEWAY = 502,
  ORIG_INK_HTTP_STATUS_SERVICE_UNAVAILABLE = 503,
  ORIG_INK_HTTP_STATUS_GATEWAY_TIMEOUT = 504,
  ORIG_INK_HTTP_STATUS_HTTPVER_NOT_SUPPORTED = 505
} ORIG_INKHttpStatus;

typedef enum
{
  ORIG_INK_HTTP_READ_REQUEST_HDR_HOOK,
  ORIG_INK_HTTP_OS_DNS_HOOK,
  ORIG_INK_HTTP_SEND_REQUEST_HDR_HOOK,
  ORIG_INK_HTTP_READ_CACHE_HDR_HOOK,
  ORIG_INK_HTTP_READ_RESPONSE_HDR_HOOK,
  ORIG_INK_HTTP_SEND_RESPONSE_HDR_HOOK,
  ORIG_INK_HTTP_REQUEST_TRANSFORM_HOOK,
  ORIG_INK_HTTP_RESPONSE_TRANSFORM_HOOK,
  ORIG_INK_HTTP_SELECT_ALT_HOOK,
  ORIG_INK_HTTP_TXN_START_HOOK,
  ORIG_INK_HTTP_TXN_CLOSE_HOOK,
  ORIG_INK_HTTP_SSN_START_HOOK,
  ORIG_INK_HTTP_SSN_CLOSE_HOOK,

  ORIG_INK_HTTP_LAST_HOOK
} ORIG_INKHttpHookID;

typedef enum
{
  ORIG_INK_EVENT_NONE = 0,
  ORIG_INK_EVENT_IMMEDIATE = 1,
  ORIG_INK_EVENT_TIMEOUT = 2,
  ORIG_INK_EVENT_ERROR = 3,
  ORIG_INK_EVENT_CONTINUE = 4,

  ORIG_INK_EVENT_VCONN_READ_READY = 100,
  ORIG_INK_EVENT_VCONN_WRITE_READY = 101,
  ORIG_INK_EVENT_VCONN_READ_COMPLETE = 102,
  ORIG_INK_EVENT_VCONN_WRITE_COMPLETE = 103,
  ORIG_INK_EVENT_VCONN_EOS = 104,

  ORIG_INK_EVENT_NET_CONNECT = 200,
  ORIG_INK_EVENT_NET_CONNECT_FAILED = 201,

  ORIG_INK_EVENT_HTTP_CONTINUE = 60000,
  ORIG_INK_EVENT_HTTP_ERROR = 60001,
  ORIG_INK_EVENT_HTTP_READ_REQUEST_HDR = 60002,
  ORIG_INK_EVENT_HTTP_OS_DNS = 60003,
  ORIG_INK_EVENT_HTTP_SEND_REQUEST_HDR = 60004,
  ORIG_INK_EVENT_HTTP_READ_CACHE_HDR = 60005,
  ORIG_INK_EVENT_HTTP_READ_RESPONSE_HDR = 60006,
  ORIG_INK_EVENT_HTTP_SEND_RESPONSE_HDR = 60007,
  ORIG_INK_EVENT_HTTP_REQUEST_TRANSFORM = 60008,
  ORIG_INK_EVENT_HTTP_RESPONSE_TRANSFORM = 60009,
  ORIG_INK_EVENT_HTTP_SELECT_ALT = 60010,
  ORIG_INK_EVENT_HTTP_TXN_START = 60011,
  ORIG_INK_EVENT_HTTP_TXN_CLOSE = 60012,
  ORIG_INK_EVENT_HTTP_SSN_START = 60013,
  ORIG_INK_EVENT_HTTP_SSN_CLOSE = 60014,

  ORIG_INK_EVENT_MGMT_UPDATE = 60100
} ORIG_INKEvent;

typedef enum
{
  ORIG_INK_DATA_ALLOCATE,
  ORIG_INK_DATA_MALLOCED,
  ORIG_INK_DATA_CONSTANT
} ORIG_INKIOBufferDataFlags;

typedef enum
{
  ORIG_INK_VC_CLOSE_ABORT = -1,
  ORIG_INK_VC_CLOSE_NORMAL = 1
} ORIG_INKVConnCloseFlags;

typedef enum
{
  ORIG_INK_SDK_VERSION_1_0 = 0,
  ORIG_INK_SDK_VERSION_1_1,
  ORIG_INK_SDK_VERSION_2_0
} ORIG_INKSDKVersion;



void
INKPluginInit(int argc, const char *argv[])
{

  PRINT_DIFF("INK_MAX_USER_NAME_LEN", INK_MAX_USER_NAME_LEN, ORIG_INK_MAX_USER_NAME_LEN);

  PRINT_DIFF("INK_PARSE_ERROR", INK_PARSE_ERROR, ORIG_INK_PARSE_ERROR);
  PRINT_DIFF("INK_PARSE_DONE", INK_PARSE_DONE, ORIG_INK_PARSE_DONE);
  PRINT_DIFF("INK_PARSE_OK", INK_PARSE_OK, ORIG_INK_PARSE_OK);
  PRINT_DIFF("INK_PARSE_CONT", INK_PARSE_CONT, ORIG_INK_PARSE_CONT);

  PRINT_DIFF("INK_HTTP_STATUS_NONE", INK_HTTP_STATUS_NONE, ORIG_INK_HTTP_STATUS_NONE);
  PRINT_DIFF("INK_HTTP_STATUS_CONTINUE", INK_HTTP_STATUS_CONTINUE, ORIG_INK_HTTP_STATUS_CONTINUE);
  PRINT_DIFF("INK_HTTP_STATUS_SWITCHING_PROTOCOL", INK_HTTP_STATUS_SWITCHING_PROTOCOL,
             ORIG_INK_HTTP_STATUS_SWITCHING_PROTOCOL);
  PRINT_DIFF("INK_HTTP_STATUS_OK", INK_HTTP_STATUS_OK, ORIG_INK_HTTP_STATUS_OK);
  PRINT_DIFF("INK_HTTP_STATUS_CREATED", INK_HTTP_STATUS_CREATED, ORIG_INK_HTTP_STATUS_CREATED);


  PRINT_DIFF("INK_HTTP_STATUS_ACCEPTED", INK_HTTP_STATUS_ACCEPTED, ORIG_INK_HTTP_STATUS_ACCEPTED);
  PRINT_DIFF("INK_HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION", INK_HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION,
             ORIG_INK_HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION);
  PRINT_DIFF("INK_HTTP_STATUS_NO_CONTENT", INK_HTTP_STATUS_NO_CONTENT, ORIG_INK_HTTP_STATUS_NO_CONTENT);
  PRINT_DIFF("INK_HTTP_STATUS_RESET_CONTENT", INK_HTTP_STATUS_RESET_CONTENT, ORIG_INK_HTTP_STATUS_RESET_CONTENT);
  PRINT_DIFF("INK_HTTP_STATUS_PARTIAL_CONTENT", INK_HTTP_STATUS_PARTIAL_CONTENT, ORIG_INK_HTTP_STATUS_PARTIAL_CONTENT);

  PRINT_DIFF("INK_HTTP_STATUS_MULTIPLE_CHOICES", INK_HTTP_STATUS_MULTIPLE_CHOICES,
             ORIG_INK_HTTP_STATUS_MULTIPLE_CHOICES);
  PRINT_DIFF("INK_HTTP_STATUS_MOVED_PERMANENTLY", INK_HTTP_STATUS_MOVED_PERMANENTLY,
             ORIG_INK_HTTP_STATUS_MOVED_PERMANENTLY);
  PRINT_DIFF("INK_HTTP_STATUS_MOVED_TEMPORARILY", INK_HTTP_STATUS_MOVED_TEMPORARILY,
             ORIG_INK_HTTP_STATUS_MOVED_TEMPORARILY);
  PRINT_DIFF("INK_HTTP_STATUS_SEE_OTHER", INK_HTTP_STATUS_SEE_OTHER, ORIG_INK_HTTP_STATUS_SEE_OTHER);
  PRINT_DIFF("INK_HTTP_STATUS_NOT_MODIFIED", INK_HTTP_STATUS_NOT_MODIFIED, ORIG_INK_HTTP_STATUS_NOT_MODIFIED);
  PRINT_DIFF("INK_HTTP_STATUS_USE_PROXY", INK_HTTP_STATUS_USE_PROXY, ORIG_INK_HTTP_STATUS_USE_PROXY);

  PRINT_DIFF("INK_HTTP_STATUS_BAD_REQUEST", INK_HTTP_STATUS_BAD_REQUEST, ORIG_INK_HTTP_STATUS_BAD_REQUEST);
  PRINT_DIFF("INK_HTTP_STATUS_UNAUTHORIZED", INK_HTTP_STATUS_UNAUTHORIZED, ORIG_INK_HTTP_STATUS_UNAUTHORIZED);
  PRINT_DIFF("INK_HTTP_STATUS_FORBIDDEN", INK_HTTP_STATUS_FORBIDDEN, ORIG_INK_HTTP_STATUS_FORBIDDEN);
  PRINT_DIFF("INK_HTTP_STATUS_NOT_FOUND", INK_HTTP_STATUS_NOT_FOUND, ORIG_INK_HTTP_STATUS_NOT_FOUND);

  PRINT_DIFF("INK_HTTP_STATUS_METHOD_NOT_ALLOWED", INK_HTTP_STATUS_METHOD_NOT_ALLOWED,
             ORIG_INK_HTTP_STATUS_METHOD_NOT_ALLOWED);
  PRINT_DIFF("INK_HTTP_STATUS_NOT_ACCEPTABLE", INK_HTTP_STATUS_NOT_ACCEPTABLE, ORIG_INK_HTTP_STATUS_NOT_ACCEPTABLE);
  PRINT_DIFF("INK_HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED", INK_HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED,
             ORIG_INK_HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED);
  PRINT_DIFF("INK_HTTP_STATUS_REQUEST_TIMEOUT", INK_HTTP_STATUS_REQUEST_TIMEOUT, ORIG_INK_HTTP_STATUS_REQUEST_TIMEOUT);
  PRINT_DIFF("INK_HTTP_STATUS_CONFLICT", INK_HTTP_STATUS_CONFLICT, ORIG_INK_HTTP_STATUS_CONFLICT);
  PRINT_DIFF("INK_HTTP_STATUS_GONE", INK_HTTP_STATUS_GONE, ORIG_INK_HTTP_STATUS_GONE);
  PRINT_DIFF("INK_HTTP_STATUS_PRECONDITION_FAILED", INK_HTTP_STATUS_PRECONDITION_FAILED,
             ORIG_INK_HTTP_STATUS_PRECONDITION_FAILED);
  PRINT_DIFF("INK_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE", INK_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE,
             ORIG_INK_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE);
  PRINT_DIFF("INK_HTTP_STATUS_REQUEST_URI_TOO_LONG", INK_HTTP_STATUS_REQUEST_URI_TOO_LONG,
             ORIG_INK_HTTP_STATUS_REQUEST_URI_TOO_LONG);
  PRINT_DIFF("INK_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE", INK_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE,
             ORIG_INK_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE);


  PRINT_DIFF("INK_HTTP_STATUS_INTERNAL_SERVER_ERROR", INK_HTTP_STATUS_INTERNAL_SERVER_ERROR,
             ORIG_INK_HTTP_STATUS_INTERNAL_SERVER_ERROR);
  PRINT_DIFF("INK_HTTP_STATUS_NOT_IMPLEMENTED", INK_HTTP_STATUS_NOT_IMPLEMENTED, ORIG_INK_HTTP_STATUS_NOT_IMPLEMENTED);
  PRINT_DIFF("INK_HTTP_STATUS_BAD_GATEWAY", INK_HTTP_STATUS_BAD_GATEWAY, ORIG_INK_HTTP_STATUS_BAD_GATEWAY);
  PRINT_DIFF("INK_HTTP_STATUS_GATEWAY_TIMEOUT", INK_HTTP_STATUS_GATEWAY_TIMEOUT, ORIG_INK_HTTP_STATUS_GATEWAY_TIMEOUT);
  PRINT_DIFF("INK_HTTP_STATUS_HTTPVER_NOT_SUPPORTED", INK_HTTP_STATUS_HTTPVER_NOT_SUPPORTED,
             ORIG_INK_HTTP_STATUS_HTTPVER_NOT_SUPPORTED);


  PRINT_DIFF("INK_HTTP_READ_REQUEST_HDR_HOOK", INK_HTTP_READ_REQUEST_HDR_HOOK, ORIG_INK_HTTP_READ_REQUEST_HDR_HOOK);
  PRINT_DIFF("INK_HTTP_OS_DNS_HOOK", INK_HTTP_OS_DNS_HOOK, ORIG_INK_HTTP_OS_DNS_HOOK);
  PRINT_DIFF("INK_HTTP_SEND_REQUEST_HDR_HOOK", INK_HTTP_SEND_REQUEST_HDR_HOOK, ORIG_INK_HTTP_SEND_REQUEST_HDR_HOOK);
  PRINT_DIFF("INK_HTTP_READ_RESPONSE_HDR_HOOK", INK_HTTP_READ_RESPONSE_HDR_HOOK, ORIG_INK_HTTP_READ_RESPONSE_HDR_HOOK);
  PRINT_DIFF("INK_HTTP_SEND_RESPONSE_HDR_HOOK", INK_HTTP_SEND_RESPONSE_HDR_HOOK, ORIG_INK_HTTP_SEND_RESPONSE_HDR_HOOK);
  PRINT_DIFF("INK_HTTP_REQUEST_TRANSFORM_HOOK", INK_HTTP_REQUEST_TRANSFORM_HOOK, ORIG_INK_HTTP_REQUEST_TRANSFORM_HOOK);
  PRINT_DIFF("INK_HTTP_RESPONSE_TRANSFORM_HOOK", INK_HTTP_RESPONSE_TRANSFORM_HOOK,
             ORIG_INK_HTTP_RESPONSE_TRANSFORM_HOOK);
  PRINT_DIFF("INK_HTTP_SELECT_ALT_HOOK", INK_HTTP_SELECT_ALT_HOOK, ORIG_INK_HTTP_SELECT_ALT_HOOK);
  PRINT_DIFF("INK_HTTP_TXN_START_HOOK", INK_HTTP_TXN_START_HOOK, ORIG_INK_HTTP_TXN_START_HOOK);
  PRINT_DIFF("INK_HTTP_TXN_CLOSE_HOOK", INK_HTTP_TXN_CLOSE_HOOK, ORIG_INK_HTTP_TXN_CLOSE_HOOK);
  PRINT_DIFF("INK_HTTP_SSN_START_HOOK", INK_HTTP_SSN_START_HOOK, ORIG_INK_HTTP_SSN_START_HOOK);
  PRINT_DIFF("INK_HTTP_SSN_CLOSE_HOOK", INK_HTTP_SSN_CLOSE_HOOK, ORIG_INK_HTTP_SSN_CLOSE_HOOK);
  PRINT_DIFF("INK_HTTP_LAST_HOOK", INK_HTTP_LAST_HOOK, ORIG_INK_HTTP_LAST_HOOK);

  PRINT_DIFF("INK_EVENT_NONE", INK_EVENT_NONE, ORIG_INK_EVENT_NONE);
  PRINT_DIFF("INK_EVENT_IMMEDIATE", INK_EVENT_IMMEDIATE, ORIG_INK_EVENT_IMMEDIATE);
  PRINT_DIFF("INK_EVENT_TIMEOUT", INK_EVENT_TIMEOUT, ORIG_INK_EVENT_TIMEOUT);
  PRINT_DIFF("INK_EVENT_ERROR", INK_EVENT_ERROR, ORIG_INK_EVENT_ERROR);
  PRINT_DIFF("INK_EVENT_CONTINUE", INK_EVENT_CONTINUE, ORIG_INK_EVENT_CONTINUE);
  PRINT_DIFF("INK_EVENT_VCONN_READ_READY", INK_EVENT_VCONN_READ_READY, ORIG_INK_EVENT_VCONN_READ_READY);
  PRINT_DIFF("INK_EVENT_VCONN_WRITE_READY", INK_EVENT_VCONN_WRITE_READY, ORIG_INK_EVENT_VCONN_WRITE_READY);
  PRINT_DIFF("INK_EVENT_VCONN_READ_COMPLETE", INK_EVENT_VCONN_READ_COMPLETE, ORIG_INK_EVENT_VCONN_READ_COMPLETE);
  PRINT_DIFF("INK_EVENT_VCONN_WRITE_COMPLETE", INK_EVENT_VCONN_WRITE_COMPLETE, ORIG_INK_EVENT_VCONN_WRITE_COMPLETE);
  PRINT_DIFF("INK_EVENT_VCONN_EOS", INK_EVENT_VCONN_EOS, ORIG_INK_EVENT_VCONN_EOS);
  PRINT_DIFF("INK_EVENT_NET_CONNECT", INK_EVENT_NET_CONNECT, ORIG_INK_EVENT_NET_CONNECT);
  PRINT_DIFF("INK_EVENT_NET_CONNECT_FAILED", INK_EVENT_NET_CONNECT_FAILED, ORIG_INK_EVENT_NET_CONNECT_FAILED);
  PRINT_DIFF("INK_EVENT_HTTP_CONTINUE", INK_EVENT_HTTP_CONTINUE, ORIG_INK_EVENT_HTTP_CONTINUE);
  PRINT_DIFF("INK_EVENT_HTTP_ERROR", INK_EVENT_HTTP_ERROR, ORIG_INK_EVENT_HTTP_ERROR);
  PRINT_DIFF("INK_EVENT_HTTP_READ_REQUEST_HDR", INK_EVENT_HTTP_READ_REQUEST_HDR, ORIG_INK_EVENT_HTTP_READ_REQUEST_HDR);
  PRINT_DIFF("INK_EVENT_HTTP_OS_DNS", INK_EVENT_HTTP_OS_DNS, ORIG_INK_EVENT_HTTP_OS_DNS);
  PRINT_DIFF("INK_EVENT_HTTP_SEND_REQUEST_HDR", INK_EVENT_HTTP_SEND_REQUEST_HDR, ORIG_INK_EVENT_HTTP_SEND_REQUEST_HDR);
  PRINT_DIFF("INK_EVENT_HTTP_READ_CACHE_HDR", INK_EVENT_HTTP_READ_CACHE_HDR, ORIG_INK_EVENT_HTTP_READ_CACHE_HDR);
  PRINT_DIFF("INK_EVENT_HTTP_READ_RESPONSE_HDR", INK_EVENT_HTTP_READ_RESPONSE_HDR,
             ORIG_INK_EVENT_HTTP_READ_RESPONSE_HDR);
  PRINT_DIFF("INK_EVENT_HTTP_SEND_RESPONSE_HDR", INK_EVENT_HTTP_SEND_RESPONSE_HDR,
             ORIG_INK_EVENT_HTTP_SEND_RESPONSE_HDR);
  PRINT_DIFF("INK_EVENT_HTTP_REQUEST_TRANSFORM", INK_EVENT_HTTP_REQUEST_TRANSFORM,
             ORIG_INK_EVENT_HTTP_REQUEST_TRANSFORM);
  PRINT_DIFF("INK_EVENT_HTTP_RESPONSE_TRANSFORM", INK_EVENT_HTTP_RESPONSE_TRANSFORM,
             ORIG_INK_EVENT_HTTP_RESPONSE_TRANSFORM);
  PRINT_DIFF("INK_EVENT_HTTP_SELECT_ALT", INK_EVENT_HTTP_SELECT_ALT, ORIG_INK_EVENT_HTTP_SELECT_ALT);
  PRINT_DIFF("INK_EVENT_HTTP_TXN_START", INK_EVENT_HTTP_TXN_START, ORIG_INK_EVENT_HTTP_TXN_START);
  PRINT_DIFF("INK_EVENT_HTTP_TXN_CLOSE", INK_EVENT_HTTP_TXN_CLOSE, ORIG_INK_EVENT_HTTP_TXN_CLOSE);
  PRINT_DIFF("INK_EVENT_HTTP_SSN_START", INK_EVENT_HTTP_SSN_START, ORIG_INK_EVENT_HTTP_SSN_START);
  PRINT_DIFF("INK_EVENT_HTTP_SSN_CLOSE", INK_EVENT_HTTP_SSN_CLOSE, ORIG_INK_EVENT_HTTP_SSN_CLOSE);
  PRINT_DIFF("INK_EVENT_MGMT_UPDATE", INK_EVENT_MGMT_UPDATE, ORIG_INK_EVENT_MGMT_UPDATE);

  PRINT_DIFF("INK_EVENT_MGMT_UPDATE", INK_EVENT_MGMT_UPDATE, ORIG_INK_EVENT_MGMT_UPDATE);

  PRINT_DIFF("INK_DATA_ALLOCATE", INK_DATA_ALLOCATE, ORIG_INK_DATA_ALLOCATE);
  PRINT_DIFF("INK_DATA_MALLOCED", INK_DATA_MALLOCED, ORIG_INK_DATA_MALLOCED);
  PRINT_DIFF("INK_DATA_CONSTANT", INK_DATA_CONSTANT, ORIG_INK_DATA_CONSTANT);

  PRINT_DIFF("INK_VC_CLOSE_ABORT", INK_VC_CLOSE_ABORT, ORIG_INK_VC_CLOSE_ABORT);
  PRINT_DIFF("INK_VC_CLOSE_NORMAL", INK_VC_CLOSE_NORMAL, ORIG_INK_VC_CLOSE_NORMAL);

  PRINT_DIFF("INK_SDK_VERSION_1_0", INK_SDK_VERSION_1_0, ORIG_INK_SDK_VERSION_1_0);

  if (file_changed) {
    printf("\n***************************************************************************************");
    printf("\ncheck-constants: W A R N I N G: some of the existing constants values have changed ....");
    printf("\n***************************************************************************************");
  } else {
    printf("\n************************");
    printf("\ncheck-constants: O K	...");
    printf("\n************************");
  }
}