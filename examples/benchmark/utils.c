/*
 * utils.c - Utility functions for MRC example applications
 *
 * Copyright (c) 2025 Microsoft Corporation
 * Licensed under MIT License
 */

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

// ---------- Logging System ----------
enum mrc_log_level g_mrc_log_level = MRC_LOG_LEVEL_INFO;

void mrc_log_init(void) {
  const char *env_level = getenv("MRC_LOG_LEVEL");
  if (!env_level) {
    g_mrc_log_level = MRC_LOG_LEVEL_INFO;
    return;
  }

  if (!strcasecmp(env_level, "ERROR") || !strcmp(env_level, "0")) {
    g_mrc_log_level = MRC_LOG_LEVEL_ERROR;
  } else if (!strcasecmp(env_level, "WARN") ||
             !strcasecmp(env_level, "WARNING") || !strcmp(env_level, "1")) {
    g_mrc_log_level = MRC_LOG_LEVEL_WARN;
  } else if (!strcasecmp(env_level, "INFO") || !strcmp(env_level, "2")) {
    g_mrc_log_level = MRC_LOG_LEVEL_INFO;
  } else if (!strcasecmp(env_level, "DEBUG") || !strcmp(env_level, "3")) {
    g_mrc_log_level = MRC_LOG_LEVEL_DEBUG;
  } else {
    fprintf(stderr, "[MRC WARN] Unknown log level '%s', using INFO\n",
            env_level);
    g_mrc_log_level = MRC_LOG_LEVEL_INFO;
  }
}

void mrc_log_set_level(enum mrc_log_level level) { g_mrc_log_level = level; }

// ---------- TCP ----------
int tcp_listen_(int port) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    MRC_LOG_ERROR("Failed to create socket: %s", strerror(errno));
    return -1;
  }
  int on = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    MRC_LOG_WARN("Failed to set SO_REUSEADDR: %s", strerror(errno));
  }
  struct sockaddr_in a = {0};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = INADDR_ANY;
  a.sin_port = htons(port);
  if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
    MRC_LOG_ERROR("Failed to bind to port %d: %s", port, strerror(errno));
    close(s);
    return -1;
  }
  if (listen(s, 1) < 0) {
    MRC_LOG_ERROR("Failed to listen on socket: %s", strerror(errno));
    close(s);
    return -1;
  }
  MRC_LOG_DEBUG("TCP listening on port %d", port);
  return s;
}

int tcp_accept_(int ls) {
  struct sockaddr_in c;
  socklen_t l = sizeof(c);
  int s = accept(ls, (struct sockaddr *)&c, &l);
  if (s < 0) {
    MRC_LOG_ERROR("Failed to accept connection: %s", strerror(errno));
    return -1;
  }
  char ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &c.sin_addr, ip_str, sizeof(ip_str));
  MRC_LOG_DEBUG("Accepted connection from %s:%d", ip_str, ntohs(c.sin_port));
  return s;
}

int tcp_connect_(const char *ip, int port) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    MRC_LOG_ERROR("Failed to create socket: %s", strerror(errno));
    return -1;
  }
  struct sockaddr_in a = {0};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) {
    MRC_LOG_ERROR("Invalid IP address: %s", ip);
    close(s);
    return -1;
  }
  if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
    MRC_LOG_ERROR("Failed to connect to %s:%d: %s", ip, port, strerror(errno));
    close(s);
    return -1;
  }
  MRC_LOG_DEBUG("Connected to %s:%d", ip, port);
  return s;
}

int xfer(int fd, void *buf, size_t len, bool sendd) {
  uint8_t *p = buf;
  size_t total = len;
  while (len) {
    ssize_t r = sendd ? send(fd, p, len, 0) : recv(fd, p, len, MSG_WAITALL);
    if (r <= 0) {
      if (r == 0) {
        MRC_LOG_ERROR("TCP connection closed during %s",
                      sendd ? "send" : "recv");
      } else {
        MRC_LOG_ERROR("TCP %s failed: %s", sendd ? "send" : "recv",
                      strerror(errno));
      }
      return -1;
    }
    p += r;
    len -= (size_t)r;
  }
  MRC_LOG_DEBUG("TCP %s: %zu bytes", sendd ? "sent" : "received", total);
  return 0;
}

