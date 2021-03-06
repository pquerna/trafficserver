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

#ifdef NON_MODULAR
#include "P_Net.h"
#include "Show.h"

struct ShowNet;
typedef int (ShowNet::*ShowNetEventHandler) (int event, Event * data);
struct ShowNet:ShowCont
{
  int ithread;
  int port;
  unsigned int ip;

  int showMain(int event, Event * e)
  {
    CHECK_SHOW(begin("Net"));
    CHECK_SHOW(show("<H3>Show <A HREF=\"/connections\">Connections</A></H3>\n"
                    "<H3>Show <A HREF=\"/threads\">Net Threads</A></H3>\n"
                    "<form method = GET action = \"/ips\">\n"
                    "Show Connections to/from IP (e.g. 127.0.0.1):<br>\n"
                    "<input type=text name=ip size=64 maxlength=256>\n"
                    "</form>\n"
                    "<form method = GET action = \"/ports\">\n"
                    "Show Connections to/from Port (e.g. 80):<br>\n"
                    "<input type=text name=name size=64 maxlength=256>\n" "</form>\n"));
    return complete(event, e);
  }

  int showConnectionsOnThread(int event, Event * e)
  {
    EThread *ethread = e->ethread;
    NetHandler *nh = get_NetHandler(ethread);
    MUTEX_TRY_LOCK(lock, nh->mutex, ethread);
    if (!lock) {
      ethread->schedule_in(this, NET_RETRY_DELAY);
      return EVENT_DONE;
    }

    ink_hrtime now = ink_get_hrtime();
    //for (int i = 0; i < n_netq_list; i++) {
    Queue<UnixNetVConnection> &q = nh->wait_list.read_wait_list;
    for (UnixNetVConnection * vc = (UnixNetVConnection *) q.head; vc; vc = (UnixNetVConnection *) vc->read.link.next) {
      if (ip && ip != vc->ip)
        continue;
      if (port && port != vc->port && port != vc->accept_port)
        continue;
      char ipbuf[80];
      snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u", PRINT_IP(vc->ip));
      char interbuf[80];
      snprintf(interbuf, sizeof(interbuf), "%u.%u.%u.%u", PRINT_IP(vc->_interface));
      CHECK_SHOW(show("<tr>"
                      // "<td><a href=\"/connection/%d\">%d</a></td>" 
                      "<td>%d</td>" "<td>%s</td>"       // ipbuf
                      "<td>%d</td>"     // port
                      "<td>%d</td>"     // fd
                      "<td>%s</td>"     // interbuf
                      "<td>%d</td>"     // accept port
                      "<td>%d secs ago</td>"    // start time
                      "<td>%d</td>"     // thread id
                      "<td>%d</td>"     // read enabled
                      "<td>%d</td>"     // read priority
                      "<td>%d</td>"     // read NBytes
                      "<td>%d</td>"     // read NDone
                      "<td>%d</td>"     // write enabled
                      "<td>%d</td>"     // write priority
                      "<td>%d</td>"     // write nbytes
                      "<td>%d</td>"     // write ndone
                      "<td>%d secs</td>"        // Inactivity timeout at
                      "<td>%d secs</td>"        // Activity timeout at
                      "<td>%d</td>"     // shutdown
                      "<td>-%s</td>"    // comments
                      "</tr>\n", vc->id,        // vc->id,
                      ipbuf,
                      vc->port,
                      vc->con.fd,
                      interbuf,
                      vc->accept_port,
                      (int) ((now - vc->submit_time) / HRTIME_SECOND),
                      ethread->id,
                      vc->read.enabled,
                      vc->read.priority,
                      vc->read.vio.nbytes,
                      vc->read.vio.ndone,
                      vc->write.enabled,
                      vc->write.priority,
                      vc->write.vio.nbytes,
                      vc->write.vio.ndone,
                      (int) (vc->inactivity_timeout_in / HRTIME_SECOND),
                      (int) (vc->active_timeout_in / HRTIME_SECOND), vc->f.shutdown, vc->closed ? "closed " : ""));
    }
    //}
    ithread++;
    if (ithread < eventProcessor.n_threads_for_type[ET_NET])
      eventProcessor.eventthread[ET_NET][ithread]->schedule_imm(this);
    else {
      CHECK_SHOW(show("</table>\n"));
      return complete(event, e);
    }
    return EVENT_CONT;
  }

