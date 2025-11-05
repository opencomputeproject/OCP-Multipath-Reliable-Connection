/*
 * mrc_example.c - RDMA write performance benchmark using LIB MRC API
 *
 *  RDMA write performance benchmark using MRC API
 * Measures bandwidth and message rate across varying message sizes.
 *
 * Copyright (c) 2025 Microsoft Corporation
 * Licensed under MIT License
 *
 * EXAMPLE USAGE:
 *
 * On server node:
 *   ./mrc_example -d mlx5_0 -g 3 -m 4096 -n 10000 -s 67108864
 *
 * On client node:
 *   ./mrc_example <server_ip> -d mlx5_0 -g 3 -m 4096 -n 10000 -s 67108864
 *
 */

#include "mrc.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <infiniband/verbs.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// ---------- Tunables & defaults ----------
#define RTS_TIMEOUT 7
#define RTS_RETRY 14
#define RTS_RNR_RETRY 14
#define CQ_ENTRY 256

#define DEF_GID_INDEX 3
#define DEF_PORT 18185
#define DEF_PORT_NUM 1
#define MAX_MSG_SIZE (1024 * 1024 * 1024)
#define DEF_ITERS 1000
#define DEF_SWEEP_MIN_MSG_SIZE 1
#define DEF_SWEEP_MAX_MSG_SIZE (64 * 1024 * 1024)
#define DEF_SWEEP_STEP_FACTOR 2
#define DEF_MSG_SIZE DEF_SWEEP_MAX_MSG_SIZE

#define DEF_WARMUP_ITERS 100
#define DEF_POLL_SIZE 16
#define DEF_TX_DEPTH 128
#define DEF_RX_DEPTH 1
#define DEF_CQ_MOD 64

#if DEF_SWEEP_MAX_MSG_SIZE > MAX_MSG_SIZE
#error "DEF_SWEEP_MAX_MSG_SIZE cannot exceed MAX_MSG_SIZE"
#endif

#if DEF_CQ_MOD >= DEF_TX_DEPTH
#error "CQ_MOD must be less than TX_DEPTH"
#endif

#define ALLOCATE(var, type, size)                                              \
  {                                                                            \
    if ((var = (type *)malloc(sizeof(type) * (size))) == NULL) {               \
      fprintf(stderr, "malloc Failed\n");                                      \
      exit(1);                                                                 \
    }                                                                          \
  }

// ---------- QP transitions ----------
static inline int change_mrc_qp_to_init(struct mrc_qp *mrc_qp) {
  struct ibv_qp_attr ibv_attr = (struct ibv_qp_attr){0};
  struct mrc_qp_attr mrc_device_attr = (struct mrc_qp_attr){0};
  int ibv_mask = 0, mrc_mask = 0;

  ibv_attr.qp_state = IBV_QPS_INIT;
  ibv_attr.pkey_index = 0;
  ibv_attr.port_num = DEF_PORT_NUM;
  ibv_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE;

  ibv_mask =
      IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;

  int ret =
      mrc_modify_qp(mrc_qp, &ibv_attr, ibv_mask, &mrc_device_attr, mrc_mask);
  if (ret) {
    MRC_LOG_ERROR("INIT failed: %d", ret);
    return -1;
  }
  MRC_LOG_DEBUG("QP -> INIT");
  return 0;
}

static inline int change_mrc_qp_to_rtr(struct mrc_qp *mrc_qp, int port_num,
                                       struct wire *peer, struct wire *me,
                                       enum ibv_mtu mtu) {
  struct ibv_qp_attr v = (struct ibv_qp_attr){0};
  struct mrc_qp_attr m = (struct mrc_qp_attr){0};
  int vmask = IBV_QP_STATE | IBV_QP_AV | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
              IBV_QP_PATH_MTU;

  v.qp_state = IBV_QPS_RTR;
  v.path_mtu = mtu;

  v.ah_attr.is_global = 1;
  v.ah_attr.grh.dgid = peer->gid;
  v.ah_attr.grh.sgid_index = me->sgid_index;
  v.ah_attr.port_num = port_num;

  v.dest_qp_num = peer->qpn;
  v.rq_psn = peer->sg_psn;

  int rc = mrc_modify_qp(mrc_qp, &v, vmask, &m, 0);
  if (rc) {
    MRC_LOG_ERROR("RTR failed: %d", rc);
    return -1;
  }
  MRC_LOG_DEBUG("QP -> RTR");
  return 0;
}

