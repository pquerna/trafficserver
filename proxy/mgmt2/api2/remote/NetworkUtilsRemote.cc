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
 * Filename: NetworkUtilsRemote.cc
 * Purpose: This file contains functions used by remote api client to 
 *          marshal requests to TM and unmarshal replies from TM.
 *          Also stores the information about the client's current
 *          socket connection to Traffic Manager
 * Created: 8/9/00
 * 
 * 
 ***************************************************************************/

#include "ink_sock.h"

#include "NetworkUtilsRemote.h"
#include "CoreAPI.h"
#include "CoreAPIShared.h"
#include "EventRegistration.h"
#include "MgmtSocket.h"

extern CallbackTable *remote_event_callbacks;

int main_socket_fd = -1;
int event_socket_fd = -1;

// need to store for reconnecting scenario
char *main_socket_path = NULL;  // "<path>/mgmtapisocket" 
char *event_socket_path = NULL; // "<path>/eventapisocket" 

/**********************************************************************
 * Socket Helper Functions
 **********************************************************************/
void
set_socket_paths(const char *path)
{
  // free previously set paths if needed
  if (main_socket_path)
    xfree(main_socket_path);
  if (event_socket_path)
    xfree(event_socket_path);

  // construct paths based on user input
  // form by replacing "mgmtapisocket" with "eventapisocket"
  if (path) {
    int api_len = strlen(path) + strlen("mgmtapisocket");
    int event_len = strlen(path) + strlen("eventapisocket");

    main_socket_path = (char *) xmalloc(sizeof(char) * (api_len + 1));
    event_socket_path = (char *) xmalloc(sizeof(char) * (event_len + 1));
    snprintf(main_socket_path, (sizeof(char) * (api_len + 1)), "%smgmtapisocket", path);
    snprintf(event_socket_path, (sizeof(char) * (event_len + 1)), "%seventapisocket", path);
    main_socket_path[api_len] = '\0';
    event_socket_path[event_len] = '\0';
  } else {
    main_socket_path = NULL;
    event_socket_path = NULL;
  }

  return;
}

/**********************************************************************
 * socket_test
 *
 * purpose: performs socket write to check status of other end of connection
 * input: None
 * output: return   0 if other end of connection closed;
 *         return < 0 if socket write failed due to some other error
 *         return > 0 if socket write successful
 * notes: send the test msg: UNDEFINED_OP 0(=msg_len)
 **********************************************************************/
int
socket_test(int fd)
{
  char msg[6];                  /* 6 = SIZE_OP + SIZE_LEN */
  ink16 op;
  ink32 msg_len = 0;
  int ret, amount_read = 0;

  // write the op
  op = (ink16) UNDEFINED_OP;
  memcpy(msg, (void *) &op, SIZE_OP_T);

  // write msg-len = 0
  memcpy(msg + SIZE_OP_T, &msg_len, SIZE_LEN);

  while (amount_read < 6) {
    ret = write(fd, msg + amount_read, 6 - amount_read);
    if (ret < 0) {
      if (errno == EAGAIN)
        continue;

      if (errno == EPIPE || errno == ENOTCONN) {        // other socket end is closed
        return 0;
      }

      return -1;
    }

    amount_read += ret;
  }

  return 1;                     // write was successful; connection still open
}


/***************************************************************************
 * connect
 *
 * purpose: connects to the port on traffic server that listens to mgmt
 *          requests & issues out responses and alerts
 * 1) create and set the client socket_fd; connect to TM
 * 2) create and set the client's event_socket_fd; connect to TM
 * output: INK_ERR_OKAY          - if both sockets sucessfully connect to TM
 *         INK_ERR_NET_ESTABLISH - at least one unsuccessful connection
 * notes: If connection breaks it is responsibility of client to reconnect
 *        otherwise traffic server will assume mgmt stopped request and
 *        goes back to just sitting and listening for connection.
 ***************************************************************************/
INKError
connect()
{
  struct sockaddr_un client_sock;
  struct sockaddr_un client_event_sock;

  int sockaddr_len;

  // make sure a socket path is set up
  if (!main_socket_path || !event_socket_path)
    goto ERROR;

  // create a socket
  main_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (main_socket_fd < 0) {
    //fprintf(stderr, "[connect] ERROR: can't open socket\n");
    goto ERROR;                 // ERROR - can't open socket
  }
  // setup Unix domain socket 
  memset(&client_sock, 0, sizeof(sockaddr_un));
  client_sock.sun_family = AF_UNIX;
  strncpy(client_sock.sun_path, main_socket_path, sizeof(client_sock.sun_path));
  sockaddr_len = sizeof(client_sock.sun_family) + strlen(client_sock.sun_path);

  // connect call
  if (connect(main_socket_fd, (struct sockaddr *) &client_sock, sockaddr_len) < 0) {
    //fprintf(stderr, "[connect] ERROR (main_socket_fd %d): %s\n", main_socket_fd, strerror(int(errno)));
    close(main_socket_fd);
    main_socket_fd = -1;
    goto ERROR;                 //connection is down
  }
  // -------- set up the event socket ------------------
  // create a socket
  event_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (event_socket_fd < 0) {
    //fprintf(stderr, "[connect] ERROR: can't open event socket\n");
    close(main_socket_fd);      // close the other socket too!
    main_socket_fd = -1;
    goto ERROR;                 // ERROR - can't open socket
  }
  // setup Unix domain socket
  memset(&client_event_sock, 0, sizeof(sockaddr_un));
  client_event_sock.sun_family = AF_UNIX;
  strncpy(client_event_sock.sun_path, event_socket_path, sizeof(client_sock.sun_path));
  sockaddr_len = sizeof(client_event_sock.sun_family) + strlen(client_event_sock.sun_path);

  // connect call
  if (connect(event_socket_fd, (struct sockaddr *) &client_event_sock, sockaddr_len) < 0) {
    //fprintf(stderr, "[connect] ERROR (event_socket_fd %d): %s\n", event_socket_fd, strerror(int(errno)));
    close(event_socket_fd);
    close(main_socket_fd);
    event_socket_fd = -1;
    main_socket_fd = -1;
    goto ERROR;                 //connection is down
  }

  return INK_ERR_OKAY;

ERROR:
  return INK_ERR_NET_ESTABLISH;
}

