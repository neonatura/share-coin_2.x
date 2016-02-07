
/*
 * @copyright
 *
 *  Copyright 2015 Neo Natura
 *
 *  This file is part of the Share Library.
 *  (https://github.com/neonatura/share)
 *        
 *  The Share Library is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version. 
 *
 *  The Share Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Share Library.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  @endcopyright
 */  

#ifndef __UNET__UNET_H__
#define __UNET__UNET_H__

#ifdef __cplusplus
extern "C" {
#endif


#define UNET_NONE 0
#define UNET_STRATUM 1
#define UNET_RPC 2
#define UNET_COIN 3
#define MAX_UNET_MODES 4



#define UNETF_SHUTDOWN (1 << 0)
#define UNETF_PEER_SCAN (1 << 1)
#define UNETF_INBOUND (1 << 2)


#define UNDEFINED_SOCKET 0
#define UNDEFINED_TIME SHTIME_UNDEFINED

#ifdef WIN32
#define MSG_NOSIGNAL        0
#define MSG_DONTWAIT        0
typedef int socklen_t;
#else
#define INVALID_SOCKET      (SOCKET)(~0)
#define SOCKET_ERROR        -1
#endif

#define MAX_UNET_SOCKETS 4096

#define MAX_CONNECT_IDLE_TIME 45
#define MAX_IDLE_TIME 3600

#define UNET_MAX_EVENTS 10240

#define UEVENT_NONE 0
#define UEVENT_PEER 1


typedef unsigned int SOCKET;
 
typedef void (*unet_op)(void);

typedef void (*unet_addr_op)(int, struct sockaddr *);


/* per unet mode */
typedef struct unet_bind_t
{
  /** the socket descriptor of the listening socket. */
  int fd;

  /** bitvector flags (UNETF_XXX) */
  int flag;

  /** the port listening for new connections. */
  int port;

  /** the frequency of successfull connections. */
  double scan_freq;

  /* a public peer reference to the bound server. */
  shpeer_t peer;

  /** the last time the timer callback was called. */
  shtime_t timer_stamp;

  /** the last time a new peer connection was attempted. */
  shtime_t scan_stamp;

  /** the timer callback */
  unet_op op_timer;

  /** called when a new socket is accepted. */
  unet_addr_op op_accept;
  unet_addr_op op_close;
} unet_bind_t;

/* per client socket connection */
typedef struct unet_table_t
{

  /** server modes (UNET_XXX) */
  int mode;

  /** the underlying socket file descriptor. */
  SOCKET fd;

  /** bitvector flags (UNETF_XXX) */
  int flag;

  /** The time-stamp of when the connection initially occurred. */
  shtime_t cstamp;

  /** The last time that I/O was processed on socket. */
  shtime_t stamp;

  /** incoming data buffer */
  shbuf_t *rbuff;

  /** outgoing data buffer */
  shbuf_t *wbuff;

  /* remote network address */
  struct sockaddr net_addr;
} unet_table_t;

typedef struct uevent_t
{
  int mode;
  int flag;
  int type;
  int fd;
  void *data;
} uevent_t;





/**
 * Write a log message from inside the unet network engine.
 */
#define unet_log(_mode, _text) \
  (f_shcoind_log(0, ((_mode != 0) ? unet_mode_label(_mode) : "Info"), \
     (_text), __FILE__, __LINE__))


const char *unet_mode_label(int mode);

int unet_add(int mode, SOCKET sk);


int unet_mode(SOCKET sk);



unet_bind_t *unet_bind_table(int mode);




unet_table_t *get_unet_table(SOCKET sk);

int unet_accept(int mode, SOCKET *sk_p);


int unet_close(SOCKET sk, char *tag);

int unet_close_all(int mode);


int unet_sbuff_add(int sk, unsigned char *data, size_t data_len);

int unet_rbuff_add(int sk, unsigned char *data, size_t data_len);


int unet_read(SOCKET sk, char *data, size_t *data_len_p);

int unet_write(SOCKET sk, char *data, size_t data_len);


int unet_timer_set(int mode, unet_op timer_f);
void unet_timer_unset(int mode);


int unet_connect(int mode, struct sockaddr *net_addr, SOCKET *sk_p);


int unet_bind(int mode, int port);

int unet_unbind(int mode);
void unet_bind_flag_set(int mode, int flags);
void unet_bind_flag_unset(int mode, int flags);


void unet_cycle(double max_t);

void unet_shutdown(SOCKET sk);

void unet_connop_set(int mode, unet_addr_op accept_op);

void unet_disconnop_set(int mode, unet_addr_op close_op);


void unet_peer_scan(void);

int unet_peer_wait(unet_bind_t *bind);

uevent_t *uevent_new_peer(int umode, shpeer_t *peer);

void uevent_cycle(void);

unsigned int uevent_type_count(int type);

void unet_peer_fill(int mode);



#ifdef __cplusplus
}
#endif

#endif /* ndef __UNET__UNET_H__ */