static inline int change_mrc_qp_to_rts(struct mrc_qp *mrc_qp, struct wire *me) {
  struct ibv_qp_attr v = (struct ibv_qp_attr){0};
  struct mrc_qp_attr m = (struct mrc_qp_attr){0};
  int vmask = IBV_QP_STATE | IBV_QP_SQ_PSN | IBV_QP_RETRY_CNT |
              IBV_QP_RNR_RETRY | IBV_QP_TIMEOUT;

  v.qp_state = IBV_QPS_RTS;
  v.sq_psn = me->sg_psn;
  v.timeout = RTS_TIMEOUT;
  v.retry_cnt = RTS_RETRY;
  v.rnr_retry = RTS_RNR_RETRY;

  int rc = mrc_modify_qp(mrc_qp, &v, vmask, &m, 0);
  if (rc) {
    MRC_LOG_ERROR("RTS failed: %d", rc);
    return -1;
  }
  MRC_LOG_DEBUG("QP -> RTS");
  return 0;
}

// ---------- QP creation ----------
static inline struct mrc_qp *create_mrc_qp(struct mrc_context *mctx,
                                           struct mrc_cq *send_cq,
                                           struct mrc_cq *recv_cq,
                                           struct ibv_pd *ibv_pd) {
  struct mrc_qp_init_attr a = (struct mrc_qp_init_attr){0};
  a.send_cq = send_cq;
  a.recv_cq = recv_cq;
  a.pd = ibv_pd;

  a.cap.max_send_wr = DEF_TX_DEPTH;
  a.cap.max_send_sge = 1;
  a.cap.max_recv_wr = DEF_RX_DEPTH;
  a.cap.max_recv_sge = 1;

  struct mrc_qp *qp = mrc_create_qp(mctx, &a);
  if (!qp) {
    MRC_LOG_ERROR("create_qp failed");
    return NULL;
  }

  MRC_LOG_INFO(
      "QP created: (req) send_wr=%d recv_wr=%d (act) send_wr=%d recv_wr=%d",
      a.cap.max_send_wr, a.cap.max_recv_wr, a.cap.max_send_wr,
      a.cap.max_recv_wr);
  return qp;
}

// ---------- Memory ----------
static struct ibv_mr *register_memory_region(struct ibv_pd *pd, void *buffer,
                                             size_t size) {
  return ibv_reg_mr(pd, buffer, size,
                    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE |
                        IBV_ACCESS_RELAXED_ORDERING);
}

// ---------- SEND helpers ----------
static int post_rdma_write(struct mrc_qp *qp, void *buf, uint32_t len,
                           uint32_t lkey, uint64_t remote_addr, uint32_t rkey,
                           int signaled, uint64_t wr_id) {
  struct ibv_sge sge = {0};
  struct ibv_send_wr wr = {0};

  sge.addr = (uintptr_t)buf;
  sge.length = len;
  sge.lkey = lkey;

  wr.wr_id = signaled ? wr_id : 0ULL;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  wr.opcode = IBV_WR_RDMA_WRITE;
  wr.send_flags = signaled ? IBV_SEND_SIGNALED : 0;
  wr.wr.rdma.remote_addr = remote_addr;
  wr.wr.rdma.rkey = rkey;
  wr.next = NULL;

  struct ibv_send_wr *bad = NULL;
  int rc = mrc_post_send(qp, &wr, &bad);
  if (rc) {
    MRC_LOG_ERROR("post WRITE rc=%d errno=%d (bad=%p)", rc, errno, (void *)bad);
  }
  return rc;
}