// ---------- helpers ----------
int gid_to_str(const union ibv_gid *g, char *out, size_t out_len) {
  if (!g || !out || out_len < 40) {
    MRC_LOG_ERROR("Invalid parameters to gid_to_str");
    return -1;
  }
  unsigned char *b = (unsigned char *)g->raw;
  int ret = snprintf(
      out, out_len,
      "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%"
      "02x%02x",
      b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11],
      b[12], b[13], b[14], b[15]);
  if (ret < 0 || (size_t)ret >= out_len) {
    MRC_LOG_ERROR("Failed to format GID string");
    return -1;
  }
  return 0;
}

uint32_t gen_psn(void) { return (uint32_t)(lrand48() & 0xFFFFFF); }

double ts_diff_sec(const struct timespec *s, const struct timespec *e) {
  return (e->tv_sec - s->tv_sec) + (e->tv_nsec - s->tv_nsec) / 1e9;
}

enum ibv_mtu mtu_to_enum(int mtu) {
  switch (mtu) {
  case 256:
    return IBV_MTU_256;
  case 512:
    return IBV_MTU_512;
  case 1024:
    return IBV_MTU_1024;
  case 2048:
    return IBV_MTU_2048;
  case 4096:
    return IBV_MTU_4096;
  default:
    return 0;
  }
}

struct bench_stats compute_stats(double elapsed_sec, long long iters, int batch,
                                 size_t msg_size) {
  struct bench_stats st;
  st.elapsed_sec = elapsed_sec;
  st.total_bytes = (double)iters * batch * msg_size;
  st.bandwidth_gbps = (st.total_bytes * 8.0) / (elapsed_sec * 1e9);
  st.msg_rate = (iters * (double)batch) / elapsed_sec;
  return st;
}

void print_results(size_t msg_size, int iters, const struct bench_stats *st) {
  double msg_rate_mpps = st->msg_rate / 1e6; // Convert to Mpps

  printf(" %-10zu %-14d %-18.2f %.6f\n", msg_size, iters,
         st->bandwidth_gbps, // BW average
         msg_rate_mpps);     // MsgRate in Mpps
  fflush(stdout);
}

int do_exchange(const int role, int port, const char *server_ip,
                struct mrc_resources *res, struct wire *me, struct wire *peer) {
  if (role == 0) {
    MRC_LOG_INFO("Server listening on %d", port);
    res->tcp_listen_sock = tcp_listen_(port);
    if (res->tcp_listen_sock < 0)
      return -1;
    res->tcp_sock = tcp_accept_(res->tcp_listen_sock);
    if (res->tcp_sock < 0)
      return -1;
    if (xfer(res->tcp_sock, peer, sizeof(*peer), false) ||
        xfer(res->tcp_sock, me, sizeof(*me), true)) {
      MRC_LOG_ERROR("xfer failed");
      return -1;
    }
  } else {
    if (!server_ip) {
      MRC_LOG_ERROR("client needs server_ip");
      return -1;
    }
    MRC_LOG_INFO("Client connecting %s:%d", server_ip, port);
    res->tcp_sock = tcp_connect_(server_ip, port);
    if (res->tcp_sock < 0)
      return -1;
    if (xfer(res->tcp_sock, me, sizeof(*me), true) ||
        xfer(res->tcp_sock, peer, sizeof(*peer), false)) {
      MRC_LOG_ERROR("xfer failed");
      return -1;
    }
  }

  char gid_s[40], pgid_s[40];
  if (gid_to_str(&me->gid, gid_s, sizeof(gid_s)) ||
      gid_to_str(&peer->gid, pgid_s, sizeof(pgid_s))) {
    MRC_LOG_ERROR("gid_to_str failed");
    return -1;
  }
  MRC_LOG_INFO("Conn info:");
  MRC_LOG_INFO(
      "  Local  -> qpn:0x%06x psn:0x%06x rkey:0x%08x addr:0x%016llx gid:%s",
      me->qpn, me->sg_psn, me->rkey, (unsigned long long)me->vaddr, gid_s);
  MRC_LOG_INFO(
      "  Remote -> qpn:0x%06x psn:0x%06x rkey:0x%08x addr:0x%016llx gid:%s",
      peer->qpn, peer->sg_psn, peer->rkey, (unsigned long long)peer->vaddr,
      pgid_s);

  if (res->tcp_listen_sock == 0) {
    close(res->tcp_listen_sock);
    res->tcp_listen_sock = -1;
  }
  // Keep tcp_sock open for synchronization
  MRC_LOG_DEBUG("Control exchange done, keeping TCP socket open for sync");
  return 0;
}

