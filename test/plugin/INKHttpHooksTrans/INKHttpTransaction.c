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


#include "InkAPI.h"

#if 0

/* HTTP transactions */

/* Read from cache as soon as write from cache completes, i.e., we
 * should be able to call INKHttpTxnCachedReqGet during the initial req.
 * from a cache miss.   
 * INK_HTTP_SEND_RESPONSE_HDR_HOOK 
 * INH_HTTP_TXN_CLOSE_HOOK
 * 
 * If above fails, i.e., cannot call this API on a cache miss even after 
 * a write to cache has completed then call only on a cache hit.
*/
inkapi int INKHttpTxnCachedReqGet(INKHttpTxn txnp, INKMBuffer * bufp, INKMLoc * offset);
inkapi int INKHttpTxnCachedRespGet(INKHttpTxn txnp, INKMBuffer * bufp, INKMLoc * offset);


/* Call as soon as we have a transaction: INK_HTTP_TXN_START
Client port */
inkapi int INKHttpTxnClientIncomingPortGet(INKHttpTxn txnp);

/* Call as soon as we have a transaction: INK_HTTP_TXN_START
/* Client IP for a transaction (not incoming) */
inkapi unsigned int INKHttpTxnClientIPGet(INKHttpTxn txnp);

/* Non-cached, 
 * get client req after recieving INK_HTTP_READ_REQUEST_HDR_HOOK 
*/
inkapi int INKHttpTxnClientReqGet(INKHttpTxn txnp, INKMBuffer * bufp, INKMLoc * offset);
/* Non-cached, 
 * get "client" resp after recieving INK_HTTP_READ_RESPONSE_HDR_HOOK 
*/
inkapi int INKHttpTxnClientRespGet(INKHttpTxn txnp, INKMBuffer * bufp, INKMLoc * offset);

/* This is a response to a client (req), which will be executed after reciept
 * of: 
 *   1. INK_HTTP_OS_DNS_HOOK fails for some reason to do the translation
 *   2. INK_HTTP_READ_RESPONSE_HDR_HOOK origin server replied with some 
 *      type of error. 
 *   3. An error is possible at any point in HTTP processing.
*/
inkapi void INKHttpTxnErrorBodySet(INKHttpTxn txnp, char *buf, int buflength, char *mimetype);

/* DONE */
inkapi void INKHttpTxnHookAdd(INKHttpTxn txnp, INKHttpHookID id, INKCont contp);

/* Origin Server (destination) or Parent IP */
inkapi unsigned int INKHttpTxnNextHopIPGet(INKHttpTxn txnp);

/* Results if parent proxy not enabled, results if parent proxy is enabled
*/
inkapi void INKHttpTxnParentProxyGet(INKHttpTxn txnp, char **hostname, int *port);
/* hostname is copied into the txn and is deletable upon return
*/
inkapi void INKHttpTxnParentProxySet(INKHttpTxn txnp, char *hostname, int port);

/*  */
inkapi void INKHttpTxnReenable(INKHttpTxn txnp, INKEvent event);

/* 
 * Origin Server IP
 * DETAILS: zero before INK_HTTP_OS_DNS_HOOK 
 *          IP addr after INK_HTTP_OS_DNS_HOOK 
*/
inkapi unsigned int INKHttpTxnServerIPGet(INKHttpTxn txnp);

/* Need a transaction and a request: earliest point is 
 * not INK_HTTP_TXN_START_HOOK but INK_HTTP_READ_REQUEST_HDR_HOOK
 * process the request.
*/
inkapi int INKHttpTxnServerReqGet(INKHttpTxn txnp, INKMBuffer * bufp, INKMLoc * offset);

/* Need a transaction and a server response: earliest point is 
 * INK_HTTP_READ_RESPONSE_HDR_HOOK, then process the response. 
*/
inkapi int INKHttpTxnServerRespGet(INKHttpTxn txnp, INKMBuffer * bufp, INKMLoc * offset);


/* Call this as soon as a transaction has been created and retrieve the session
 * and do some processing.
*/
inkapi INKHttpSsn INKHttpTxnSsnGet(INKHttpTxn txnp);

