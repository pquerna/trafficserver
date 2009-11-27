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

#include "P_Net.h"

// For Stat Pages
#ifdef NON_MODULAR
#include "StatPages.h"
#endif

// Globals
int use_accept_thread = 0;


int net_connection_number = 1;
unsigned int
net_next_connection_number()
{
  unsigned int res = 0;
  do {
    res = (unsigned int)
      ink_atomic_increment(&net_connection_number, 1);
  } while (!res);
  return res;
}

Action *
NetProcessor::accept(Continuation * cont,
                     int port,
                     bool frequent_accept,
                     unsigned int accept_ip,
                     bool callback_on_open,
                     SOCKET listen_socket_in,
                     int accept_pool_size,
                     bool accept_only,
                     sockaddr * bound_sockaddr,
                     int *bound_sockaddr_size,
                     int recv_bufsize, int send_bufsize, unsigned long sockopt_flags, EventType etype)
{
  (void) listen_socket_in;      // NT only
  (void) accept_pool_size;      // NT only
  (void) accept_only;           // NT only
  (void) bound_sockaddr;        // NT only
  (void) bound_sockaddr_size;   // NT only
#ifdef __INKIO
  Debug("net_processor", "NetProcessor::accept - port %d,recv_bufsize %d, send_bufsize %d, sockopt 0x%0lX",
        port, recv_bufsize, send_bufsize, sockopt_flags);
  return ((UnixNetProcessor *) this)->accept_internal(cont, NO_FD,
                                                      port,
                                                      bound_sockaddr,
                                                      bound_sockaddr_size,
                                                      frequent_accept,
                                                      inkio_net_accept,
                                                      recv_bufsize, send_bufsize, sockopt_flags,
                                                      accept_ip, callback_on_open, etype);

#else
  Debug("net_processor", "NetProcessor::accept - port %d,recv_bufsize %d, send_bufsize %d, sockopt 0x%0lX",
        port, recv_bufsize, send_bufsize, sockopt_flags);
  return ((UnixNetProcessor *) this)->accept_internal(cont, NO_FD, port,
                                                      bound_sockaddr,
                                                      bound_sockaddr_size,
                                                      frequent_accept,
                                                      net_accept,
                                                      recv_bufsize, send_bufsize, sockopt_flags,
                                                      accept_ip, callback_on_open, etype);

#endif
}

Action *
NetProcessor::main_accept(Continuation * cont, SOCKET fd, int port,
                          sockaddr * bound_sockaddr, int *bound_sockaddr_size,
                          bool accept_only,
                          int recv_bufsize, int send_bufsize, unsigned long sockopt_flags,
                          EventType etype, bool callback_on_open)
{
  (void) accept_only;           // NT only
#ifdef __INKIO
  Debug("net_processor", "NetProcessor::main_accept - port %d,recv_bufsize %d, send_bufsize %d, sockopt 0x%0lX",
        port, recv_bufsize, send_bufsize, sockopt_flags);

  return ((UnixNetProcessor *) this)->accept_internal(cont, fd, port,
                                                      bound_sockaddr,
                                                      bound_sockaddr_size,
                                                      true,
                                                      inkio_net_accept,
                                                      recv_bufsize,
                                                      send_bufsize,
                                                      sockopt_flags,
                                                      ((UnixNetProcessor *) this)->incoming_ip_to_bind_saddr,
                                                      callback_on_open, etype);
#else
  Debug("net_processor", "NetProcessor::main_accept - port %d,recv_bufsize %d, send_bufsize %d, sockopt 0x%0lX",
        port, recv_bufsize, send_bufsize, sockopt_flags);
  return ((UnixNetProcessor *) this)->accept_internal(cont, fd, port,
                                                      bound_sockaddr,
                                                      bound_sockaddr_size,
                                                      true,
                                                      net_accept,
                                                      recv_bufsize,
                                                      send_bufsize,
                                                      sockopt_flags,
                                                      ((UnixNetProcessor *) this)->incoming_ip_to_bind_saddr,
                                                      callback_on_open, etype);

#endif
}