int sock_sync(int sock, bool is_client) {
  char dummy = 0;
  if (is_client) {
    // Client: wait for server ready, then ack
    MRC_LOG_DEBUG("Client: waiting for server ready signal...");
    if (xfer(sock, &dummy, 1, false)) {
      MRC_LOG_ERROR("Client: failed to receive server ready");
      return -1;
    }
    MRC_LOG_DEBUG("Client: received server ready, sending ack...");
    if (xfer(sock, &dummy, 1, true)) {
      MRC_LOG_ERROR("Client: failed to send ack");
      return -1;
    }
    MRC_LOG_DEBUG("Client: sync complete");
  } else {
    // Server: signal ready, then wait for client ack
    MRC_LOG_DEBUG("Server: sending ready signal...");
    if (xfer(sock, &dummy, 1, true)) {
      MRC_LOG_ERROR("Server: failed to send ready signal");
      return -1;
    }
    MRC_LOG_DEBUG("Server: waiting for client ack...");
    if (xfer(sock, &dummy, 1, false)) {
      MRC_LOG_ERROR("Server: failed to receive client ack");
      return -1;
    }
    MRC_LOG_DEBUG("Server: sync complete");
  }
  return 0;
}

int init_wire_info(struct wire *me, struct mrc_qp *mrc_qp, struct ibv_mr *mr,
                   void *buffer, struct ibv_context *ibv_context, int ib_port,
                   uint8_t sgid_index) {
  uint32_t qpn = 0;
  if (mrc_get_qpn(mrc_qp, &qpn)) {
    MRC_LOG_ERROR("get_qpn failed");
    return -1;
  }

  union ibv_gid gid;
  if (ibv_query_gid(ibv_context, ib_port, sgid_index, &gid)) {
    MRC_LOG_ERROR("query_gid failed (port=%d index=%d)", ib_port, sgid_index);
    return -1;
  }

  me->qpn = qpn;
  me->rkey = mr->rkey;
  me->vaddr = (uintptr_t)buffer;
  me->gid = gid;
  me->sgid_index = sgid_index;
  me->sg_psn = gen_psn();

  MRC_LOG_DEBUG("Wire: qpn=0x%06x psn=0x%06x rkey=0x%08x", me->qpn, me->sg_psn,
                me->rkey);
  return 0;
}

void cleanup_resources(struct mrc_resources *res) {
  if (res->mrc_qp)
    mrc_destroy_qp(res->mrc_qp);
  if (res->mrc_cq)
    mrc_destroy_cq(res->mrc_cq);
  if (res->mrc_recv_cq)
    mrc_destroy_cq(res->mrc_recv_cq);
  if (res->mr)
    ibv_dereg_mr(res->mr);
  if (res->ibv_pd)
    ibv_dealloc_pd(res->ibv_pd);
  if (res->mctx)
    mrc_destroy_context(res->mctx);
  if (res->ibv_context)
    ibv_close_device(res->ibv_context);
  if (res->device_list)
    ibv_free_device_list(res->device_list);
  if (res->buffer)
    free(res->buffer);
  if (res->tcp_sock > 0)
    close(res->tcp_sock);
  if (res->tcp_listen_sock > 0)
    close(res->tcp_listen_sock);
}