static int post_rdma_write_with_imm(struct mrc_qp *qp, void *buf, uint32_t len,
                                    uint32_t lkey, uint64_t remote_addr,
                                    uint32_t rkey, uint32_t imm_data,
                                    uint64_t wr_id) {
  struct ibv_sge sge = {0};
  struct ibv_send_wr wr = {0};

  sge.addr = (uintptr_t)buf;
  sge.length = len;
  sge.lkey = lkey;

  wr.wr_id = wr_id;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.imm_data = htonl(imm_data);
  wr.wr.rdma.remote_addr = remote_addr;
  wr.wr.rdma.rkey = rkey;
  wr.next = NULL;

  struct ibv_send_wr *bad = NULL;
  int rc = mrc_post_send(qp, &wr, &bad);
  if (rc) {
    MRC_LOG_ERROR("post WRITE_WITH_IMM rc=%d errno=%d (bad=%p)", rc, errno,
                  (void *)bad);
  }
  return rc;
}

static int post_recv(struct mrc_qp *qp, void *buf, uint32_t len, uint32_t lkey,
                     uint64_t wr_id) {
  struct ibv_sge sge = {.addr = (uintptr_t)buf, .length = len, .lkey = lkey};
  struct ibv_recv_wr wr = {0};
  wr.wr_id = wr_id;
  if (len > 0) {
    wr.sg_list = &sge;
    wr.num_sge = 1;
  } else {
    wr.sg_list = NULL;
    wr.num_sge = 0;
  }

  struct ibv_recv_wr *bad = NULL;
  int rc = mrc_post_recv(qp, &wr, &bad);
  if (rc)
    MRC_LOG_ERROR("post RECV rc=%d errno=%d", rc, errno);
  return rc;
}

// ---------- warmup ----------
// Warmup for sender: post and complete a few operations to establish connection
static int warmup_sender(struct mrc_resources *res, size_t msg_size,
                         uint64_t peer_recv_addr, uint32_t peer_rkey,
                         int warmup) {
  MRC_LOG_DEBUG("Warmup sender...");
  struct ibv_wc wc;
  int n;
  char *send_buf = (char *)res->buffer;

  for (int i = 0; i < warmup; i++) {
    // One signaled operation per warmup iteration
    if (post_rdma_write(res->mrc_qp, send_buf, (uint32_t)msg_size,
                        res->mr->lkey, peer_recv_addr, peer_rkey, 1,
                        (uint64_t)i))
      return -1;

    // Wait for one CQE
    do {
      n = mrc_poll_cq(res->mrc_cq, 1, &wc);
    } while (n == 0);

    if (n < 0 || wc.status != IBV_WC_SUCCESS) {
      MRC_LOG_ERROR("Warmup send wc failed: %s", ibv_wc_status_str(wc.status));
      return -1;
    }
  }
  MRC_LOG_DEBUG("Warmup sender done");
  return 0;
}

// No warmup needed for receiver
static int warmup_receiver(struct mrc_resources *res, int warmup) {
  (void)res;
  MRC_LOG_DEBUG("Warmup receiver - allowing %d warmup ops to complete", warmup);
  return 0;
}