  int showConnections(int event, Event * e)
  {
    CHECK_SHOW(begin("Net Connections"));
    CHECK_SHOW(show("<H3>Connections</H3>\n"
                    "<table border=1><tr>"
                    "<th>ID</th>"
                    "<th>IP</th>"
                    "<th>Port</th>"
                    "<th>FD</th>"
                    "<th>Interface</th>"
                    "<th>Accept Port</th>"
                    "<th>Time Started</th>"
                    "<th>Thread</th>"
                    "<th>Read Enabled</th>"
                    "<th>Read Priority</th>"
                    "<th>Read NBytes</th>"
                    "<th>Read NDone</th>"
                    "<th>Write Enabled</th>"
                    "<th>Write Priority</th>"
                    "<th>Write NBytes</th>"
                    "<th>Write NDone</th>"
                    "<th>Inactive Timeout</th>"
                    "<th>Active   Timeout</th>" "<th>Shutdown</th>" "<th>Comments</th>" "</tr>\n"));
    SET_HANDLER(&ShowNet::showConnectionsOnThread);
    eventProcessor.eventthread[ET_NET][0]->schedule_imm(this);
    return EVENT_CONT;
  }

  int showSingleThread(int event, Event * e)
  {
    EThread *ethread = e->ethread;
    NetHandler *nh = get_NetHandler(ethread);
    PollDescriptor *pollDescriptor = get_PollDescriptor(ethread);
    MUTEX_TRY_LOCK(lock, nh->mutex, ethread);
    if (!lock) {
      ethread->schedule_in(this, NET_RETRY_DELAY);
      return EVENT_DONE;
    }

    CHECK_SHOW(show("<H3>Thread: %d</H3>\n", ithread));
    CHECK_SHOW(show("<table border=1>\n"));
    int connections = 0;
    /*int *read_pri = new int[n_netq_list];
       int *write_pri = new int[n_netq_list];
       int *read_buck = new int[n_netq_list];
       int *write_buck = new int[n_netq_list]; */
    Queue<UnixNetVConnection> &qr = nh->wait_list.read_wait_list;
    UnixNetVConnection *vc = (UnixNetVConnection *) qr.head;
    for (; vc; vc = (UnixNetVConnection *) vc->read.link.next) {
      connections++;
      //read_pri[vc->read.priority <= 0 ? 0 : vc->read.priority]++;
      //read_buck[i]++;
    }
    Queue<UnixNetVConnection> &qw = nh->wait_list.write_wait_list;
    for (vc = (UnixNetVConnection *) qw.head; vc; vc = (UnixNetVConnection *) vc->write.link.next) {
      //write_pri[vc->write.priority <= 0 ? 0 : vc->write.priority]++;
      //write_buck[i]++;
    }
    CHECK_SHOW(show("<tr><td>%s</td><td>%d</td></tr>\n", "Connections", connections));
#ifdef BSD_TCP
    CHECK_SHOW(show("<tr><td>%s</td><td>%d</td></tr>\n",
                    "Last Poll Size", ethread->pollDescriptor->nfds_bsd + ethread->pollDescriptor->nfds_sol));
#else
    CHECK_SHOW(show("<tr><td>%s</td><td>%d</td></tr>\n", "Last Poll Size", pollDescriptor->nfds));
#endif
    CHECK_SHOW(show("<tr><td>%s</td><td>%d</td></tr>\n", "Last Poll Ready", pollDescriptor->result));
    CHECK_SHOW(show("</table>\n"));
    CHECK_SHOW(show("<table border=1>\n"));
    CHECK_SHOW(show
               ("<tr><th>#</th><th>Read Priority</th><th>Read Bucket</th><th>Write Priority</th><th>Write Bucket</th></tr>\n"));
    /*for (i = 0; i < n_netq_list; i++) {
       CHECK_SHOW(show(
       "<tr><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>\n",
       i, read_pri[i],read_buck[i],write_pri[i],write_buck[i]));
       } */
    CHECK_SHOW(show("</table>\n"));
    ithread++;
    if (ithread < eventProcessor.n_threads_for_type[ET_NET])
      eventProcessor.eventthread[ET_NET][ithread]->schedule_imm(this);
    else
      return complete(event, e);
    return EVENT_CONT;
  }