Action *
UnixNetProcessor::accept_internal(Continuation * cont,
                                  int fd,
                                  int port,
                                  struct sockaddr * bound_sockaddr,
                                  int *bound_sockaddr_size,
                                  bool frequent_accept,
                                  AcceptFunction fn,
                                  int recv_bufsize,
                                  int send_bufsize,
                                  unsigned long sockopt_flags,
                                  unsigned int accept_ip, bool callback_on_open, EventType etype)
{
  setEtype(etype);
  NetAccept *na = createNetAccept();

  EThread *thread = this_ethread();
  ProxyMutex *mutex = thread->mutex;
  NET_INCREMENT_DYN_STAT(net_accepts_currently_open_stat);
  na->port = port;
  na->accept_fn = fn;
  na->server.fd = fd;
  na->server.accept_ip = accept_ip;
  na->action_ = NEW(new NetAcceptAction());
  *na->action_ = cont;
  na->action_->server = &na->server;
  na->callback_on_open = callback_on_open;
  na->recv_bufsize = recv_bufsize;
  na->send_bufsize = send_bufsize;
  na->sockopt_flags = sockopt_flags;
  na->etype = etype;
  if (na->callback_on_open)
    na->mutex = cont->mutex;
  if (frequent_accept)          // true
  {
    if (use_accept_thread)      // 0
    {
      na->init_accept_loop();
    } else {
      na->init_accept_per_thread();
    }
  } else {
    na->init_accept();
  }
  if (bound_sockaddr && bound_sockaddr_size)
    safe_getsockname(na->server.fd, bound_sockaddr, bound_sockaddr_size);

#ifdef TCP_DEFER_ACCEPT
  // set tcp defer accept timeout if it is configured, this will not trigger an accept until there is
  // data on the socket ready to be read
  int accept_timeout = 0;
  IOCORE_ReadConfigInteger(accept_timeout, "proxy.config.net.tcp_accept_defer_timeout");
  if (accept_timeout > 0) {
    setsockopt(na->server.fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &accept_timeout, sizeof(int));
  }
#endif
  return na->action_;
}

Action *
UnixNetProcessor::connect_re_internal(Continuation * cont,
                                      unsigned int ip, int port, unsigned int _interface, NetVCOptions * opt)
{

  ProxyMutex *mutex = cont->mutex;
  EThread *t = mutex->thread_holding;
  UnixNetVConnection *vc = allocateThread(t);
  if (opt)
    vc->options = *opt;
  else
    opt = &vc->options;

  vc->_interface = _interface;
  // virtual function used to set etype to ET_SSL
  // for SSLNetProcessor.  Does nothing if not overwritten.
  setEtype(opt->etype);

  bool fast =
#ifndef INK_NO_SOCKS
    (opt->socks_support == NO_SOCKS || !socks_conf_stuff->socks_needed) &&
#endif
    cont->mutex->is_thread();

  if (fast) {

    NET_INCREMENT_DYN_STAT(net_connections_currently_open_stat);
    vc->id = net_next_connection_number();
#ifdef XXTIME
    vc->submit_time = ink_get_hrtime_internal();
#else
    vc->submit_time = ink_get_hrtime();
#endif
    vc->setSSLClientConnection(true);
    vc->ip = ip;
    vc->port = port;
    vc->action_ = cont;
    vc->mutex = mutex;
    vc->connectUp(t);
    return ACTION_RESULT_DONE;
  } else {

#ifndef INK_NO_SOCKS
    bool using_socks = (socks_conf_stuff->socks_needed && opt->socks_support != NO_SOCKS
#ifdef SOCKS_WITH_TS
                        && (opt->socks_version != SOCKS_DEFAULT_VERSION ||
                            /* This implies we are tunnelling. 
                             * we need to connect using socks server even 
                             * if this ip is in no_socks list.
                             */
                            !socks_conf_stuff->ip_range.match(ip))
#endif
      );
    SocksEntry *socksEntry = NULL;
#endif
    NET_INCREMENT_DYN_STAT(net_connections_currently_open_stat);
    vc->id = net_next_connection_number();
#ifdef XXTIME
    vc->submit_time = ink_get_hrtime_internal();
#else
    vc->submit_time = ink_get_hrtime();
#endif
    vc->setSSLClientConnection(true);
    vc->ip = ip;
    vc->port = port;
    vc->mutex = cont->mutex;
    Action *result = &vc->action_;
#ifndef INK_NO_SOCKS
    if (using_socks) {

      Debug("Socks", "Using Socks ip: %u.%u.%u.%u:%d\n", PRINT_IP(ip), port);
      socksEntry = socksAllocator.alloc();
      socksEntry->init(cont->mutex, vc, opt->socks_support, opt->socks_version);        /*XXXX remove last two args */
      socksEntry->action_ = cont;
      cont = socksEntry;
      if (socksEntry->server_ip == (inku32) - 1) {
        socksEntry->lerrno = ESOCK_NO_SOCK_SERVER_CONN;
        socksEntry->free();
        return ACTION_RESULT_DONE;
      }
      vc->ip = socksEntry->server_ip;
      vc->port = socksEntry->server_port;
      result = &socksEntry->action_;
      vc->action_ = socksEntry;
    } else {
      Debug("Socks", "Not Using Socks %d \n", socks_conf_stuff->socks_needed);
      vc->action_ = cont;
    }
#else
    vc->action_ = cont;
#endif /*INK_NO_SOCKS */

    if (t->is_event_type(opt->etype)) {
      MUTEX_TRY_LOCK(lock, cont->mutex, t);
      if (lock) {
#ifdef __INKIO
        MUTEX_TRY_LOCK(lock2, t->netQueueMonitor->mutex, t);
#else
        MUTEX_TRY_LOCK(lock2, get_NetHandler(t)->mutex, t);
#endif
        if (lock2) {
          int ret;
          ret = vc->connectUp(t);
#ifndef INK_NO_SOCKS
          if ((using_socks) && (ret == CONNECT_SUCCESS))
            return &socksEntry->action_;
          else
#endif
            return ACTION_RESULT_DONE;
        }
      }
    }
    eventProcessor.schedule_imm(vc, opt->etype);
#ifndef INK_NO_SOCKS
    if (using_socks) {
      return &socksEntry->action_;
    } else
#endif
      return result;
  }
}