// ---------- benchmark loops ----------
static int run_benchmark_sender(struct mrc_resources *res, size_t msg_size,
                                int iters, uint64_t peer_recv_addr,
                                uint32_t peer_rkey, struct bench_stats *out) {
  struct timespec ts0, ts1;
  struct ibv_wc *wc;
  int n, i;
  char *send_buf = (char *)res->buffer;
  uint64_t totscnt = 0;
  uint64_t totccnt = 0;
  uint64_t tot_iters = (uint64_t)iters;
  int tx_depth = DEF_TX_DEPTH;
  int cq_mod = DEF_CQ_MOD;

  // Runtime safety checks
  if (cq_mod <= 0 || cq_mod >= tx_depth) {
    MRC_LOG_ERROR("CQ_MOD(%d) must be in (0, TX_DEPTH(%d))", cq_mod, tx_depth);
    return -1;
  }

  ALLOCATE(wc, struct ibv_wc, DEF_POLL_SIZE);

  clock_gettime(CLOCK_MONOTONIC, &ts0);

  // Main loop: keep posting until all sent and all completed
  while (totscnt < tot_iters || totccnt < tot_iters) {
    while (totscnt < tot_iters && totscnt < (uint64_t)tx_depth + totccnt) {
      int should_signal =
          (((uint64_t)totscnt % (uint64_t)cq_mod) == (uint64_t)(cq_mod - 1)) ||
          (totscnt == tot_iters - 1);

      // Submit one operation
      if (post_rdma_write(res->mrc_qp, send_buf, (uint32_t)msg_size,
                          res->mr->lkey, peer_recv_addr, peer_rkey,
                          should_signal, totscnt)) {
        free(wc);
        return -1;
      }

      totscnt++; // posted operations +1
    }

    // Poll for completions if outstanding operations exist
    if (totccnt < tot_iters) {
      n = mrc_poll_cq(res->mrc_cq, DEF_POLL_SIZE, wc);
      if (n > 0) {
        for (i = 0; i < n; i++) {
          if (wc[i].status != IBV_WC_SUCCESS) {
            MRC_LOG_ERROR("Send wc failed: %s",
                          ibv_wc_status_str(wc[i].status));
            free(wc);
            return -1;
          }
          // Each signaled completion covers cq_mod operations
          uint64_t fill = (uint64_t)cq_mod;
          uint64_t remain = tot_iters - totccnt;
          if (fill > remain)
            fill = remain;
          totccnt += fill;
        }
      } else if (n < 0) {
        MRC_LOG_ERROR("Poll CQ failed");
        free(wc);
        return -1;
      }
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &ts1);

  // Send one-byte RDMA write with immediate as signal after benchmark poll done
  MRC_LOG_DEBUG("Sending one-byte write-with-immediate signal");
  // Write to peer's base address
  uint64_t signal_addr =
      peer_recv_addr - msg_size; // Write to start of peer's buffer
  if (post_rdma_write_with_imm(res->mrc_qp, res->signal_buffer, 1,
                               res->signal_mr->lkey, signal_addr, peer_rkey,
                               0x004D5243, 0xFFFFFFFFULL)) {
    free(wc);
    return -1;
  }

  // Poll for the signal completion
  struct ibv_wc signal_wc;
  do {
    n = mrc_poll_cq(res->mrc_cq, 1, &signal_wc);
  } while (n == 0);

  if (n < 0 || signal_wc.status != IBV_WC_SUCCESS) {
    MRC_LOG_ERROR("Signal write completion failed: %s",
                  ibv_wc_status_str(signal_wc.status));
    free(wc);
    return -1;
  }
  MRC_LOG_DEBUG("Signal write-with-immediate completed successfully");

  free(wc);
  double elapsed = ts_diff_sec(&ts0, &ts1);
  *out = compute_stats(elapsed, iters, /*batch=*/1, msg_size);

  return 0;
}

static int run_benchmark_receiver(struct mrc_resources *res, int iters,
                                  size_t msg_size, struct bench_stats *out) {
  (void)iters;
  (void)msg_size;
  (void)out;

  // Post one receive for the write-with-immediate signal
  MRC_LOG_DEBUG("Receiver: posting RECV for write-with-immediate signal");
  char *dummy_recv_buf = (char *)res->buffer + 2 * msg_size;
  if (post_recv(res->mrc_qp, dummy_recv_buf, 0, res->mr->lkey, 0x1000)) {
    MRC_LOG_ERROR("Receiver: failed to post RECV");
    return -1;
  }

  // Wait for the write-with-immediate completion
  MRC_LOG_DEBUG("Receiver: waiting for write-with-immediate signal");
  struct ibv_wc wc;
  int n;
  do {
    n = mrc_poll_cq(res->mrc_recv_cq, 1, &wc);
  } while (n == 0);

  if (n < 0 || wc.status != IBV_WC_SUCCESS) {
    MRC_LOG_ERROR("Receiver: signal recv completion failed: %s",
                  ibv_wc_status_str(wc.status));
    return -1;
  }

  if (wc.opcode != IBV_WC_RECV_RDMA_WITH_IMM) {
    MRC_LOG_ERROR("Receiver: unexpected opcode: %d", wc.opcode);
    return -1;
  }

  uint32_t imm_data = ntohl(wc.imm_data);
  MRC_LOG_DEBUG(
      "Receiver: received write-with-immediate signal, imm_data=0x%08x",
      imm_data);

  return 0;
}

// ---------- simple validation (receiver) ----------
static void validate_received_data(char *recv_buf, size_t msg_size) {
  __sync_synchronize();
  MRC_LOG_DEBUG("Validating received data...");
  int errors = 0;
  size_t check = msg_size < 256 ? msg_size : 256;
  for (size_t i = 0; i < check; i++) {
    if ((unsigned char)recv_buf[i] != (unsigned char)(i & 0xFF)) {
      errors++;
      if (errors == 1) {
        MRC_LOG_ERROR("Data mismatch @%zu: expect 0x%02x got 0x%02x", i,
                      (unsigned char)(i & 0xFF), (unsigned char)recv_buf[i]);
      }
    }
  }
  if (errors == 0)
    MRC_LOG_DEBUG("Data validation passed: no errors in first %zu bytes",
                  check);
  else
    MRC_LOG_ERROR("Data validation found %d errors (first %zu bytes checked)",
                  errors, check);
}

// ---------- usage ----------
static void usage(const char *argv0) {
  printf("Usage:\n");
  printf("  %s            start a server and wait for connection\n", argv0);
  printf("  %s <host>     connect to server at <host>\n", argv0);
  printf("\n");
  printf("Options:\n");
  printf("  -p, --port=<port>      listen on/connect to port <port> (default "
         "%d)\n",
         DEF_PORT);
  printf("  -d, --ib-dev=<dev>     use IB device <dev> (default first device "
         "found)\n");
  printf("  -i, --ib-port=<port>   use port <port> of IB device (default %d)\n",
         DEF_PORT_NUM);
  printf("  -g, --gid-idx=<idx>    GID index (default %d)\n", DEF_GID_INDEX);
  printf("  -m, --mtu=<mtu>        path MTU (default 4096 bytes)\n");
  printf("  -n, --iters=<iters>    number of exchanges per message size "
         "(default %d)\n",
         DEF_ITERS);
  printf("  -s, --size=<size>      maximum message size in bytes (default "
         "%zu)\n",
         (size_t)DEF_MSG_SIZE);
  printf("      --min-size=<size>  minimum message size for sweep (default "
         "%d)\n",
         DEF_SWEEP_MIN_MSG_SIZE);
  printf("      --step-factor=<n>  message size step factor for sweep "
         "(default %d)\n",
         DEF_SWEEP_STEP_FACTOR);
  printf("  -w, --warmup-iters=<n> number of warmup iterations (default %d)\n",
         DEF_WARMUP_ITERS);
  printf("\n");
  printf("Note: Benchmark sweeps message sizes from min-size up to size "
         "(multiply by step-factor each iteration)\n");
  printf("\n");
  printf("Environment:\n");
  printf("  MRC_LOG_LEVEL = ERROR|WARN|INFO|DEBUG or 0-3 (default INFO)\n");
}

// ----------  main ----------
int main(int argc, char **argv) {
  const char *devname = NULL;
  const char *servername = NULL;
  unsigned int port = DEF_PORT;
  int iters = DEF_ITERS;
  size_t max_msg_size = DEF_MSG_SIZE;
  size_t min_msg_size = DEF_SWEEP_MIN_MSG_SIZE;
  int step_factor = DEF_SWEEP_STEP_FACTOR;
  int warmup_iters = DEF_WARMUP_ITERS;
  int ib_port = DEF_PORT_NUM;
  int gid_index = DEF_GID_INDEX;
  enum ibv_mtu mtu = IBV_MTU_4096;

  while (1) {
    int c;

    static struct option long_options[] = {
        {.name = "port", .has_arg = 1, .val = 'p'},
        {.name = "ib-dev", .has_arg = 1, .val = 'd'},
        {.name = "ib-port", .has_arg = 1, .val = 'i'},
        {.name = "gid-idx", .has_arg = 1, .val = 'g'},
        {.name = "mtu", .has_arg = 1, .val = 'm'},
        {.name = "iters", .has_arg = 1, .val = 'n'},
        {.name = "size", .has_arg = 1, .val = 's'},
        {.name = "min-size", .has_arg = 1, .val = 'S'},
        {.name = "step-factor", .has_arg = 1, .val = 'F'},
        {.name = "warmup-iters", .has_arg = 1, .val = 'w'},
        {.name = "help", .has_arg = 0, .val = 'h'},
        {}};

    c = getopt_long(argc, argv, "p:d:i:g:m:n:s:S:F:w:h", long_options, NULL);
    if (c == -1)
      break;

    switch (c) {
    case 'p':
      port = strtoul(optarg, NULL, 0);
      if (port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
      }
      break;

    case 'd':
      devname = strdup(optarg);
      break;

    case 'i':
      ib_port = strtol(optarg, NULL, 0);
      if (ib_port < 1 || ib_port > 255) {
        fprintf(stderr, "Invalid IB port number (must be 1-255)\n");
        return 1;
      }
      break;

    case 'g':
      gid_index = strtol(optarg, NULL, 0);
      if (gid_index < 0) {
        fprintf(stderr, "Invalid GID index (must be >= 0)\n");
        return 1;
      }
      break;

    case 's':
      max_msg_size = (size_t)strtoull(optarg, NULL, 0);
      break;

    case 'S':
      min_msg_size = (size_t)strtoull(optarg, NULL, 0);
      if (min_msg_size == 0) {
        fprintf(stderr, "Invalid min-size (must be > 0)\n");
        return 1;
      }
      break;

    case 'F':
      step_factor = strtol(optarg, NULL, 0);
      if (step_factor < 2) {
        fprintf(stderr, "Invalid step-factor (must be >= 2)\n");
        return 1;
      }
      break;

    case 'n':
      iters = strtoul(optarg, NULL, 0);
      break;

    case 'w':
      warmup_iters = strtol(optarg, NULL, 0);
      if (warmup_iters < 0) {
        fprintf(stderr, "Invalid warmup-iters (must be >= 0)\n");
        return 1;
      }
      break;

    case 'm':
      mtu = mtu_to_enum(strtol(optarg, NULL, 0));
      if (mtu == 0) {
        fprintf(stderr, "Invalid MTU value. Valid values: 256, 512, "
                        "1024, 2048, 4096\n");
        return 1;
      }
      break;

    case 'h':
    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (optind == argc - 1)
    servername = strdup(argv[optind]);
  else if (optind < argc) {
    usage(argv[0]);
    return 1;
  }

  // --- Validate parameters ---
  if (min_msg_size > max_msg_size) {
    fprintf(stderr, "Error: min-size (%zu) cannot be larger than size (%zu)\n",
            min_msg_size, max_msg_size);
    return 1;
  }

  // --- Logging ---
  mrc_log_init();

  const int role = servername ? 1 : 0; // client=1, server=0
  const char *server_ip = servername;

  if (max_msg_size == 0)
    max_msg_size = DEF_SWEEP_MAX_MSG_SIZE;
  if (max_msg_size < min_msg_size) {
    MRC_LOG_ERROR("msg_size must be >= %zu", min_msg_size);
    return 1;
  }
  if (max_msg_size > MAX_MSG_SIZE) {
    MRC_LOG_WARN("msg_size %zu exceeds MAX_MSG_SIZE %zu, clamping",
                 max_msg_size, (size_t)MAX_MSG_SIZE);
    max_msg_size = MAX_MSG_SIZE;
  }
  if (max_msg_size > DEF_SWEEP_MAX_MSG_SIZE) {
    MRC_LOG_WARN("msg_size %zu exceeds DEF_SWEEP_MAX_MSG_SIZE %zu, clamping",
                 max_msg_size, (size_t)DEF_SWEEP_MAX_MSG_SIZE);
    max_msg_size = DEF_SWEEP_MAX_MSG_SIZE;
  }
  if (iters <= 0) {
    MRC_LOG_ERROR("bad iters");
    return 1;
  }

  MRC_LOG_INFO("Params: iters=%d msg_size_sweep=[%zu, %zu] step=x%d", iters,
               min_msg_size, max_msg_size, step_factor);

  struct mrc_resources res = {0};
  res.tcp_sock = res.tcp_listen_sock = -1;

  srand48((long)time(NULL) ^ (long)getpid());

  // --- Open device ---
  int ndev = 0;
  res.device_list = ibv_get_device_list(&ndev);
  if (!res.device_list || ndev <= 0) {
    MRC_LOG_ERROR("no IB devs");
    return 1;
  }

  struct ibv_device *dev = NULL;
  for (int i = 0; i < ndev; i++)
    if (!strcmp(ibv_get_device_name(res.device_list[i]), devname)) {
      dev = res.device_list[i];
      break;
    }
  if (!dev) {
    MRC_LOG_ERROR("dev '%s' not found", devname);
    cleanup_resources(&res);
    return 1;
  }

  res.ibv_context = ibv_open_device(dev);
  if (!res.ibv_context) {
    MRC_LOG_ERROR("open_device failed");
    cleanup_resources(&res);
    return 1;
  }
  MRC_LOG_DEBUG("Opened device: %s", devname);

  // --- MRC caps ---
  struct mrc_device_attr mrc_device_attr = {0};
  int supported = 0;
  if (mrc_query_device(res.ibv_context, &mrc_device_attr, &supported) ||
      !supported) {
    MRC_LOG_ERROR("MRC not supported");
    cleanup_resources(&res);
    return 1;
  }

  // --- Context, PD, CQ, QP ---
  struct mrc_context_attr ctx_attr = {.mrc_api_version_used =
                                          MRC_PROTOCOL_VERSION_1};
  res.mctx = mrc_create_context(res.ibv_context, &ctx_attr);
  if (!res.mctx) {
    MRC_LOG_ERROR("create_context failed");
    cleanup_resources(&res);
    return 1;
  }

  res.ibv_pd = ibv_alloc_pd(res.ibv_context);
  if (!res.ibv_pd) {
    MRC_LOG_ERROR("alloc_pd failed");
    cleanup_resources(&res);
    return 1;
  }

  res.mrc_cq = mrc_create_cq(res.mctx, CQ_ENTRY, NULL, NULL, 0);
  if (!res.mrc_cq) {
    MRC_LOG_ERROR("create send_cq failed");
    cleanup_resources(&res);
    return 1;
  }

  res.mrc_recv_cq = mrc_create_cq(res.mctx, CQ_ENTRY, NULL, NULL, 0);
  if (!res.mrc_recv_cq) {
    MRC_LOG_ERROR("create recv_cq failed");
    cleanup_resources(&res);
    return 1;
  }

  res.mrc_qp = create_mrc_qp(res.mctx, res.mrc_cq, res.mrc_recv_cq, res.ibv_pd);
  if (!res.mrc_qp) {
    cleanup_resources(&res);
    return 1;
  }

  // --- Buffer & MR ---
  const size_t buf_size = 2 * max_msg_size;
  res.buffer = memalign(sysconf(_SC_PAGESIZE), buf_size);
  if (!res.buffer) {
    MRC_LOG_ERROR("memalign failed");
    cleanup_resources(&res);
    return 1;
  }
  memset(res.buffer, 0, buf_size);
  for (size_t i = 0; i < max_msg_size; i++)
    ((char *)res.buffer)[i] = (char)(i & 0xFF);

  res.mr = register_memory_region(res.ibv_pd, res.buffer, buf_size);
  if (!res.mr) {
    MRC_LOG_ERROR("reg_mr failed");
    cleanup_resources(&res);
    return 1;
  }
  MRC_LOG_INFO("MR: %zu bytes (send %zu, recv %zu, counter 4)", buf_size,
               max_msg_size, max_msg_size);

  // --- Signal Buffer & MR ---
  res.signal_buffer = memalign(sysconf(_SC_PAGESIZE), 1);
  if (!res.signal_buffer) {
    MRC_LOG_ERROR("signal_buffer memalign failed");
    cleanup_resources(&res);
    return 1;
  }
  memset(res.signal_buffer, 0, 1);

  res.signal_mr = register_memory_region(res.ibv_pd, res.signal_buffer, 1);
  if (!res.signal_mr) {
    MRC_LOG_ERROR("signal_mr registration failed");
    cleanup_resources(&res);
    return 1;
  }
  MRC_LOG_INFO("Signal MR: 1 byte registered");

  // --- INIT ---
  if (change_mrc_qp_to_init(res.mrc_qp)) {
    cleanup_resources(&res);
    return 1;
  }

  // --- Wire info ---
  struct wire me = {0}, peer = {0};
  if (init_wire_info(&me, res.mrc_qp, res.mr, res.buffer, res.ibv_context,
                     ib_port, gid_index)) {
    cleanup_resources(&res);
    return 1;
  }

  // --- Control exchange ---
  if (do_exchange(role, port, server_ip, &res, &me, &peer)) {
    cleanup_resources(&res);
    return 1;
  }

  // --- RTR/RTS ---
  if (change_mrc_qp_to_rtr(res.mrc_qp, ib_port, &peer, &me, mtu)) {
    cleanup_resources(&res);
    return 1;
  }
  if (change_mrc_qp_to_rts(res.mrc_qp, &me)) {
    cleanup_resources(&res);
    return 1;
  }
  MRC_LOG_INFO("QP ready (RTS)");

  // --- Single-direction pure RDMA WRITE benchmark ---
  MRC_LOG_INFO(
      "Start benchmark sweep: iters=%d msg_size range [%zu, %zu] (x%d steps)",
      iters, min_msg_size, max_msg_size, step_factor);
  if (role == 1) {
    printf("\n %-10s %-14s %-18s %s\n", "MsgSize", "TotalOps", "BW_avg(Gbps)",
           "MsgRate(Mpps)");
  }
  int rc = 0;
  struct bench_stats st = {0};
  size_t cur_msg_size = min_msg_size;

  while (true) {
    MRC_LOG_DEBUG("=== Starting iteration: msg_size=%zu ===", cur_msg_size);

    char *send_buf = (char *)res.buffer;
    memset(send_buf, 0, cur_msg_size);
    char *recv_buf = (char *)res.buffer + cur_msg_size;
    memset(recv_buf, 0, cur_msg_size);

    // Synchronize before starting
    int is_client = role == 1;

    MRC_LOG_DEBUG("SYNC POINT 1: sock_sync (is_client=%d, msg_size=%zu)...",
                  is_client, cur_msg_size);
    if (sock_sync(res.tcp_sock, is_client)) {
      MRC_LOG_ERROR("sock_sync failed");
      rc = -1;
      break;
    }
    MRC_LOG_DEBUG("SYNC POINT 1: completed");
    if (is_client) {
      uint64_t peer_recv_addr = peer.vaddr + cur_msg_size;
      if ((rc = warmup_sender(&res, cur_msg_size, peer_recv_addr, peer.rkey,
                              warmup_iters)))
        break;
      if ((rc = run_benchmark_sender(&res, cur_msg_size, iters, peer_recv_addr,
                                     peer.rkey, &st)))
        break;
    } else {
      if ((rc = warmup_receiver(&res, warmup_iters)))
        break;
      if ((rc = run_benchmark_receiver(&res, iters, cur_msg_size, &st)))
        break;
    }

    if (is_client) {
      print_results(cur_msg_size, iters, &st);
    } else {
      validate_received_data(recv_buf, cur_msg_size);
    }

    if (cur_msg_size >= max_msg_size)
      break;

    size_t next_size = cur_msg_size * step_factor;
    if (next_size <= cur_msg_size) {
      MRC_LOG_WARN("Next msg_size did not increase, stopping sweep");
      break;
    }

    if (next_size > max_msg_size)
      cur_msg_size = max_msg_size;
    else
      cur_msg_size = next_size;
  }

  cleanup_resources(&res);

  if (rc != 0)
    return 1;

  MRC_LOG_INFO("MRC example completed successfully");
  return 0;
}