/***************************************************************************
 * disconnect
 *
 * purpose: disconnect from traffic server; closes sockets and resets their values
 * input: None
 * output: INK_ERR_FAIL, INK_ERR_OKAY
 * notes: doesn't do clean up - all cleanup should be done before here
 ***************************************************************************/
INKError
disconnect()
{
  int ret;

  if (main_socket_fd > 0) {
    ret = close(main_socket_fd);
    main_socket_fd = -1;
    if (ret < 0)
      return INK_ERR_FAIL;
  }

  if (event_socket_fd > 0) {
    ret = close(event_socket_fd);
    event_socket_fd = -1;
    if (ret < 0)
      return INK_ERR_FAIL;
  }

  return INK_ERR_OKAY;
}

/***************************************************************************
 * reconnect
 *
 * purpose: reconnects to TM (eg. when TM restarts); does all the necesarry
 *          set up for reconnection
 * input: None
 * output: INK_ERR_FAIL, INK_ERR_OKAY
 * notes: necessarry events for a new client-TM connection:
 * 1) get new socket_fd using old socket_path by calling connect()
 * 2) relaunch event_poll_thread_main with new socket_fd
 * 3) re-notify TM of all the client's registered callbacks by send msg
 ***************************************************************************/
INKError
reconnect()
{
  INKError err;

  err = disconnect();
  if (err != INK_ERR_OKAY)      // problem disconnecting
    return err;

  // use the socket_path that was called by remote client on first init
  // use connect instead of INKInit() b/c if TM restarted, client-side tables 
  // would be recreated; just want to reconnect to same socket_path
  err = connect();
  if (err != INK_ERR_OKAY)      // problem establishing connection
    return err;

  // relaunch a new event thread since socket_fd changed
  ink_thread_create(event_poll_thread_main, &(event_socket_fd));

  // reregister the callbacks on the TM side for this new client connection
  err = send_register_all_callbacks(event_socket_fd, remote_event_callbacks);
  if (err != INK_ERR_OKAY)      // problem establishing connection
    return err;

  return INK_ERR_OKAY;
}

/***************************************************************************
 * reconnect_loop
 *
 * purpose: attempts to reconnect to TM (eg. when TM restarts) for the 
 *          specified number of times
 * input:  num_attempts - number of reconnection attempts to try before quit
 * output: INK_ERR_OKAY - if succesfully reconnected within num_attempts
 *         INK_ERR_xx - the reason the reconnection failed
 * notes: 
 ***************************************************************************/
INKError
reconnect_loop(int num_attempts)
{
  int numTries = 0;
  INKError err = INK_ERR_FAIL;

  while (numTries < num_attempts) {
    numTries++;
    err = reconnect();
    if (err == INK_ERR_OKAY) {
      //fprintf(stderr, "[reconnect_loop] Successful reconnction; Leave loop\n");
      return INK_ERR_OKAY;      // successful connection
    }
    sleep(1);                   // to make it slower  
  }

  //fprintf(stderr, "[reconnect_loop] FAIL TO CONNECT after %d tries\n", num_attempts);
  return err;                   // unsuccessful connection after num_attempts
}

/*************************************************************************
 * connect_and_send
 *
 * purpose: 
 * When sending a request, it's possible that the user had restarted 
 * Traffic Manager. This means that the connection between TM and 
 * the remote client has been broken, so the client needs to re-"connect"
 * to Traffic Manager. So, after "writing" to the socket in each 
 * "send_xx_request" function, need to check if the TM socket has 
 * been closed or not; the "write" function's errno will indicate if
 * the other end of the socket has been closed or not. If it is closed,
 * then need to try to re"connect", then resend the message request if 
 * the "connect" was successful. 
 * 1) try connect() 
 * 2) if connect() success, then resend the request. 
 * output: INK_ERR_NET_xx - connection problem or INK_ERR_OKAY
 * notes:
 * This function is basically called by the special "socket_write_conn" fn
 * which will call this fn if it tries to write to the socket and discovers
 * the local end of the socket is closed
 * Warning: system also sends a SIGPIPE error when try to write to socket
 * which is not open; which will by default terminate the process;
 * client needs to "ignore" the SIGPIPE signal
 **************************************************************************/
INKError
connect_and_send(const char *msg, int msg_len)
{
  INKError err;
  int total_wrote = 0, ret;

  // connects to TM and does all necessary event updates required
  err = reconnect();
  if (err != INK_ERR_OKAY)
    return err;

  // makes sure the descriptor is writable
  if (socket_write_timeout(main_socket_fd, MAX_TIME_WAIT, 0) <= 0) {
    return INK_ERR_NET_TIMEOUT;
  }
  // connection successfully established; resend msg
  // socket_fd should be new fd
  while (total_wrote < msg_len) {
    ret = write(main_socket_fd, msg + total_wrote, msg_len - total_wrote);

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else if (errno == EPIPE || errno == ENOTCONN) {
        // clean-up sockets
        close(main_socket_fd);
        close(event_socket_fd);
        main_socket_fd = -1;
        event_socket_fd = -1;

        return INK_ERR_NET_ESTABLISH;   // can't establish connection

      } else
        return INK_ERR_NET_WRITE;       // general socket writing error

    }

    total_wrote += ret;
  }

  return INK_ERR_OKAY;
}

/**************************************************************************
 * socket_write_conn
 * 
 * purpose: guarantees writing of n bytes; if connection error, tries 
 *          reconnecting to TM again (in case TM was restarted)
 * input:   fd to write to, buffer to write from & number of bytes to write
 * output:  INK_ERR_xx
 * note:   EPIPE - this happens if client makes a call after stopping then 
 *         starting TM again. 
 *         ENOTCONN - this happens if the client tries to make a call after 
 *         stopping TM, but before starting it; then restarts TM and makes a
 *          new call 
 * In the send_xx_request function, use a special socket writing function
 * which calls connect_and_send() instead of just the basic connect():
 * 1) if the write returns EPIPE error, then call connect_and_send()
 * 2) return the value returned from EPIPE
 *************************************************************************/
INKError
socket_write_conn(int fd, const char *msg_buf, int bytes)
{
  int ret, byte_wrote = 0;

  // makes sure the descriptor is writable
  if (socket_write_timeout(fd, MAX_TIME_WAIT, 0) <= 0) {
    return INK_ERR_NET_TIMEOUT;
  }
  // read until we fulfill the number
  while (byte_wrote < bytes) {
    ret = write(fd, msg_buf + byte_wrote, bytes - byte_wrote);

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;

      else if (errno == EPIPE || errno == ENOTCONN) {   // other socket end is closed
        // clean-up of sockets is done in reconnect()
        return connect_and_send(msg_buf, bytes);
      } else
        return INK_ERR_NET_WRITE;
    }
    // we are all good here   
    byte_wrote += ret;
  }

  return INK_ERR_OKAY;
}

