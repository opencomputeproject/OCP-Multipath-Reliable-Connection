/*
 * utils.h - Utility functions for MRC example applications
 *
 * Copyright (c) 2025 Microsoft Corporation
 * Licensed under MIT License
 */

#ifndef UTILS_H
#define UTILS_H

#include "mrc.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct wire {
  uint32_t qpn;
  uint32_t rkey;
  uint64_t vaddr;
  union ibv_gid gid;
  uint8_t sgid_index;
  uint32_t sg_psn;
};

struct mrc_resources {
  struct mrc_qp *mrc_qp;
  struct mrc_cq *mrc_cq;
  struct mrc_cq *mrc_recv_cq;
  struct ibv_mr *mr;
  struct ibv_pd *ibv_pd;
  struct mrc_context *mctx;
  struct ibv_context *ibv_context;
  struct ibv_device **device_list;
  void *buffer;
  int tcp_sock;
  int tcp_listen_sock;
};

struct bench_stats {
  double elapsed_sec;
  double total_bytes;
  double bandwidth_gbps;
  double msg_rate;
};

// ---------- Logging System ----------
enum mrc_log_level {
  MRC_LOG_LEVEL_ERROR = 0,
  MRC_LOG_LEVEL_WARN = 1,
  MRC_LOG_LEVEL_INFO = 2,
  MRC_LOG_LEVEL_DEBUG = 3,
};

extern enum mrc_log_level g_mrc_log_level;

void mrc_log_init(void);

void mrc_log_set_level(enum mrc_log_level level);

// Logging Macros
#define MRC_LOG_ERROR(fmt, ...)                                                \
  do {                                                                         \
    if (g_mrc_log_level >= MRC_LOG_LEVEL_ERROR) {                              \
      fprintf(stderr, "[MRC ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__,      \
              ##__VA_ARGS__);                                                  \
    }                                                                          \
  } while (0)

#define MRC_LOG_WARN(fmt, ...)                                                 \
  do {                                                                         \
    if (g_mrc_log_level >= MRC_LOG_LEVEL_WARN) {                               \
      fprintf(stderr, "[MRC WARN] " fmt "\n", ##__VA_ARGS__);                  \
    }                                                                          \
  } while (0)

#define MRC_LOG_INFO(fmt, ...)                                                 \
  do {                                                                         \
    if (g_mrc_log_level >= MRC_LOG_LEVEL_INFO) {                               \
      fprintf(stdout, "[MRC INFO] " fmt "\n", ##__VA_ARGS__);                  \
    }                                                                          \
  } while (0)

#define MRC_LOG_DEBUG(fmt, ...)                                                \
  do {                                                                         \
    if (g_mrc_log_level >= MRC_LOG_LEVEL_DEBUG) {                              \
      fprintf(stdout, "[MRC DEBUG] " fmt "\n", ##__VA_ARGS__);                 \
    }                                                                          \
  } while (0)

int tcp_listen_(int port);
int tcp_accept_(int ls);
int tcp_connect_(const char *ip, int port);
int xfer(int fd, void *buf, size_t len, bool sendd);
uint32_t gen_psn(void);
int gid_to_str(const union ibv_gid *g, char *out, size_t out_len);
void print_stats(const struct bench_stats *st);
int do_exchange(const int role, int port, const char *server_ip,
                struct mrc_resources *res, struct wire *me, struct wire *peer);
int sock_sync(int sock, bool is_client);
int init_wire_info(struct wire *me, struct mrc_qp *mrc_qp, struct ibv_mr *mr,
                   void *buffer, struct ibv_context *ibv_context, int ib_port,
                   uint8_t sgid_index);
struct bench_stats compute_stats(double elapsed_sec, long long iters, int batch,
                                 size_t msg_size);
void print_results(size_t msg_size, int iters, const struct bench_stats *st);
double ts_diff_sec(const struct timespec *s, const struct timespec *e);
enum ibv_mtu mtu_to_enum(int mtu);
void cleanup_resources(struct mrc_resources *res);
#endif // UTILS_H