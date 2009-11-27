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

  SSLUnixNet.h

  This file implements an I/O Processor for network I/O for Unix.
  Contains additions for handling port pairs for RTSP/RTP.

  
 ****************************************************************************/
#ifdef HAVE_LIBSSL
#include "ink_unused.h"   /* MAGIC_EDITING_TAG */
#include <string.h>
#include "P_Net.h"
//
// Global Data
//

SSLNetProcessor ssl_NetProcessor;
NetProcessor & sslNetProcessor = ssl_NetProcessor;

EventType ET_SSL;

typedef int (SSLNetAccept::*SSLNetAcceptHandler) (int, void *);


int
SSLNetProcessor::start(int number_of_ssl_threads)
{
  sslTerminationConfig.startup();
  int err = reconfigure();

  if (err != 0) {
    return -1;
  }

  if (number_of_ssl_threads < 1)
    return -1;

  ET_SSL = eventProcessor.spawn_event_threads(number_of_ssl_threads);
  if (err == 0)
    err = UnixNetProcessor::start();
  return (err);
}


NetAccept *
SSLNetProcessor::createNetAccept()
{
  return ((NetAccept *) NEW(new SSLNetAccept));
}

// Virtual function allows etype
// to be set to ET_SSL for SSLNetProcessor.  Does
// nothing for NetProcessor
void
SSLNetProcessor::setEtype(EventType & etype)
{
  if (etype == ET_NET) {
    etype = ET_SSL;
  }
}

// Virtual function allows the correct
// etype to be used in NetAccept functions (ET_SSL
// or ET_NET). 
EventType
SSLNetAccept::getEtype()
{
  return (ET_SSL);
}

// Functions all THREAD_FREE and THREAD_ALLOC to be performed
// for both SSL and regular NetVConnection transparent to
// accept functions.
UnixNetVConnection *
SSLNetAccept::allocateThread(EThread * t)
{
  return ((UnixNetVConnection *) THREAD_ALLOC(sslNetVCAllocator, t));
}

void
SSLNetAccept::freeThread(UnixNetVConnection * vc, EThread * t)
{
  THREAD_FREE((SSLNetVConnection *) vc, sslNetVCAllocator, t);
}

// Functions all THREAD_FREE and THREAD_ALLOC to be performed
// for both SSL and regular NetVConnection transparent to
// netProcessor connect functions. Yes it looks goofy to
// have them in both places, but it saves a bunch of
// connect code from being duplicated.
UnixNetVConnection *
SSLNetProcessor::allocateThread(EThread * t)
{
  return ((UnixNetVConnection *) THREAD_ALLOC(sslNetVCAllocator, t));
}

void
SSLNetProcessor::freeThread(UnixNetVConnection * vc, EThread * t)
{
  THREAD_FREE((SSLNetVConnection *) vc, sslNetVCAllocator, t);
}

void
SSLNetAccept::init_accept_per_thread()
{
  int i, n;
  if (do_listen(NON_BLOCKING))
    return;
  if (accept_fn == net_accept)
    SET_HANDLER((SSLNetAcceptHandler) & SSLNetAccept::acceptFastEvent);
  else
    SET_HANDLER((SSLNetAcceptHandler) & SSLNetAccept::acceptEvent);
  period = ACCEPT_PERIOD;
  NetAccept *a = this;
  n = eventProcessor.n_threads_for_type[ET_SSL];
  for (i = 0; i < n; i++) {
    if (i < n - 1) {
      a = NEW(new SSLNetAccept);
      *a = *this;
    } else
      a = this;
    EThread *t = eventProcessor.eventthread[ET_SSL][i];

    //added by YTS Team, yamsat
    PollDescriptor *pd = get_PollDescriptor(t);
    struct epoll_data_ptr *eptr;
    eptr = (struct epoll_data_ptr *) xmalloc(sizeof(struct epoll_data_ptr));
    eptr->type = EPOLL_NETACCEPT;
    eptr->data.na = (NetAccept *) a;

#if defined(USE_EPOLL)
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = eptr;

    if (epoll_ctl(pd->epoll_fd, EPOLL_CTL_ADD, a->server.fd, &ev) < 0) {
      printf("error in epoll_ctl\n");
    }
#elif defined(USE_KQUEUE)
    struct kevent ev;
    EV_SET(&ev, a->server.fd, EVFILT_READ, EV_ADD, 0, 0, eptr);
    if (kevent(pd->kqueue_fd, &ev, 1, NULL, 0, NULL) < 0) {
      printf("error in kevent\n");
    }
#else
#error port me
#endif

    a->mutex = get_NetHandler(t)->mutex;
    t->schedule_every(a, period, etype);
  }
}

SSLNetProcessor::~SSLNetProcessor()
{
  cleanup();
}


#endif