/**********************************************************************
 * socket_test_thread
 *
 * purpose: continually polls to check if local end of socket connection 
 *          is still open; this thread is created when the client calls 
 *          Init() to initialize the API; and will not
 *          die until the client process dies
 * input: none 
 * output: if other end is closed, it reconnects to TM 
 * notes: uses the current main_socket_fd because the main_socket_fd could be 
 *        in flux; basically it is possible that the client will reconnect
 *        from some other call, thus making the main_socket_fd actually
 *        valid when socket_test is called
 * reason: decided to create this "watcher" thread for the socket 
 *         connection because if TM is restarted or the client process
 *         is started before the TM process, then the client will not 
 *         be able to receive any event notifications until a "request"
 *         is issued. In order to prevent losing an event notifications
 *         that are called in between the time TM is restarted and 
 *         client issues a first request, we just run this thread which
 *         will try to reconnect to TM if it is not already connected
 **********************************************************************/
void *
socket_test_thread(void *arg)
{
  // loop until client process dies
  while (1) {
    if (socket_test(main_socket_fd) <= 0) {
      // ASSUMES that in between the time the socket_test is made
      // and this reconnect call is made, the main_socket_fd remains
      // the same (eg. no one else called reconnect to TM successfully!! 
      // WHAT IF in between this time, the client had issued a request
      // calling socket_write_conn which then calls reconnect(); then
      // reconnect will return an "ALREADY CONNECTED" error when it 
      // tries to connect, and on the next loop iteration, the socket_test
      // will actually pass because main_socket_fd is valid!!
      if (reconnect() == INK_ERR_OKAY) {
        //fprintf(stderr, "[socket_test_thread] reconnect succeeds\n");
      }
    }

    sleep(5);
  }

  ink_thread_exit(NULL);
  return NULL;
}

/**********************************************************************
 * MARSHALL REQUESTS
 **********************************************************************/
/**********************************************************************
 * send_request
 *
 * purpose: sends file read request to Traffic Manager
 * input:   fd - file descriptor to use to send to 
 *          op - the type of OpType request sending
 * output:  INK_ERR_xx
 * notes:  used by operations which don't need to send any additional
 *         parameters 
 * format: <OpType> <msg_len=0>
 **********************************************************************/
INKError
send_request(int fd, OpType op)
{
  ink16 op_t;
  ink32 msg_len;
  char msg_buf[SIZE_OP_T + SIZE_LEN];
  INKError err;

  // fill in op type
  op_t = (ink16) op;
  memcpy(msg_buf, &op_t, SIZE_OP_T);

  // fill in msg_len == 0
  msg_len = 0;
  memcpy(msg_buf + SIZE_OP_T, &msg_len, SIZE_LEN);

  // send message
  err = socket_write_conn(fd, msg_buf, SIZE_OP_T + SIZE_LEN);
  return err;
}

/**********************************************************************
 * send_request_name (helper fn)
 *
 * purpose: sends generic  request with one string argument name
 * input: fd - file descriptor to use
 *        op - .
 * output: INK_ERR_xx
 * note: format: <OpType> <str_len> <string>
 **********************************************************************/
INKError
send_request_name(int fd, OpType op, char *name)
{
  char *msg_buf;
  ink16 op_t;
  ink32 msg_len;
  int total_len;
  INKError err;

  if (name == NULL) {           //reg callback for all events when op==EVENT_REG_CALLBACK
    msg_len = 0;
  } else {
    msg_len = (ink32) strlen(name);
  }

  total_len = SIZE_OP_T + SIZE_LEN + msg_len;
  msg_buf = (char *) xmalloc(sizeof(char) * total_len);
  if (!msg_buf)
    return INK_ERR_SYS_CALL;

  // fill in op type
  op_t = (ink16) op;
  memcpy(msg_buf, (void *) &op_t, SIZE_OP_T);

  // fill in msg_len
  memcpy(msg_buf + SIZE_OP_T, (void *) &msg_len, SIZE_LEN);

  // fill in name (if NOT NULL)
  if (name)
    memcpy(msg_buf + SIZE_OP_T + SIZE_LEN, name, msg_len);


  // send message  
  err = socket_write_conn(fd, msg_buf, total_len);
  if (msg_buf)
    xfree(msg_buf);
  return err;
}

/**********************************************************************
 * send_request_name_value (helper fn)
 *
 * purpose: sends generic request with 2 str arguments; a name-value pair
 * input: fd - file descriptor to use
 *        op - Op type
 * output: INK_ERR_xx
 * note: format: <OpType> <name-len> <val-len> <name> <val>
 **********************************************************************/
INKError
send_request_name_value(int fd, OpType op, char *name, char *value)
{
  char *msg_buf;
  int msg_pos = 0, total_len;
  ink32 msg_len, name_len, val_size;    // these are written to msg
  ink16 op_t;
  INKError err;

  if (!name || !value)
    return INK_ERR_PARAMS;

  // set the sizes
  name_len = strlen(name);
  val_size = strlen(value);
  msg_len = (SIZE_LEN * 2) + name_len + val_size;
  total_len = SIZE_OP_T + SIZE_LEN + msg_len;
  msg_buf = (char *) xmalloc(sizeof(char) * (total_len));
  if (!msg_buf)
    return INK_ERR_SYS_CALL;

  // fill in op type
  op_t = (ink16) op;
  memcpy(msg_buf + msg_pos, (void *) &op_t, SIZE_OP_T);
  msg_pos += SIZE_OP_T;

  // fill in msg length
  memcpy(msg_buf + msg_pos, (void *) &msg_len, SIZE_LEN);
  msg_pos += SIZE_LEN;

  // fill in record name length
  memcpy(msg_buf + msg_pos, (void *) &name_len, SIZE_LEN);
  msg_pos += SIZE_LEN;

  // fill in record value length
  memcpy(msg_buf + msg_pos, (void *) &val_size, SIZE_LEN);
  msg_pos += SIZE_LEN;

  // fill in record name
  memcpy(msg_buf + msg_pos, name, name_len);
  msg_pos += name_len;

  // fill in record value
  memcpy(msg_buf + msg_pos, value, val_size);

  // send message
  err = socket_write_conn(fd, msg_buf, total_len);
  xfree(msg_buf);
  return err;
}