/* Call this before a write to the cache is done, else too late for 
 * current txn:  
 * INK_HTTP_READ_RESPONSE_HDR_HOOK. 
 * default: transformed copy written to cache 
 * on == non-zero,	cache_transformed = true (default)
 * on == zero,		cache_transformed = false
*/
inkapi void INKHttpTxnTransformedRespCache(INKHttpTxn txnp, int on);

/* INK_HTTP_READ_RESPONSE_HOOK & INK_HTTP_RESPONSE_HDR_HOOK
 * Get the transform resp header from the HTTP transaction. 
 * re = 0 if dne, else 1.
*/
inkapi int INKHttpTxnTransformRespGet(INKHttpTxn txnp, INKMBuffer * bufp, INKMLoc * offset);

/* Call this before a write to the cache is done, else too late for 
 * current txn:  
 * INK_HTTP_READ_RESPONSE_HDR_HOOK. 
 * default: un-transformed copy not written to cache
 * on == non-zero,	cache_untransformed = true
 * on == zero,		cache_untransformed = false (default)
*/
inkapi void INKHttpTxnUntransformedRespCache(INKHttpTxn txnp, int on);

#endif

static int
handle_HTTP_SEND_RESPONSE_HDR(INKCont contp, INKEvent event, void *eData)
{
  INKMBuffer buffer;
  INKMLoc buffOffset;
  INKHttpTxn txnp = (INKHttpTxn) contp;
  int re = 0, err = 0;
  re = INKHttpTxnCachedReqGet(txnp, &buffer, &buffOffset);
  if (re) {
    INKDebug("INKHttpTransaction", "INKHttpTxnCachedReqGet(): INK_EVENT_HTTP_SEND_RESPONSE_HDR, and txnp set\n");
    /* Display all buffer contents */

  } else {
    INKDebug("INKHttpTransaction", "INKHttpTxnCachedReqGet(): Failed.");
    err++;
  }

/* 
INKHttpTxnCachedRespGet (INKHttpTxn txnp, INKMBuffer *bufp, INKMLoc *offset);
*/
/* Display buffer contents */

/* Other API calls that should process this event */
/* Display results */

  return err;
}

static int
handle_READ_REQUEST_HDR(INKCont cont, INKEvent event, void *eData)
{
  int err = 0;
#if 0
/* Non-cached, 
 * get client req after recieving INK_HTTP_READ_REQUEST_HDR_HOOK 
*/
  inkapi int INKHttpTxnClientReqGet(INKHttpTxn txnp, INKMBuffer * bufp, INKMLoc * offset);
#endif
  return err;
}

static int
handle_READ_RESPONSE_HDR(INKCont contp, INKEvent event, void *eData)
{
  int err = 0;
#if 0
  /* Non-cached, 
   * get "client" resp after recieving INK_HTTP_READ_RESPONSE_HDR_HOOK 
   */
  inkapi int INKHttpTxnClientRespGet(INKHttpTxn txnp, INKMBuffer * bufp, INKMLoc * offset);
#endif
  return err;
}

static int
INKHttpTransaction(INKCont contp, INKEvent event, void *eData)
{
  INKHttpSsn ssnp = (INKHttpSsn) eData;
  INKHttpTxn txnp = (INKHttpTxn) eData;

  switch (event) {

  case INK_EVENT_HTTP_SEND_RESPONSE_HDR:
    handle_HTTP_SEND_RESPONSE_HDR(contp, event, eData);
    INKHttpTxnReenable(txnp, INK_EVENT_HTTP_CONTINUE);
    break;

  case INK_EVENT_HTTP_READ_REQUEST_HDR:
    handle_READ_REQUEST_HDR(contp, event, eData);
    INKHttpTxnReenable(txnp, INK_EVENT_HTTP_CONTINUE);
    break;

  case INK_EVENT_HTTP_READ_RESPONSE_HDR:
    handle_READ_RESPONSE_HDR(contp, event, eData);
    INKHttpTxnReenable(txnp, INK_EVENT_HTTP_CONTINUE);
    break;
  default:
    break;
  }
}

void
INKPluginInit(int argc, const char *argv[])
{
  INKCont contp = INKContCreate(INKHttpTransaction, NULL);
  INKHttpHookAdd(INK_HTTP_SSN_START_HOOK, contp);
}