Action *
UnixNetProcessor::connect(Continuation * cont,
                          UnixNetVConnection ** avc,
                          unsigned int ip, int port, unsigned int _interface, NetVCOptions * opt)
{
  bool fast =
#ifndef INK_NO_SOCKS
    !socks_conf_stuff->socks_needed &&
#endif
    cont->mutex->is_thread();
  if (!fast) {
    return connect_re(cont, ip, port, _interface, opt);
  }
  ProxyMutex *mutex = cont->mutex;
  EThread *t = mutex->thread_holding;
  //NET_INCREMENT_DYN_STAT(net_connections_currently_open_stat);
  UnixNetVConnection *vc = allocateThread(t);
  vc->_interface = _interface;
  if (opt)
    vc->options = *opt;
  else
    opt = &vc->options;
  vc->id = net_next_connection_number();
#ifdef XXTIME
  vc->submit_time = ink_get_hrtime_internal();
#else
  vc->submit_time = ink_get_hrtime();
#endif
  vc->setSSLClientConnection(true);
  vc->ip = ip;
  vc->port = port;
  vc->action_ = cont;
  vc->thread = t;
  vc->nh = get_NetHandler(t);   //added by YTS Team, yamsat
  vc->mutex = mutex;
  if (check_net_throttle(CONNECT, vc->submit_time)) {
    check_throttle_warning();
    free(t);
    *avc = NULL;
    return ACTION_RESULT_DONE;
  }

  int res = 0;
  if (!_interface)
    res = vc->con.fast_connect(vc->ip, vc->port, opt);
  else
    res = vc->con.bind_connect(vc->ip, vc->port, _interface, opt);

  if (res) {
    free(t);
    *avc = NULL;
    return ACTION_RESULT_DONE;
  }

  check_emergency_throttle(vc->con);

  // start up next round immediately

  //added by YTS Team, yamsat
  struct epoll_data_ptr *eptr;
  eptr = (struct epoll_data_ptr *) xmalloc(sizeof(struct epoll_data_ptr));
  eptr->type = EPOLL_READWRITE_VC;
  eptr->data.vc = vc;

  vc->ep = eptr;

  PollDescriptor *pd = get_PollDescriptor(t);

#if defined(USE_EPOLL)
  struct epoll_event ev;
  memset(&ev, 0, sizeof(struct epoll_event));
  ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
  ev.data.ptr = eptr;

  res = epoll_ctl(pd->epoll_fd, EPOLL_CTL_ADD, vc->con.fd, &ev);

  if (res < 0) {
    Debug("iocore_net", "connect : Error in adding to epoll list\n");
    close_UnixNetVConnection(vc, vc->thread);
    return ACTION_RESULT_DONE;
  }

#elif defined(USE_KQUEUE)
  struct kevent ev;
  EV_SET(&ev, vc->con.fd, EVFILT_READ, EV_ADD, 0, 0, eptr);
  if (kevent(pd->kqueue_fd, &ev, 1, NULL, 0, NULL) < 0) {
    Debug("iocore_net", "connect : Error in adding to kqueue list\n");
    close_UnixNetVConnection(vc, vc->thread);
    return ACTION_RESULT_DONE;
  }

  EV_SET(&ev, vc->con.fd, EVFILT_WRITE, EV_ADD, 0, 0, eptr);
  if (kevent(pd->kqueue_fd, &ev, 1, NULL, 0, NULL) < 0) {
    Debug("iocore_net", "connect : Error in adding to kqueue list\n");
    close_UnixNetVConnection(vc, vc->thread);
    return ACTION_RESULT_DONE;
  }
#else
#error port me
#endif

  Debug("iocore_net", "connect : Adding fd %d to read wait list\n", vc->con.fd);
  vc->nh->wait_list.epoll_addto_read_wait_list(vc);
  Debug("iocore_net", "connect : Adding fd %d to write wait list\n", vc->con.fd);
  vc->nh->wait_list.epoll_addto_write_wait_list(vc);

  SET_CONTINUATION_HANDLER(vc, (NetVConnHandler) & UnixNetVConnection::mainEvent);
  ink_assert(!vc->inactivity_timeout_in);
  ink_assert(!vc->active_timeout_in);
  XTIME(printf("%d 1connect\n", vc->id));
  *avc = vc;
  return ACTION_RESULT_DONE;
}