/**********************************************************************
 * send_file_read_request
 *
 * purpose: sends file read request to Traffic Manager
 * input:   fd - file descriptor to use to send to 
 *          file - file to read 
 * output:  INK_ERR_xx
 * notes:   first must create the message and then send it across network
 *          msg format = <OpType> <msg_len> <INKFileNameT>
 **********************************************************************/
INKError
send_file_read_request(int fd, INKFileNameT file)
{
  char *msg_buf;
  int msg_pos = 0, total_len;
  ink32 msg_len = (ink32) SIZE_FILE_T;  //marshalled values
  ink16 op, file_t;
  INKError err;

  total_len = SIZE_OP_T + SIZE_LEN + SIZE_FILE_T;
  msg_buf = (char *) xmalloc(sizeof(char) * total_len);
  if (!msg_buf)
    return INK_ERR_SYS_CALL;

  // fill in op type
  op = (ink16) FILE_READ;
  memcpy(msg_buf + msg_pos, &op, SIZE_OP_T);
  msg_pos += SIZE_OP_T;

  // fill in msg length
  memcpy(msg_buf + msg_pos, &msg_len, SIZE_LEN);
  msg_pos += SIZE_LEN;

  // fill in file type
  file_t = (ink16) file;
  memcpy(msg_buf + msg_pos, &file_t, SIZE_FILE_T);

  // send message
  err = socket_write_conn(fd, msg_buf, total_len);
  xfree(msg_buf);
  return err;

}

/**********************************************************************
 * send_file_write_request
 *
 * purpose: sends file write request to Traffic Manager
 * input: fd - file descriptor to use
 *        file - file to read 
 *        text - new text to write to specified file
 *        size - length of the text
 *        ver  - version of the file to be written
 * output: INK_ERR_xx
 * notes: format - FILE_WRITE <msg_len> <file_type> <file_ver> <file_size> <text>
 **********************************************************************/
INKError
send_file_write_request(int fd, INKFileNameT file, int ver, int size, char *text)
{
  char *msg_buf;
  int msg_pos = 0, total_len;
  ink32 msg_len, f_size;        //marshalled values
  ink16 op, file_t, f_ver;
  INKError err;

  if (!text)
    return INK_ERR_PARAMS;

  msg_len = SIZE_FILE_T + SIZE_VER + SIZE_LEN + size;
  total_len = SIZE_OP_T + SIZE_LEN + msg_len;
  msg_buf = (char *) xmalloc(sizeof(char) * total_len);
  if (!msg_buf)
    return INK_ERR_SYS_CALL;

  // fill in op type
  op = (ink16) FILE_WRITE;
  memcpy(msg_buf + msg_pos, &op, SIZE_OP_T);
  msg_pos += SIZE_OP_T;

  // fill in msg length
  memcpy(msg_buf + msg_pos, &msg_len, SIZE_LEN);
  msg_pos += SIZE_LEN;

  // fill in file type
  file_t = (ink16) file;
  memcpy(msg_buf + msg_pos, &file_t, SIZE_FILE_T);
  msg_pos += SIZE_FILE_T;

  // fill in file version
  f_ver = (ink16) ver;
  memcpy(msg_buf + msg_pos, &f_ver, SIZE_VER);
  msg_pos += SIZE_VER;

  // fill in file size
  f_size = (ink32) size;        //typecase to be safe
  memcpy(msg_buf + msg_pos, &f_size, SIZE_LEN);
  msg_pos += SIZE_LEN;

  // fill in text of file
  memcpy(msg_buf + msg_pos, text, size);

  // send message
  err = socket_write_conn(fd, msg_buf, total_len);
  xfree(msg_buf);
  return err;

}

/**********************************************************************
 * send_record_get_request
 *
 * purpose: sends request to get record value from Traffic Manager
 * input: fd       - file descriptor to use
 *        rec_name - name of record to retrieve value for
 * output: INK_ERR_xx
 * format: RECORD_GET <msg_len> <rec_name>
 **********************************************************************/
INKError
send_record_get_request(int fd, char *rec_name)
{
  char *msg_buf;
  int msg_pos = 0, total_len;
  ink16 op;
  ink32 msg_len;
  INKError err;

  if (!rec_name)
    return INK_ERR_PARAMS;

  total_len = SIZE_OP_T + SIZE_LEN + strlen(rec_name);
  msg_buf = (char *) xmalloc(sizeof(char) * total_len);
  if (!msg_buf)
    return INK_ERR_SYS_CALL;

  // fill in op type
  op = (ink16) RECORD_GET;
  memcpy(msg_buf + msg_pos, (void *) &op, SIZE_OP_T);
  msg_pos += SIZE_OP_T;

  // fill in msg length
  msg_len = (ink32) strlen(rec_name);
  memcpy(msg_buf + msg_pos, (void *) &msg_len, SIZE_LEN);
  msg_pos += SIZE_LEN;

  // fill in record name
  memcpy(msg_buf + msg_pos, rec_name, strlen(rec_name));

  // send message  
  err = socket_write_conn(fd, msg_buf, total_len);
  xfree(msg_buf);
  return err;

}


/*------ control functions -------------------------------------------*/
/**********************************************************************
 * send_proxy_state_get_request
 *
 * purpose: sends request to get the proxy state (on/off)
 * input: fd       - file descriptor to use
 * output: INK_ERR_xx
 * note: format: PROXY_STATE_GET 0(=msg_len)
 **********************************************************************/
INKError
send_proxy_state_get_request(int fd)
{
  INKError err;

  err = send_request(fd, PROXY_STATE_GET);
  return err;
}

/**********************************************************************
 * send_proxy_state_set_request
 *
 * purpose: sends request to set the proxy state (on/off)
 * input: fd    - file descriptor to use
 *        state - INK_PROXY_ON, INK_PROXY_OFF
 * output: INK_ERR_xx
 * note: format: PROXY_STATE_SET  <msg_len> <INKProxyStateT> <INKCacheClearT>
 **********************************************************************/
