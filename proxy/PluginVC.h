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

   PluginVC.h

   Description: Allows bi-directional transfer for data from one
      continuation to another via a mechanism that impersonates a
      NetVC.  Should implement all external attributes of NetVConnections.
      [See PluginVC.cc for further comments]

   
 ****************************************************************************/

#ifndef _PLUGIN_VC_H_
#define _PLUGIN_VC_H_

#include "P_Net.h"
#include "ink_atomic.h"

class PluginVCCore;

struct PluginVCState
{
  PluginVCState();
  VIO vio;
  bool shutdown;
};

inline
PluginVCState::PluginVCState():
vio(),
shutdown(false)
{
}

enum PluginVC_t
{
  PLUGIN_VC_UNKNOWN,
  PLUGIN_VC_ACTIVE,
  PLUGIN_VC_PASSIVE
};

// For the id in set_data/get_data
enum
{
  PLUGIN_VC_DATA_LOCAL,
  PLUGIN_VC_DATA_REMOTE
};

enum
{
  PLUGIN_VC_MAGIC_ALIVE = 0xaabbccdd,
  PLUGIN_VC_MAGIC_DEAD = 0xaabbdead
};

class PluginVC:public NetVConnection
{
  friend class PluginVCCore;
public:

    PluginVC();
   ~PluginVC();

  virtual VIO *do_io_read(Continuation * c = NULL, int nbytes = INT_MAX, MIOBuffer * buf = 0);

  virtual VIO *do_io_write(Continuation * c = NULL, int nbytes = INT_MAX, IOBufferReader * buf = 0, bool owner = false);

  virtual bool is_over_ssl()
  {
    return (false);
  }

  virtual void do_io_close(int lerrno = -1);
  virtual void do_io_shutdown(ShutdownHowTo_t howto);

  // Reenable a given vio.  The public interface is through VIO::reenable
  virtual void reenable(VIO * vio);
  virtual void reenable_re(VIO * vio);

  // Timeouts
  virtual void set_active_timeout(ink_hrtime timeout_in);
  virtual void set_inactivity_timeout(ink_hrtime timeout_in);
  virtual void cancel_active_timeout();
  virtual void cancel_inactivity_timeout();
  virtual ink_hrtime get_active_timeout();
  virtual ink_hrtime get_inactivity_timeout();

  // Pure virutal functions we need to compile
  virtual SOCKET get_socket();
  virtual void set_local_addr();
  virtual void set_remote_addr();

  virtual bool get_data(int id, void *data);
  virtual bool set_data(int id, void *data);

  int main_handler(int event, void *data);

private:
  void process_read_side(bool);
  void process_write_side(bool);
  void process_close();
  void process_timeout(Event * e, int event_to_send, Event ** our_eptr);

  void setup_event_cb(ink_hrtime in, Event ** e_ptr);

  void update_inactive_time();
  int transfer_bytes(MIOBuffer * transfer_to, IOBufferReader * transfer_from, int act_on);

  inku32 magic;
  PluginVC_t vc_type;
  PluginVCCore *core_obj;

  PluginVC *other_side;

  PluginVCState read_state;
  PluginVCState write_state;

  bool need_read_process;
  bool need_write_process;

  volatile bool closed;
  Event *sm_lock_retry_event;
  Event *core_lock_retry_event;

  bool deletable;
  int reentrancy_count;

  ink_hrtime active_timeout;
  ink_hrtime inactive_timeout;
  Event *active_event;
  Event *inactive_event;

};

class PluginVCCore:public Continuation
{
  friend class PluginVC;
public:
    PluginVCCore();
   ~PluginVCCore();

  static PluginVCCore *alloc();
  void init();
  void set_accept_cont(Continuation * c);

  int state_send_accept(int event, void *data);
  int state_send_accept_failed(int event, void *data);

  void attempt_delete();

  PluginVC *connect();
  Action *connect_re(Continuation * c);
  void kill_no_connect();

  void set_active_addr(inku32 ip, int port);
  void set_passive_addr(inku32 ip, int port);

  void set_active_data(void *data);
  void set_passive_data(void *data);

private:

  void destroy();

  // The active vc is handed to the initiator of
  //   connection.  The passive vc is handled to
  //   receiver of the connection
  PluginVC active_vc;
  PluginVC passive_vc;
  Continuation *connect_to;
  bool connected;

  MIOBuffer *p_to_a_buffer;
  IOBufferReader *p_to_a_reader;

  MIOBuffer *a_to_p_buffer;
  IOBufferReader *a_to_p_reader;

  struct sockaddr_in passive_addr_struct;
  struct sockaddr_in active_addr_struct;

  void *passive_data;
  void *active_data;

  static vink32 nextid;
  unsigned id;
};

inline
PluginVCCore::PluginVCCore():
active_vc(),
passive_vc(),
connect_to(NULL),
connected(false),
p_to_a_buffer(NULL),
p_to_a_reader(NULL),
a_to_p_buffer(NULL),
a_to_p_reader(NULL),
passive_data(NULL),
active_data(NULL),
id(0)
{
  memset(&active_addr_struct, 0, sizeof(struct sockaddr_in));
  memset(&passive_addr_struct, 0, sizeof(struct sockaddr_in));

  id = ink_atomic_increment(&nextid, 1);
}

#endif
