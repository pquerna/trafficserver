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

/****************************************************************************

  Async Disk IO operations.

  
  
 ****************************************************************************/
#ifndef _P_AIO_h_
#define _P_AIO_h_

#ifndef INLINE_CC
#undef INK_INLINE
#define INK_INLINE inline
#endif

#include "P_EventSystem.h"
#include "I_AIO.h"

// #define AIO_STATS

#undef  AIO_MODULE_VERSION
#define AIO_MODULE_VERSION        makeModuleVersion(AIO_MODULE_MAJOR_VERSION,\
						    AIO_MODULE_MINOR_VERSION,\
						    PRIVATE_MODULE_HEADER)
struct AIO_Reqs;

struct AIOCallbackInternal:AIOCallback
{
  AIOCallback *first;
  AIO_Reqs *aio_req;
  ink_hrtime sleep_time;
  int io_complete(int event, void *data);
  void AIOCallback_is_an_abstract_class()
  {
  }
  AIOCallbackInternal()
  {
    const size_t to_zero = sizeof(AIOCallbackInternal)
      - (size_t) & (((AIOCallbackInternal *) 0)->aiocb);
    memset((char *) &(this->aiocb), 0, to_zero);
    SET_HANDLER(&AIOCallbackInternal::io_complete);
    // we do a memset() on AIOCallback and AIOCallbackInternal, so it sets all the members to 0
    // coverity[uninit_member]
  }
};

INK_INLINE int
AIOCallback::ok()
{
  return (ink_off_t) aiocb.aio_nbytes == (ink_off_t) aio_result;
};

INK_INLINE int
AIOCallbackInternal::io_complete(int event, void *data)
{
  (void) event;
  (void) data;
  if (!action.cancelled)
    action.continuation->handleEvent(AIO_EVENT_DONE, this);
  return EVENT_DONE;
};

struct AIO_Reqs
{
  Queue<AIOCallback> aio_todo;       /* queue for holding non-http requests */
  Queue<AIOCallback> http_aio_todo;  /* queue for http requests */
  /* Atomic list to temporarily hold the request if the
     lock for a particular queue cannot be acquired */
  InkAtomicList aio_temp_list;
#ifdef INKDISKAIO
  ProxyMutexPtr list_mutex;
#endif
  ink_mutex aio_mutex;
  ink_cond aio_cond;
  int index;                    /* position of this struct in the aio_reqs array */
  volatile int pending;         /* number of outstanding requests on the disk */
  volatile int queued;          /* total number of aio_todo and http_todo requests */
  volatile int filedes;         /* the file descriptor for the requests */
  volatile int requests_queued;
};

#ifdef AIO_STATS
class AIOTestData:public Continuation
{
public:
  int num_req;
  int num_temp;
  int num_queue;
  ink_hrtime start;
    AIOTestData():Continuation(new_ProxyMutex()), num_req(0), num_temp(0), num_queue(0)
  {
    start = ink_get_hrtime();
    SET_HANDLER(&AIOTestData::ink_aio_stats);
  }

  int ink_aio_stats(int event, void *data);
};
#endif


#ifdef INKDISKAIO
#include "inkaio.h"
void initialize_thread_for_diskaio(EThread *);
struct AIOMissEvent:Continuation
{
  AIOCallback *cb;

  int mainEvent(int event, Event * e)
  {
    if (!cb->action.cancelled)
      cb->action.continuation->handleEvent(AIO_EVENT_DONE, cb);
    delete this;
      return EVENT_DONE;
  }

  AIOMissEvent(ProxyMutex * amutex, AIOCallback * acb)
  : Continuation(amutex), cb(acb)
  {
    SET_HANDLER(&AIOMissEvent::mainEvent);
  }
};
static ink_off_t kcb_offset, dm_offset, dmid_offset;
inline INKAIOCB *
get_kcb(EThread * t)
{
  return *((INKAIOCB **) ETHREAD_GET_PTR(t, kcb_offset));
}

inline Continuation *
get_dm(EThread * t)
{
  return *((Continuation **) ETHREAD_GET_PTR(t, dm_offset));
}
inline int
get_dmid(EThread * t)
{
  return *((int *) ETHREAD_GET_PTR(t, dmid_offset));
}
#endif

enum aio_stat_enum
{
  AIO_STAT_READ_PER_SEC,
  AIO_STAT_KB_READ_PER_SEC,
  AIO_STAT_WRITE_PER_SEC,
  AIO_STAT_KB_WRITE_PER_SEC,
  AIO_STAT_COUNT
};
extern RecRawStatBlock *aio_rsb;

#ifdef _WIN32
extern NTIOCompletionPort aio_completion_port;
#endif

#endif