INKError
send_proxy_state_set_request(int fd, INKProxyStateT state, INKCacheClearT clear)
{
  ink16 op, state_t, cache_t;
  ink32 msg_len;
  int total_len;
  char *msg_buf;
  INKError err;

  total_len = SIZE_OP_T + SIZE_LEN + SIZE_PROXY_T + SIZE_TS_ARG_T;
  msg_buf = (char *) xmalloc(sizeof(char) * (total_len));
  if (!msg_buf)
    return INK_ERR_SYS_CALL;

  // fill in op type
  op = (ink16) PROXY_STATE_SET;
  memcpy(msg_buf, (void *) &op, SIZE_OP_T);

  // fill in msg_len
  msg_len = (ink32) (SIZE_PROXY_T + SIZE_TS_ARG_T);
  memcpy(msg_buf + SIZE_OP_T, (void *) &msg_len, SIZE_LEN);

  // fill in proxy state
  state_t = (ink16) state;
  memcpy(msg_buf + SIZE_OP_T + SIZE_LEN, (void *) &state_t, SIZE_PROXY_T);

  // fill in cache clearing option
  cache_t = (ink16) clear;
  memcpy(msg_buf + SIZE_OP_T + SIZE_LEN + SIZE_PROXY_T, (void *) &cache_t, SIZE_TS_ARG_T);

  // send message  
  err = socket_write_conn(fd, msg_buf, total_len);
  xfree(msg_buf);
  return err;

}

/**********************************************************************
 * send_restart_request
 *
 * purpose: sends request restart Traffic Manager and Traffic Server
 * input: fd      - file descriptor to use
 *        cluster - true= clusterwide, false= local restart
 * output: INK_ERR_xx
 * note: format: RESTART 2(=msg_len) <bool>
 **********************************************************************/
INKError
send_restart_request(int fd, bool cluster)
{
  char *msg_buf;
  ink16 op, clust_t;
  ink32 msg_len;
  int total_len;
  INKError err;

  total_len = SIZE_OP_T + SIZE_LEN + SIZE_BOOL;
  msg_buf = (char *) xmalloc(sizeof(char) * total_len);
  if (!msg_buf)
    return INK_ERR_SYS_CALL;

  // fill in op type
  op = (ink16) RESTART;
  memcpy(msg_buf, (void *) &op, SIZE_OP_T);

  // fill in msg_len = SIZE_BOOL
  msg_len = (ink32) SIZE_BOOL;
  memcpy(msg_buf + SIZE_OP_T, (void *) &msg_len, SIZE_LEN);

  // fill in cluster?; true=0, false=1
  if (cluster)
    clust_t = 0;
  else
    clust_t = 1;
  memcpy(msg_buf + SIZE_OP_T + SIZE_LEN, (void *) &clust_t, SIZE_BOOL);


  // send message  
  err = socket_write_conn(fd, msg_buf, total_len);
  xfree(msg_buf);
  return err;

}

/*------ events -------------------------------------------------------*/

/**********************************************************************
 * send_register_all_callbacks
 *
 * purpose: determines all events which have at least one callback registered
 *          and sends message to notify TM that this client has a callback
 *          registered for each event
 * input: None
 * output: return INK_ERR_OKAY only if ALL events sent okay
 * notes: could create a function which just sends a list of all the events to 
 * reregister; but actually just reuse the function 
 * send_request_name(EVENT_REG_CALLBACK) and call it for each event
 * 1) get list of all events with callbacks
 * 2) for each event, call send_request_name
 **********************************************************************/
INKError
send_register_all_callbacks(int fd, CallbackTable * cb_table)
{
  LLQ *events_with_cb;
  INKError err, send_err = INK_ERR_FAIL;
  bool no_errors = true;        // set to false if one send is not okay

  events_with_cb = get_events_with_callbacks(cb_table);
  // need to check that the list has all the events registered
  if (!events_with_cb) {        // all events have registered callback
    err = send_request_name(fd, EVENT_REG_CALLBACK, NULL);
    if (err != INK_ERR_OKAY)
      return err;
  } else {
    char *event_name;
    int event_id;
    int num_events = queue_len(events_with_cb);
    // iterate through the LLQ and send request for each event
    for (int i = 0; i < num_events; i++) {
      event_id = *(int *) dequeue(events_with_cb);
      event_name = (char *) get_event_name(event_id);
      if (event_name) {
        err = send_request_name(fd, EVENT_REG_CALLBACK, event_name);
        xfree(event_name);      // free memory
        if (err != INK_ERR_OKAY) {
          send_err = err;       // save the type of send error
          no_errors = false;
        }
      }
      // REMEMBER: WON"T GET A REPLY from TM side!
    }
  }

  if (events_with_cb)
    delete_queue(events_with_cb);

  if (no_errors)
    return INK_ERR_OKAY;
  else
    return send_err;
}

/**********************************************************************
 * send_unregister_all_callbacks
 *
 * purpose: determines all events which have no callback registered
 *          and sends message to notify TM that this client has no
 *          callbacks registered for that event
 * input: None
 * output: INK_ERR_OKAY only if all send requests are okay
 * notes: could create a function which just sends a list of all the events to 
 * unregister; but actually just reuse the function 
 * send_request_name(EVENT_UNREG_CALLBACK) and call it for each event
 **********************************************************************/
INKError
send_unregister_all_callbacks(int fd, CallbackTable * cb_table)
{
  char *event_name;
  int event_id;
  LLQ *events_with_cb;          // list of events with at least one callback
  int reg_callback[NUM_EVENTS];
  INKError err, send_err = INK_ERR_FAIL;
  bool no_errors = true;        // set to false if at least one send fails

  // init array so that all events don't have any callbacks
  for (int i = 0; i < NUM_EVENTS; i++) {
    reg_callback[i] = 0;
  }

  events_with_cb = get_events_with_callbacks(cb_table);
  if (!events_with_cb) {        // all events have a registered callback
    return INK_ERR_OKAY;
  } else {
    int num_events = queue_len(events_with_cb);
    // iterate through the LLQ and mark events that have a callback
    for (int i = 0; i < num_events; i++) {
      event_id = *(int *) dequeue(events_with_cb);
      reg_callback[event_id] = 1;       // mark the event as having a callback
    }
    delete_queue(events_with_cb);
  }

  // send message to TM to mark unregister
  for (int k = 0; k < NUM_EVENTS; k++) {
    if (reg_callback[k] == 0) { // event has no registered callbacks
      event_name = get_event_name(k);
      err = send_request_name(fd, EVENT_UNREG_CALLBACK, event_name);
      xfree(event_name);
      if (err != INK_ERR_OKAY) {
        send_err = err;         //save the type of the sending error
        no_errors = false;
      }
      // REMEMBER: WON"T GET A REPLY!
      // only the event_poll_thread_main does any reading of the event_socket;
      // so DO NOT parse reply b/c a reply won't be sent            
    }
  }

  if (no_errors)
    return INK_ERR_OKAY;
  else
    return send_err;
}