struct CheckConnect:public Continuation
{
  UnixNetVConnection *vc;
  Action action_;
  MIOBuffer *buf;
  IOBufferReader *reader;
  int connect_status;
  int recursion;
  ink_hrtime timeout;

  int handle_connect(int event, Event * e)
  {
    connect_status = event;
    switch (event) {
    case NET_EVENT_OPEN:
      vc = (UnixNetVConnection *) e;
      Debug("connect", "connect Net open");
      vc->do_io_write(this, 10, /* some non-zero number just to get the poll going */
                      reader);
      /* dont wait for more than timeout secs */
      vc->set_inactivity_timeout(timeout);
      return EVENT_CONT;
      break;

      case NET_EVENT_OPEN_FAILED:Debug("connect", "connect Net open failed");
      if (!action_.cancelled)
        action_.continuation->handleEvent(NET_EVENT_OPEN_FAILED, (void *) e);
      break;

      case VC_EVENT_WRITE_READY:int sl, ret;
      socklen_t sz;
      if (!action_.cancelled)
      {
        sz = sizeof(int);
          ret = getsockopt(vc->con.fd, SOL_SOCKET, SO_ERROR, (char *) &sl, &sz);
        if (!ret && sl == 0)
        {
          Debug("connect", "connection established");
          /* disable write on vc */
          vc->write.enabled = 0;
          vc->cancel_inactivity_timeout();
          //write_disable(get_NetHandler(this_ethread()), vc); 
          /* clean up vc fields */
          vc->write.vio.nbytes = 0;
          vc->write.vio.op = VIO::NONE;
          vc->write.vio.buffer.clear();


          action_.continuation->handleEvent(NET_EVENT_OPEN, vc);
          delete this;
            return EVENT_DONE;
        }
      }
      vc->do_io_close();
      if (!action_.cancelled)
        action_.continuation->handleEvent(NET_EVENT_OPEN_FAILED, (void *) -ENET_CONNECT_FAILED);
      break;
    case VC_EVENT_INACTIVITY_TIMEOUT:
      Debug("connect", "connect timed out");
      vc->do_io_close();
      if (!action_.cancelled)
        action_.continuation->handleEvent(NET_EVENT_OPEN_FAILED, (void *) -ENET_CONNECT_TIMEOUT);
      break;
    default:
      ink_debug_assert(!"unknown connect event");
      if (!action_.cancelled)
        action_.continuation->handleEvent(NET_EVENT_OPEN_FAILED, (void *) -ENET_CONNECT_FAILED);

    }
    if (!recursion)
      delete this;
    return EVENT_DONE;
  }

  Action *connect_s(Continuation * cont, unsigned int ip, int port,
                    unsigned int _interface, int _timeout, NetVCOptions * opt)
  {
    action_ = cont;
    timeout = HRTIME_MSECONDS(_timeout);
    recursion++;
    netProcessor.connect_re(this, ip, port, _interface, opt);
    recursion--;
    if (connect_status != NET_EVENT_OPEN_FAILED)
      return &action_;
    else {
      delete this;
      return ACTION_RESULT_DONE;
    }
  }

CheckConnect(ProxyMutex * m = NULL):Continuation(m), connect_status(-1), recursion(0), timeout(0)
  {
    SET_HANDLER(&CheckConnect::handle_connect);
    buf = new_empty_MIOBuffer(1);
    reader = buf->alloc_reader();
  }

