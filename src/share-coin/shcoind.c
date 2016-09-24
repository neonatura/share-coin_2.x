
/*
 * @copyright
 *
 *  Copyright 2014 Neo Natura
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

#include "shcoind.h"
#include <signal.h>

shpeer_t *server_peer;
int server_msgq;
shbuf_t *server_msg_buff;
shtime_t server_start_t;

static int opt_no_fork;

static int _rpc_thread_running;



void shcoind_term(void)
{
  int idx;

  /* terminate stratum server */
  stratum_term();

  for (idx = 1; idx < MAX_COIN_IFACE; idx++) {
#ifndef USDE_SERVICE
    if (idx == USDE_COIN_IFACE)
      continue;
#endif
    unet_unbind(idx);
  }

  shpeer_free(&server_peer);
  shbuf_free(&server_msg_buff);

  if (_rpc_thread_running) {
    /* terminate coin server */
    server_shutdown();
  }

  /* de-allocation options */
  opt_term();

}

void usage_help(void)
{
  fprintf(stdout,
      "Usage: shcoind [OPTIONS]\n"
      "Virtual currency daemon for the Share Library Suite.\n"
      "\n"
      "Network Options:\n"
      "\t--max-conn <#>\tThe maximum number of incoming coin-service connections.\n"
      "\t--no-seed\tPrevent pre-defined seed IP addresses from being used.\n"
      "\t--check-addr\tRe-verify the external IP address used by this machine.\n"
      "\n"
      "Peer Options:\n"
      "\t--ban-span <#>\tThe number of seconds a peer will be banned for.\n"
      "\t--ban-threshold <#>\tThe degree of misbehaviour before a peer is disconnected.\n"
      "\n"
      "Diagnostic Options:\n"
      "\t--debug\t\tLog verbose debugging information.\n"
      "\t-nf\t\tRun daemon in foreground (no fork).\n"
      "\n"
      "Persistent Preferences:\n"
      "\tshcoind.debug\t\tSee '--debug' command-line option.\n"
      "\tshcoind.net.max\t\tSee '--conn-max' command-line option.\n"
      "\tshcoind.net.seed\tWhether to use pre-defined seed IP addresses.\n"
      "\tshcoind.ban.span\tSee '--ban-span' command-line option.\n"
      "\tshcoind.ban.threshold\tSee '--ban-threshold' command-line option.\n"
      "\n"
      "Note: Run \"shpref <name> <val>\" to set persistent preferences.\n"
      "\n"
      "Visit 'http://docs.sharelib.net/' for libshare API documentation."
      "Report bugs to <support@neo-natura.com>.\n"
      );
//      "\t--rescan\t\tRescan blocks for missing wallet transactions.\n"
}
void usage_version(void)
{
  fprintf(stdout,
      "shcoind version %s\n"
      "\n"
      "Copyright 2013 Neo Natura\n" 
      "Licensed under the GNU GENERAL PUBLIC LICENSE Version 3\n"
      "This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/)\n",
      get_libshare_version());
}

extern bc_t *GetBlockChain(CIface *iface);

int main(int argc, char *argv[])
{
  CIface *iface;
  bc_t *bc;
  char buf[1024];
  int idx;
  int fd;
  int err;
  int i;

  if (argc >= 2 &&
      (0 == strcmp(argv[1], "-h") ||
       0 == strcmp(argv[1], "--help"))) {
    usage_help();
    return (0);
  }
  if (argc >= 2 &&
      (0 == strcmp(argv[1], "-v") ||
       0 == strcmp(argv[1], "--version"))) {
    usage_version();
    return (0);
  }

  server_start_t = shtime();

  /* initialize options */
  opt_init();

  /* always perform 'fresh' tx rescan */

  for (i = 1; i < argc; i++) {
    if (0 == strcmp(argv[i], "-nf")) {
      opt_no_fork = TRUE;
    } else if (0 == strcmp(argv[i], "--max-conn")) {
      if (i + 1 < argc && isdigit(argv[i+1][0])) {
        i++;
        if (isdigit(argv[i][0]))
          opt_num_set(OPT_MAX_CONN, MAX(0, atoi(argv[i])));
      }
    } else if (0 == strcmp(argv[i], "--no-seed")) {
      opt_bool_set(OPT_PEER_SEED, FALSE);
    } else if (0 == strcmp(argv[i], "--check-addr")) {
      shpref_set("shcoind.net.addr", ""); /* clear cached IP addr */
    } else if (0 == strcmp(argv[i], "--ban-span")) {
      if (i + 1 < argc && isdigit(argv[i+1][0])) {
        i++;
        if (isdigit(argv[i][0]))
          opt_num_set(OPT_BAN_SPAN, MAX(1, atoi(argv[i])));
      }
    } else if (0 == strcmp(argv[i], "--ban-threshold")) {
      if (i + 1 < argc && isdigit(argv[i+1][0])) {
        i++;
        if (isdigit(argv[i][0]))
          opt_num_set(OPT_BAN_THRESHOLD, MAX(1, atoi(argv[i])));
      }
#if 0
    } else if (0 == strcmp(argv[i], "--rescan")) {
      SoftSetBoolArg("-rescan", true);
#endif
    }
  }

  if (!opt_no_fork)
    daemon(0, 1);

  shcoind_signal_init();

  /* initialize libshare */
  server_peer = shapp_init("shcoind", "127.0.0.1:9448", 0);
  server_msgq = shmsgget(NULL); /* shared server msg-queue */
  server_msg_buff = shbuf_init();

  shapp_listen(TX_APP, server_peer);
  shapp_listen(TX_IDENT, server_peer);
  shapp_listen(TX_SESSION, server_peer);
  shapp_listen(TX_BOND, server_peer);

  /* initialize coin interface's block-chain */
  for (idx = 1; idx < MAX_COIN_IFACE; idx++) {
    CIface *iface = GetCoinByIndex(idx);
    if (!iface || !iface->enabled)
      continue;

#ifndef USDE_SERVICE
    if (idx == USDE_COIN_IFACE) {
      iface->enabled = FALSE;
      continue;
    }
#endif

    if (iface->op_init) {
      err = iface->op_init(iface, NULL);
      if (err) {
        fprintf(stderr, "critical: unable to initialize %s service (%s).", iface->name, sherrstr(err));
        exit(1);
      }
    }
  }

  /* initialize coin interface's network service */
  for (idx = 1; idx < MAX_COIN_IFACE; idx++) {
    CIface *iface = GetCoinByIndex(idx);
    if (!iface || !iface->enabled)
      continue;

#ifndef USDE_SERVICE
    if (idx == USDE_COIN_IFACE) {
      iface->enabled = FALSE;
      continue;
    }
#endif

    if (iface->op_init) {
      err = iface->op_bind(iface, NULL);
      if (err) {
        fprintf(stderr, "critical: unable to initialize %s service (%s).", iface->name, sherrstr(err));
        exit(1);
      }
    }
  }

#ifdef STRATUM_SERVICE
  /* initialize stratum server */
  err = stratum_init();
  if (err) {
    fprintf(stderr, "critical: init stratum: %s. [sherr %d]", sherrstr(err), err);
    raise(SIGTERM);
  }
#endif

  start_node();

#ifdef RPC_SERVICE
  _rpc_thread_running = TRUE;
  start_rpc_server();
#endif

  /* unet_cycle() */
  daemon_server();


  return (0);
}

shpeer_t *shcoind_peer(void)
{
  return (server_peer);
}
