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

  UnixPollDescriptor.h

  
*****************************************************************************/
#ifndef __P_UNIXPOLLDESCRIPTOR_H__
#define __P_UNIXPOLLDESCRIPTOR_H__

typedef struct pollfd Pollfd;

#define POLL_DESCRIPTOR_SIZE 32768

struct PollDescriptor
{
  int result;                   // result of poll
  struct ev_loop *eloop;
  int nfds;                     // actual number
  int seq_num;                  // sequence number
  Pollfd pfd[POLL_DESCRIPTOR_SIZE];

  bool empty()
  {
    return nfds == 0;
  }
  void reset()
  {
    nfds = 0;
  }
  Pollfd *alloc()
  {
    return &pfd[nfds++];
  }

  PollDescriptor *init()
  {
    result = 0;
    nfds = 0;
    seq_num = 0;
    eloop = ::ev_loop_new(EVBACKEND_EPOLL|EVBACKEND_KQUEUE|
                          EVBACKEND_DEVPOL|EVBACKEND_PORT);
    memset(pfd, 0, sizeof(pfd));
    return this;
  }
  PollDescriptor() {
    init();
  }
};

#endif
