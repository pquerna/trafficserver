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

#ifndef _MGMT_SOCKET_H_
#define _MGMT_SOCKET_H_

#include "ink_platform.h"

//-------------------------------------------------------------------------
// defines
//-------------------------------------------------------------------------

#define MGMT_MAX_TRANSIENT_ERRORS 64

//-------------------------------------------------------------------------
// transient_error
//-------------------------------------------------------------------------

inline bool
mgmt_transient_error()
{
  bool transient = false;
  transient = (errno == EINTR);
#ifdef ENOMEM
  transient = transient || (errno == ENOMEM);
#endif
#ifdef ENOBUF
  transient = transient || (errno == ENOBUF);
#endif
  return transient;
}

//-------------------------------------------------------------------------
// system calls (based on implementation from UnixSocketManager)
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// mgmt_accept
//-------------------------------------------------------------------------

inline int
mgmt_accept(int s, struct sockaddr *addr, int *addrlen)
{
  int r, retries;
  for (retries = 0; retries < MGMT_MAX_TRANSIENT_ERRORS; retries++) {
#if (HOST_OS == linux)
    r =::accept(s, addr, (socklen_t *) addrlen);
#else
    r =::accept(s, addr, addrlen);
#endif
    if (r >= 0)
      return r;
    if (!mgmt_transient_error())
      break;
  }
  return r;
}

//-------------------------------------------------------------------------
// mgmt_fopen
//-------------------------------------------------------------------------

inline FILE *
mgmt_fopen(const char *filename, const char *mode)
{
  FILE *f;
  int retries;
  for (retries = 0; retries < MGMT_MAX_TRANSIENT_ERRORS; retries++) {
    // no leak here as f will be returned if it is > 0
    // coverity[overwrite_var]
    f =::fopen(filename, mode);
    if (f > 0)
      return f;
    if (!mgmt_transient_error())
      break;
  }
  return f;
}

//-------------------------------------------------------------------------
// mgmt_open
//-------------------------------------------------------------------------

inline int
mgmt_open(const char *path, int oflag)
{
  int r, retries;
  for (retries = 0; retries < MGMT_MAX_TRANSIENT_ERRORS; retries++) {
    r =::open(path, oflag);
    if (r >= 0)
      return r;
    if (!mgmt_transient_error())
      break;
  }
  return r;
}

//-------------------------------------------------------------------------
// mgmt_open_mode
//-------------------------------------------------------------------------

inline int
mgmt_open_mode(const char *path, int oflag, mode_t mode)
{
  int r, retries;
  for (retries = 0; retries < MGMT_MAX_TRANSIENT_ERRORS; retries++) {
    r =::open(path, oflag, mode);
    if (r >= 0)
      return r;
    if (!mgmt_transient_error())
      break;
  }
  return r;
}

//-------------------------------------------------------------------------
// mgmt_select
//-------------------------------------------------------------------------

inline int
mgmt_select(int nfds, fd_set * readfds, fd_set * writefds, fd_set * errorfds, struct timeval *timeout)
{
  // Note: Linux select() has slight different semantics.  From the
  // man page: "On Linux, timeout is modified to reflect the amount of
  // time not slept; most other implementations do not do this."
  // Linux select() can also return ENOMEM, so we espeically need to
  // protect the call with the transient error retry loop.
  // Fortunately, because of the Linux timeout handling, our
  // mgmt_select call will still timeout correctly, rather than
  // possibly extending our timeout period by up to
  // MGMT_MAX_TRANSIENT_ERRORS times.
#if (HOST_OS == linux)
  int r, retries;
  for (retries = 0; retries < MGMT_MAX_TRANSIENT_ERRORS; retries++) {
    r =::select(nfds, readfds, writefds, errorfds, timeout);
    if (r >= 0)
      return r;
    if (!mgmt_transient_error())
      break;
  }
  return r;
#else
  return::select(nfds, readfds, writefds, errorfds, timeout);
#endif
}

//-------------------------------------------------------------------------
// mgmt_sendto
//-------------------------------------------------------------------------

inline int
mgmt_sendto(int fd, void *buf, int len, int flags, struct sockaddr *to, int tolen)
{
  int r, retries;
  for (retries = 0; retries < MGMT_MAX_TRANSIENT_ERRORS; retries++) {
    r =::sendto(fd, (char *) buf, len, flags, to, tolen);
    if (r >= 0)
      return r;
    if (!mgmt_transient_error())
      break;
  }
  return r;
}

//-------------------------------------------------------------------------
// mgmt_socket
//-------------------------------------------------------------------------

inline int
mgmt_socket(int domain, int type, int protocol)
{
  int r, retries;
  for (retries = 0; retries < MGMT_MAX_TRANSIENT_ERRORS; retries++) {
    r =::socket(domain, type, protocol);
    if (r >= 0)
      return r;
    if (!mgmt_transient_error())
      break;
  }
  return r;
}

#endif // _MGMT_SOCKET_H_