  int showThreads(int event, Event * e)
  {
    CHECK_SHOW(begin("Net Threads"));
    SET_HANDLER(&ShowNet::showSingleThread);
    eventProcessor.eventthread[ET_NET][0]->schedule_imm(this);
    return EVENT_CONT;
  }
  int showSingleConnection(int event, Event * e)
  {
    CHECK_SHOW(begin("Net Connection"));
    return complete(event, e);
  }
  int showHostnames(int event, Event * e)
  {
    CHECK_SHOW(begin("Net Connections to/from Host"));
    return complete(event, e);
  }

ShowNet(Continuation * c, HTTPHdr * h):
  ShowCont(c, h), ithread(0), port(0), ip(0) {
    SET_HANDLER(&ShowNet::showMain);
  }
};

#undef STREQ_PREFIX
#define STREQ_PREFIX(_x,_n,_s) (!ptr_len_ncasecmp(_x,_n,_s,sizeof(_s)-1))
Action *
register_ShowNet(Continuation * c, HTTPHdr * h)
{
  ShowNet *s = NEW(new ShowNet(c, h));
  int path_len;
  const char *path = h->url_get()->path_get(&path_len);
  SET_CONTINUATION_HANDLER(s, &ShowNet::showMain);
  if (STREQ_PREFIX(path, path_len, "connections")) {
    SET_CONTINUATION_HANDLER(s, &ShowNet::showConnections);
  } else if (STREQ_PREFIX(path, path_len, "threads")) {
    SET_CONTINUATION_HANDLER(s, &ShowNet::showThreads);
#if 0
  } else if (STREQ_PREFIX(path, path_len, "connection/")) {
    s->iarg = atoi(path + sizeof("connection/") - 1);
    SET_CONTINUATION_HANDLER(s, &ShowNet::showSingleConnection);
  } else if (STREQ_PREFIX(path, path_len, "hostnames")) {
    s->sarg = xstrdup(h->url_get().query_get());
    if (s->sarg && *s->sarg) {
      SET_CONTINUATION_HANDLER(s, &ShowNet::showHostnames);
    }
#endif
  } else if (STREQ_PREFIX(path, path_len, "ips")) {
    int query_len;
    const char *query = h->url_get()->query_get(&query_len);
    s->sarg = xstrndup(query, query_len);
    char *gn = NULL;
    if (s->sarg)
      gn = (char *) ink_memchr(s->sarg, '=', strlen(s->sarg));
    if (gn)
      s->ip = ink_inet_addr(gn + 1);
    SET_CONTINUATION_HANDLER(s, &ShowNet::showConnections);
  } else if (STREQ_PREFIX(path, path_len, "ports")) {
    int query_len;
    const char *query = h->url_get()->query_get(&query_len);
    s->sarg = xstrndup(query, query_len);
    char *gn = NULL;
    if (s->sarg)
      gn = (char *) ink_memchr(s->sarg, '=', strlen(s->sarg));
    if (gn)
      s->port = atoi(gn + 1);
    SET_CONTINUATION_HANDLER(s, &ShowNet::showConnections);
  }
  eventProcessor.schedule_imm(s, ET_NET);
  return &s->action;
}

#endif
