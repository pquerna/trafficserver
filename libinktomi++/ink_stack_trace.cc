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

#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "inktomi++.h"
#include "ink_stack_trace.h"

#ifdef HAVE_EXECINFO_H

#include <execinfo.h>           /* for backtrace_symbols, etc. */
#include <signal.h>
struct sigframe
{
  char *pretcode;
  int sig;
  struct sigcontext sc;
  struct _fpstate fpstate;
};

static void
ink_restore_signal_handler_frame(void **stack, int len, int signalhandler_frame)
{
  void **fp;
  int i;
  struct sigframe *sf;
  struct sigcontext *scxt;

#ifdef __i386__
  asm volatile ("movl %%ebp,%0":"=r" (fp));
#else
  asm volatile ("mov %%rbp,%0":"=r" (fp));
#endif
  for (i = 0; i < signalhandler_frame; i++)
    fp = (void **) (*fp);
  sf = (struct sigframe *) (fp + 1);
  scxt = &(sf->sc);
#ifdef __i386__
  stack[signalhandler_frame + 1] = (void *) scxt->eip;
#else
  stack[signalhandler_frame + 1] = (void *) scxt->rip;
#endif
  for (i = signalhandler_frame + 2; i < len - 1; i++)
    stack[i] = stack[i + 1];
}

int
ink_stack_trace_get(void **stack, int len, int signalhandler_frame)
{
  int btl, i;
  if ((btl = backtrace(stack, len)) > 0) {
    if (signalhandler_frame)
      ink_restore_signal_handler_frame(stack, btl, signalhandler_frame + 1);
    // remove the frame corresponding to ink_backtrace &
    // ink_stack_trace_get
    for (i = 0; i < btl - 2; i++)
      stack[i] = stack[i + 2];
  }
  return btl - 2;
}

void
ink_stack_trace_dump(int sighandler_frame)
{
  int btl;

  // Recopy and re-terminate the app name in case it has been trashed.
  char name[256];
  char *msg = " - STACK TRACE: \n";
  ink_strncpy(name, program_name, sizeof(name) - 2);
  if (write(2, name, strlen(name)) == -1)
    return;
  if (write(2, msg, strlen(msg)) == -1)
    return;

  void *stack[INK_STACK_TRACE_MAX_LEVELS + 1];
  memset(stack, 0, sizeof(stack));
  btl = ink_stack_trace_get(stack, INK_STACK_TRACE_MAX_LEVELS, sighandler_frame);

  // dump the backtrace to stderr
  backtrace_symbols_fd(stack, btl, 2);
}

#else  // linux check

void
ink_stack_trace_dump(int sighandler_frame)
{
  const char msg[] = "ink_stack_trace_dump not implemented on this operating system\n";
  write(2, msg, sizeof(msg) - 1);
}

#endif  // linux check