/**********************************************************************
 * send_diags_msg
 *
 * purpose: sends the diag msg across along with they diag msg type
 * input: mode - type of diags msg
 *        msg  - the diags msg
 * output: INK_ERR_xx
 * note: format: <OpType> <msg_len> <INKDiagsT> <diag_msg_len> <diag_msg>
 **********************************************************************/
INKError
send_diags_msg(int fd, INKDiagsT mode, const char *diag_msg)
{
  char *msg_buf;
  ink16 op_t, diag_t;
  ink32 msg_len, diag_msg_len;
  int total_len;
  INKError err;

  if (!diag_msg)
    return INK_ERR_PARAMS;

  diag_msg_len = (ink32) strlen(diag_msg);
  msg_len = SIZE_DIAGS_T + SIZE_LEN + diag_msg_len;
  total_len = SIZE_OP_T + SIZE_LEN + msg_len;
  msg_buf = (char *) xmalloc(sizeof(char) * total_len);
  if (!msg_buf)
    return INK_ERR_SYS_CALL;

  // fill in op type
  op_t = (ink16) DIAGS;
  memcpy(msg_buf, (void *) &op_t, SIZE_OP_T);

  // fill in entire msg len
  memcpy(msg_buf + SIZE_OP_T, (void *) &msg_len, SIZE_LEN);

  // fill in INKDiagsT
  diag_t = (ink16) mode;
  memcpy(msg_buf + SIZE_OP_T + SIZE_LEN, (void *) &diag_t, SIZE_DIAGS_T);

  // fill in diags msg_len
  memcpy(msg_buf + SIZE_OP_T + SIZE_LEN + SIZE_DIAGS_T, (void *) &diag_msg_len, SIZE_LEN);

  // fill in diags msg
  memcpy(msg_buf + SIZE_OP_T + SIZE_LEN + SIZE_DIAGS_T + SIZE_LEN, diag_msg, diag_msg_len);

  // send message  
  err = socket_write_conn(fd, msg_buf, total_len);
  if (msg_buf)
    xfree(msg_buf);
  return err;
}


/**********************************************************************
 * UNMARSHAL REPLIES
 **********************************************************************/

/* Error handling implementation: 
 * All the parsing functions which parse the reply returned from local side
 * also must read the INKERror return value sent from local side; this return 
 * value is the same value that will be returned by the parsing function.
 * ALL PARSING FUNCTIONS MUST FIRST CHECK that the retval is INK_ERR_OKAY;
 * if it is not, then DON"T PARSE THE REST OF THE REPLY!!
 */

/* Reading replies:
 * The reading is done in while loop in the parse_xx_reply functions; 
 * need to add a timeout so that the function is not left looping and 
 * waiting if a msg isn't sent to the socket from local side (eg. TM died)
 */

/**********************************************************************
 * parse_reply
 *
 * purpose: parses a reply from traffic manager. return that error
 * input: fd 
 * output: errors on error or fill up class with response & 
 *         return INK_ERR_xx
 * notes: only returns an INKError 
 **********************************************************************/