  ~CheckConnect() {
    buf->dealloc_all_readers();
    buf->clear();
    free_MIOBuffer(buf);
  }
};

Action *
NetProcessor::connect_s(Continuation * cont, unsigned int ip,
                        int port, unsigned int _interface, int timeout, NetVCOptions * opt)
{

  Debug("connect", "NetProcessor::connect_s called");
  CheckConnect *c = NEW(new CheckConnect(cont->mutex));
  return c->connect_s(cont, ip, port, _interface, timeout, opt);

}



struct PollCont;

int
UnixNetProcessor::start(int)
{
  EventType etype = ET_NET;
  netHandler_offset = eventProcessor.allocate(sizeof(NetHandler));
  pollCont_offset = eventProcessor.allocate(sizeof(PollCont));

  // customize the threads for net 
  setEtype(etype);
  // etype is ET_NET for netProcessor
  // and      ET_SSL for sslNetProcessor
  n_netthreads = eventProcessor.n_threads_for_type[etype];
  netthreads = eventProcessor.eventthread[etype];
  for (int i = 0; i < n_netthreads; i++) {
    initialize_thread_for_net(netthreads[i], i);
  }

  if ((incoming_ip_to_bind = IOCORE_ConfigReadString("proxy.local.incoming_ip_to_bind")) != 0)
    incoming_ip_to_bind_saddr = inet_addr(incoming_ip_to_bind);
  else
    incoming_ip_to_bind_saddr = 0;

  RecData d;
  d.rec_int = 0;
  change_net_connections_throttle(NULL, RECD_INT, d, NULL);

  // Socks
#ifndef INK_NO_SOCKS
  if (!netProcessor.socks_conf_stuff) {
    socks_conf_stuff = NEW(new socks_conf_struct);
    loadSocksConfiguration(socks_conf_stuff);
    if (!socks_conf_stuff->socks_needed && socks_conf_stuff->accept_enabled) {
      Warning("We can not have accept_enabled and socks_needed turned off" " disabling Socks accept\n");
      socks_conf_stuff->accept_enabled = 0;
    } else {
      // this is sslNetprocessor
      socks_conf_stuff = netProcessor.socks_conf_stuff;
    }
  }
#endif /*INK_NO_SOCKS */
  // commented by vijay -  bug 2489945 
  /*if (use_accept_thread) // 0
     { NetAccept * na = createNetAccept();
     SET_CONTINUATION_HANDLER(na,&NetAccept::acceptLoopEvent);
     accept_thread_event = eventProcessor.spawn_thread(na);
     if (!accept_thread_event) delete na;
     } */


/*
 * Stat pages
 */
#ifdef NON_MODULAR
#ifdef __INKIO
  statPagesManager.register_http("net", register_InkioShowNet);
#else
  extern Action *register_ShowNet(Continuation * c, HTTPHdr * h);
  statPagesManager.register_http("net", register_ShowNet);
#endif
#endif
  return 1;
}

// Functions all THREAD_FREE and THREAD_ALLOC to be performed
// for both SSL and regular UnixNetVConnection transparent to
// netProcessor connect functions. Yes it looks goofy to
// have them in both places, but it saves a bunch of
// code from being duplicated.
UnixNetVConnection *
UnixNetProcessor::allocateThread(EThread * t)
{
#ifdef __INKIO
  return ((UnixNetVConnection *) THREAD_ALLOC(inkioNetVCAllocator, t));
#else
  return ((UnixNetVConnection *) THREAD_ALLOC(netVCAllocator, t));
#endif
}

void
UnixNetProcessor::freeThread(UnixNetVConnection * vc, EThread * t)
{
#ifdef __INKIO
  THREAD_FREE((InkioNetVConnection *) vc, inkioNetVCAllocator, t);
#else
  THREAD_FREE(vc, netVCAllocator, t);
#endif
}


// Virtual function allows creation of an 
// SSLNetAccept or NetAccept transparent to NetProcessor.
NetAccept *
UnixNetProcessor::createNetAccept()
{
#ifdef __INKIO
  return (NEW(new InkioNetAccept));
#else
  return (NEW(new NetAccept));
#endif
}

struct socks_conf_struct *
  NetProcessor::socks_conf_stuff = NULL;
int
  NetProcessor::accept_mss = 0;

UnixNetProcessor
  unix_netProcessor;
NetProcessor & netProcessor = unix_netProcessor;