INKError
parse_reply(int fd)
{
  int ret, amount_read = 0;
  ink16 ret_val;

  // check to see if anything to read; wait for specified time = 1 sec
  if (socket_read_timeout(fd, MAX_TIME_WAIT, 0) <= 0) { // time expires before ready to read
    return INK_ERR_NET_TIMEOUT;
  }
  // get the return value (INKError type)
  while (amount_read < SIZE_ERR_T) {
    ret = read(fd, (void *) &ret_val, SIZE_ERR_T - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }

  return (INKError) ret_val;

}

/**********************************************************************
 * parse_reply_list
 *
 * purpose: parses a TM reply to a request to get a list of string tokens
 * input: fd - socket to read
 *        list - will contain delimited string list of tokens
 * output: INK_ERR_xx
 * notes:
 * format: <INKError> <string_list_len> <delimited_string_list>
 **********************************************************************/
INKError
parse_reply_list(int fd, char **list)
{
  int ret, amount_read = 0;
  ink16 ret_val;
  ink32 list_size;
  INKError err_t;

  if (!list)
    return INK_ERR_PARAMS;

  // check to see if anything to read; wait for specified time
  if (socket_read_timeout(fd, MAX_TIME_WAIT, 0) <= 0) {
    return INK_ERR_NET_TIMEOUT;
  }
  // get the return value (INKError type)
  while (amount_read < SIZE_ERR_T) {
    ret = read(fd, (void *) &ret_val, SIZE_ERR_T - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }
  // if !INK_ERR_OKAY, stop reading rest of msg
  err_t = (INKError) ret_val;
  if (err_t != INK_ERR_OKAY) {
    return err_t;
  }
  // now get size of string event list
  amount_read = 0;
  while (amount_read < SIZE_LEN) {
    ret = read(fd, (void *) &list_size, SIZE_LEN - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }

  // get the delimited event list string
  *list = (char *) xmalloc(sizeof(char) * (list_size + 1));
  if (!(*list)) {
    return INK_ERR_SYS_CALL;
  }

  amount_read = 0;
  while (amount_read < list_size) {
    ret = read(fd, (void *) *list, list_size - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        xfree(*list);
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      xfree(*list);
      return INK_ERR_NET_EOF;
    }

    amount_read += ret;
  }
  // add end of string to end of the record value
  ((char *) (*list))[list_size] = '\0';

  return err_t;
}


/**********************************************************************
 * parse_file_read_reply
 *
 * purpose: parses a file read reply from traffic manager. 
 * input: fd 
 *        ver -
 *        size - size of text
 *        text - 
 * output: errors on error or fill up class with response & 
 *         return INK_ERR_xx
 * notes: reply format = <INKError> <file_version> <file_size> <text>
 **********************************************************************/
INKError
parse_file_read_reply(int fd, int *ver, int *size, char **text)
{
  int ret, amount_read = 0;
  ink32 f_size;
  ink16 ret_val, f_ver;
  INKError err_t;

  if (!ver || !size || !text)
    return INK_ERR_PARAMS;

  // check to see if anything to read; wait for specified time
  if (socket_read_timeout(fd, MAX_TIME_WAIT, 0) <= 0) { // time expires before ready to read
    return INK_ERR_NET_TIMEOUT;
  }
  // get the error return value 
  while (amount_read < SIZE_ERR_T) {
    ret = read(fd, (void *) &ret_val, SIZE_ERR_T - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }

    amount_read += ret;
  }
  // if !INK_ERR_OKAY, stop reading rest of msg
  err_t = (INKError) ret_val;
  if (err_t != INK_ERR_OKAY) {
    return err_t;
  }
  // now get file version
  amount_read = 0;
  while (amount_read < SIZE_VER) {
    ret = read(fd, (void *) &f_ver, SIZE_VER - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }

    amount_read += ret;
  }
  *ver = (int) f_ver;

  // now get file size
  amount_read = 0;
  while (amount_read < SIZE_LEN) {
    ret = read(fd, (void *) &f_size, SIZE_LEN - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }
  *size = (int) f_size;

  // check size before reading text
  if ((*size) <= 0) {
    *text = "";                 // set to empty string
  } else {
    // now we got the size, we can read everything into our msg * then parse it
    *text = (char *) xmalloc(sizeof(char) * (f_size + 1));
    if (!(*text)) {
      return INK_ERR_SYS_CALL;
    }

    amount_read = 0;
    while (amount_read < f_size) {
      ret = read(fd, (void *) *text, f_size - amount_read);

      if (ret < 0) {
        if (errno == EAGAIN)
          continue;
        else {
          xfree(*text);
          return INK_ERR_NET_READ;
        }
      }

      if (ret == 0) {
        xfree(*text);
        return INK_ERR_NET_EOF;
      }

      amount_read += ret;
    }
    (*text)[f_size] = '\0';     // end the string
  }

  return err_t;
}

/**********************************************************************
 * parse_record_get_reply
 *
 * purpose: parses a record_get reply from traffic manager. 
 * input: fd 
 *        retval   -
 *        rec_type - the type of the record
 *        rec_value - the value of the record in string format 
 * output: errors on error or fill up class with response & 
 *         return SUCC
 * notes: reply format = <INKError> <val_size> <rec_type> <record_value> 
 * It's the responsibility of the calling function to conver the rec_value 
 * based on the rec_type!! 
 **********************************************************************/
INKError
parse_record_get_reply(int fd, INKRecordT * rec_type, void **rec_val)
{
  int ret, amount_read = 0;
  ink16 ret_val, rec_t;
  ink32 rec_size;
  INKError err_t;

  if (!rec_type || !rec_val)
    return INK_ERR_PARAMS;

  // check to see if anything to read; wait for specified time 
  if (socket_read_timeout(fd, MAX_TIME_WAIT, 0) <= 0) { //time expired before ready to read
    return INK_ERR_NET_TIMEOUT;
  }
  // get the return value (INKError type)
  while (amount_read < SIZE_ERR_T) {
    ret = read(fd, (void *) &ret_val, SIZE_ERR_T - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }
  // if !INK_ERR_OKAY, stop reading rest of msg
  err_t = (INKError) ret_val;
  if (err_t != INK_ERR_OKAY) {
    return err_t;
  }
  // now get size of record_value
  amount_read = 0;
  while (amount_read < SIZE_LEN) {
    ret = read(fd, (void *) &rec_size, SIZE_LEN - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }

  // get the record type
  amount_read = 0;
  while (amount_read < SIZE_REC_T) {
    ret = read(fd, (void *) &rec_t, SIZE_REC_T - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN) {
        continue;
      } else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }
  *rec_type = (INKRecordT) rec_t;

  // get record value
  // allocate correct amount of memory for record value
  if (*rec_type == INK_REC_STRING)
    *rec_val = xmalloc(sizeof(char) * (rec_size + 1));
  else
    *rec_val = xmalloc(sizeof(char) * (rec_size));

  if (!(*rec_val)) {
    return INK_ERR_SYS_CALL;
  }

  amount_read = 0;
  while (amount_read < rec_size) {
    ret = read(fd, (void *) *rec_val, rec_size - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        xfree(*rec_val);
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      xfree(*rec_val);
      return INK_ERR_NET_EOF;
    }

    amount_read += ret;
  }
  // add end of string to end of the record value
  if (*rec_type == INK_REC_STRING)
    ((char *) (*rec_val))[rec_size] = '\0';

  return err_t;
}

/**********************************************************************
 * parse_record_set_reply
 *
 * purpose: parses a record_set reply from traffic manager. 
 * input: fd 
 *        action_need - will contain the type of action needed from the set
 * output: INK_ERR_xx
 * notes: reply format = <INKError> <val_size> <rec_type> <record_value> 
 * It's the responsibility of the calling function to conver the rec_value 
 * based on the rec_type!! 
 **********************************************************************/
INKError
parse_record_set_reply(int fd, INKActionNeedT * action_need)
{
  int ret, amount_read = 0;
  ink16 ret_val, action_t;
  INKError err_t;

  if (!action_need)
    return INK_ERR_PARAMS;

  // check to see if anything to read; wait for specified time 
  if (socket_read_timeout(fd, MAX_TIME_WAIT, 0) <= 0) {
    return INK_ERR_NET_TIMEOUT;
  }
  // get the return value (INKError type)
  while (amount_read < SIZE_ERR_T) {
    ret = read(fd, (void *) &ret_val, SIZE_ERR_T - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }
  // if !INK_ERR_OKAY, stop reading rest of msg
  err_t = (INKError) ret_val;
  if (err_t != INK_ERR_OKAY) {
    return err_t;
  }
  // now get the action needed
  amount_read = 0;
  while (amount_read < SIZE_ACTION_T) {
    ret = read(fd, (void *) &action_t, SIZE_ACTION_T - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }
  *action_need = (INKActionNeedT) action_t;

  return err_t;
}


/**********************************************************************
 * parse_proxy_state_get_reply
 *
 * purpose: parses a TM reply to a PROXY_STATE_GET request
 * input: fd 
 *        state - will contain the state of the proxy
 * output: INK_ERR_xx
 * notes: function is DIFFERENT becuase it has NO INKError at head of msg
 * format: <INKProxyStateT> 
 **********************************************************************/
INKError
parse_proxy_state_get_reply(int fd, INKProxyStateT * state)
{
  int ret, amount_read = 0;
  ink16 state_t;

  if (!state)
    return INK_ERR_PARAMS;

  // check to see if anything to read; wait for specified time 
  if (socket_read_timeout(fd, MAX_TIME_WAIT, 0) <= 0) { // time expires before ready to read
    return INK_ERR_NET_TIMEOUT;
  }
  // now get proxy state
  amount_read = 0;
  while (amount_read < SIZE_PROXY_T) {
    ret = read(fd, (void *) &state_t, SIZE_PROXY_T - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }
  *state = (INKProxyStateT) state_t;

  return INK_ERR_OKAY;
}

/*------------- events ---------------------------------------------*/

/**********************************************************************
 * parse_event_active_reply
 *
 * purpose: parses a TM reply to a request to get status of an event
 * input: fd - socket to read
 *        is_active - set to true if event is active; false otherwise
 * output: INK_ERR_xx
 * notes:
 * format: reply format = <INKError> <bool>
 **********************************************************************/
INKError
parse_event_active_reply(int fd, bool * is_active)
{
  int ret, amount_read = 0;
  ink16 ret_val, active;
  INKError err_t;

  if (!is_active)
    return INK_ERR_PARAMS;

  // check to see if anything to read; wait for specified time 
  if (socket_read_timeout(fd, MAX_TIME_WAIT, 0) <= 0) {
    return INK_ERR_NET_TIMEOUT;
  }
  // get the return value (INKError type)
  while (amount_read < SIZE_ERR_T) {
    ret = read(fd, (void *) &ret_val, SIZE_ERR_T - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }
  // if !INK_ERR_OKAY, stop reading rest of msg
  err_t = (INKError) ret_val;
  if (err_t != INK_ERR_OKAY) {
    return err_t;
  }
  // now get the boolean
  amount_read = 0;
  while (amount_read < SIZE_BOOL) {
    ret = read(fd, (void *) &active, SIZE_BOOL - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        return INK_ERR_NET_READ;
      }
    }

    if (ret == 0) {
      return INK_ERR_NET_EOF;
    }
    // all is good here :)
    amount_read += ret;
  }
  *is_active = (bool) active;

  return err_t;
}

/**********************************************************************
 * parse_event_notification
 *
 * purpose: parses the event notification message from TM when an event
 *          is signalled; stores the event info in the INKEvent
 * input: fd - socket to read
 *        event - where the event info from msg is stored
 * output:INK_ERR_OKAY, INK_ERR_NET_READ, INK_ERR_NET_EOF, INK_ERR_PARAMS
 * notes:
 * format: <OpType> <event_name_len> <event_name> <desc_len> <desc>
 **********************************************************************/
INKError
parse_event_notification(int fd, INKEvent * event)
{
  int amount_read, ret;
  OpType msg_type;
  ink16 type_op;
  ink32 msg_len;
  char *event_name = NUL, *desc = NULL;

  if (!event)
    return INK_ERR_PARAMS;

  // read the operation type; should be EVENT_NOTIFY
  amount_read = 0;
  while (amount_read < SIZE_OP_T) {
    ret = read(fd, (void *) &type_op, SIZE_OP_T - amount_read);

    // the thread can receive a bad file descriptor error(EBADF) 
    // if the current socket_fd being used by this thread is invalid; 
    // this occurs when TM restarts and the client has to reconnect and
    // get a new socket_fd; in this case, this thread will return null 
    // and die; and the client will launch a new event_poll_thread_main 
    // when it reconnects to TM

    // connection broken or error
    if ((ret < 0) && (errno != EAGAIN)) {
      //fprintf(stderr, "[event_poll_thread_main] ERROR read event socket %d: %s\n", sock_fd, strerror((int)errno));
      goto ERROR_READ;
    }

    if (ret == 0) {
      goto ERROR_EOF;
    }

    // if we read some bytes keep on going.
    amount_read += ret;
  }

  // got the message type; the msg_type should be EVENT_NOTIFY
  msg_type = (OpType) type_op;
  if (msg_type != EVENT_NOTIFY)
    return INK_ERR_FAIL;
  //fprintf(stderr, "[event_poll_thread_main] received EVENT_NOTIFY from TM\n");

  // read in event name length
  amount_read = 0;
  while (amount_read < SIZE_LEN) {
    ret = read(fd, (void *) &msg_len, SIZE_LEN - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        //fprintf(stderr, "[event_poll_thread_main] EXIT: error reading\n");
        goto ERROR_READ;
      }
    }

    if (ret == 0) {
      goto ERROR_EOF;
    }

    amount_read += ret;
  }

  // read the event name
  event_name = (char *) xmalloc(sizeof(char) * (msg_len + 1));
  amount_read = 0;
  while (amount_read < msg_len) {
    ret = read(fd, (void *) event_name, msg_len - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        goto ERROR_READ;
      }
    }

    if (ret == 0) {
      goto ERROR_EOF;
    }

    amount_read += ret;
  }
  event_name[msg_len] = '\0';   // end the string

  // read in event description length
  amount_read = 0;
  while (amount_read < SIZE_LEN) {
    ret = read(fd, (void *) &msg_len, SIZE_LEN - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        goto ERROR_READ;
      }
    }

    if (ret == 0) {
      goto ERROR_EOF;
    }

    amount_read += ret;
  }

  // read the event description
  desc = (char *) xmalloc(sizeof(char) * (msg_len + 1));
  amount_read = 0;
  while (amount_read < msg_len) {
    ret = read(fd, (void *) desc, msg_len - amount_read);

    if (ret < 0) {
      if (errno == EAGAIN)
        continue;
      else {
        goto ERROR_READ;
      }
    }

    if (ret == 0) {
      goto ERROR_EOF;
    }

    amount_read += ret;
  }
  desc[msg_len] = '\0';         // end the string

  // fill in event info
  event->name = event_name;
  event->id = (int) get_event_id(event_name);
  event->description = desc;

  return INK_ERR_OKAY;

ERROR_READ:
  if (event_name)
    xfree(event_name);
  if (desc)
    xfree(desc);
  return INK_ERR_NET_READ;

ERROR_EOF:
  if (event_name)
    xfree(event_name);
  if (desc)
    xfree(desc);
  return INK_ERR_NET_EOF;

}
