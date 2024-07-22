/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include <stdint.h>
#include <verbs.h>

enum mrc_version {
	MRC_VERSION_0 = 0, /* MRC not supported */
	MRC_VERSION_1 = (1 << 0),
};

struct mrc_attr {
	uint32_t version; /* see enum mrc_version */
	uint16_t max_wimm_dest;
};

/**
 * @brief Query Device
 *
 * Query the device to check MRC support.
 *
 * @param context[in] - IB Verbs context
 * @param attrs[out]  - MRC attributes
 *
 * @return
 * Returns 0 on success. Error codes as per ibv_query_device().
 */
int mrc_query_device(struct ibv_context *context,
		     struct mrc_attr *attr);

struct mrc_context {
	struct ibv_context *context;
	// TBD...
};

/**
 * @brief Initialize the MRC lib
 *
 * Initialize the MRC library. struct mrc_context provides a container for all
 * objects created under the MRC library. The primary function of this is to
 * track the objects and clean up when required.
 *
 * Additionally, it provides the implementation an opportunity to allocate
 * any system resources.
 *
 * This context supports layered applications where different libraries inside
 * one application can create / manage their own MRC objects, removing the
 * requirement of global variables in MRC lib.
 *
 * @param context[in]  - IB Verbs context
 * @param mrc_ctx[out] - MRC context, if successful
 *
 * @return
 * Returns 0 on success, and -1 on failure. Error code in errno.
 */
int mrc_context_init(struct ibv_context *context,
		     struct mrc_context **mrc_ctx);

/**
 * @brief Destroy the MRC lib context
 *
 * Destroy the MRC lib context.
 *
 * @param[in] mrc_ctx - MRC context
 *
 * @return
 * Returns 0 on success, and -1 on failure. Error code in errno.
 */
int mrc_context_destroy(struct mrc_context *mrc_ctx);

struct mrc_qp_init_attr {
	void               *qp_context;
	struct mrc_cq      *send_cq;
	struct mrc_cq      *recv_cq;
	struct ibv_qp_cap   cap;
	int                 sq_sig_all;
	struct ibv_pd      *pd;
	/* see enum ibv_qp_create_send_ops_flags */
	uint64_t            send_ops_flags;
};

struct mrc_qp {
	struct ibv_qp *qp;
	// TBD...
};

/**
 * @brief Destroy a QP
 *
 * Destroy a QP
 *
 * @param qp[in] - MRC QP
 *
 * @return
 * Returns 0 on success. Errors like ibv_destroy_qp()
 */
int mrc_destroy_qp(struct mrc_qp *qp);

/**
 * @brief Create an MRC QP
 *
 * Create an MRC QP.
 *
 * @param context[in]     - IB Verbs context
 * @param qp_init_attr_ex - QP init attributes
 *
 * @return
 * Returns 0 on success. Errors like ibv_create_qp().
 */
int mrc_create_qp(struct mrc_context *mrc_ctx,
		  struct mrc_qp_init_attr *mrc_qp_attr,
		  struct mrc_qp **qp);

#define MRC_MAX_VENDOR_CFG_SIZE 128

enum mrc_qp_attr_mask {
	// maximum inflight WriteIMM operations as Requester
	MRC_QP_ATTR_MAX_WIMM      = (1<<0),
	// maximum inflight WriteIMM operations as Responder
	MRC_QP_ATTR_MAX_WIMM_DEST = (1<<1),
	// maximum retry count in exponential range
	MRC_QP_ATTR_RETRY_CNT_EXP = (1<<2),
	// vendor specific configuration data
	MRC_QP_ATTR_VENDOR_CFG    = (1<<31)
};

/**
 * @brief Destroy a CQ
 *
 * Destroy a CQ
 *
 * @param cq[in] - MRC CQ
 *
 * @return
 * Returns 0 on success. Errors like ibv_destroy_cq()
 */
int mrc_destroy_cq(struct mrc_cq *cq);

struct mrc_qp_attr {
	uint16_t max_wimm;
	uint16_t max_wimm_dest;
	uint8_t  retry_cnt_exp;
	uint8_t  vendor_cfg[MRC_MAX_VENDOR_CFG_SIZE];
};

/**
 * @brief Query a QP attributes
 *
 * Queries a QP.
 *
 * @param qp[in]            - MRC QP
 * @param vattr[out]        - Libibverbs attributes returned
 * @param vattr_mask[in]    - Libibverbs attributes requested
 * @param mrc_attr[out]     - MRC attributes returned
 * @param mrc_attr_mask[in] - MRC attributes requested
 * @param init_attr[in]     - Additional MRC attributes returned
 *
 * @return
 * Returns 0 on success. Errors like ibv_query_qp()
 */
int mrc_query_qp(struct mrc_qp *qp,
		 struct ibv_qp_attr *vattr,
		 int vattr_mask,
		 struct mrc_qp_attr *mrc_attr,
		 enum mrc_qp_attr_mask mrc_attr_mask,
		 struct mrc_qp_init_attr *init_attr);

/**
 * @brief Modify a QP
 *
 * Modify a QP.
 *
 * @param qp[in]            - MRC QP
 * @param vattr[in]         - Libibverbs attributes to modify
 * @param vattr_mask[in]    - Libibverbs attribute mask
 * @param mrc_attr[in]      - MRC QP attributes to modify
 * @param mrc_attr_mask[in] - MRC QP attributes to modify
 *
 * @return
 * Returns 0 on success. Errors like ibv_modify_qp().
 */
int mrc_modify_qp(struct mrc_qp *qp,
		  struct ibv_qp_attr *vattr,
		  int vattr_mask,
		  struct mrc_qp_attr *mrc_attr,
		  enum mrc_qp_attr_mask mrc_attr_mask);

// Need mrc_destroy_qp()

struct mrc_cq {
	struct ibv_cq *cq;
	// TBD...
};

/**
 * @brief Create a CQ
 *
 * Create a CQ
 *
 * @param mrc_ctx[in]    - MRC context to use
 * @param cqe[in]        - Minimum number of entries required for CQ
 * @param cq_context[in] - application context
 * @param cq[out]        - Created CQ
 *
 * @return
 * Returns 0 on success. Errors like ibv_create_cq()
 */
int mrc_create_cq(struct mrc_context *mrc_ctx,
		  int cqe,
		  void *cq_context,
		  struct mrc_cq **cq);

// Need mrc_destroy_cq()

/**
 * @brief Post a receive operation on a QP
 *
 * Posts a receive operation for RDMA w/ IMM operations on the specified
 * QP. The semantics are like ibv_post_recv()
 *
 * @param qp[in]      - MRC QP to post receive on
 * @param wr[in]      - Work request
 * @param bad_wr[out] - Error receive request
 *
 * @return
 * Returns 0 on success, and -1 on failure. Error semantics like
 * ibv_post_recv().
 */
int mrc_post_recv(struct mrc_qp *qp,
		  struct ibv_recv_wr *wr,
		  struct ibv_recv_wr **bad_wr);

/**
 * @brief Poll for a Completion
 *
 * Polls for a completion entry, like ibv_poll_cq()
 *
 * @param cq[in]          - Completion queue
 * @param num_entries[in] - Number of completion entries
 * @param wc[out]         - Obtained completion entries
 *
 * @return
 * Like ibv_poll_cq().
 */
int mrc_poll_cq(struct mrc_cq *cq,
		int num_entries,
		struct ibv_wc *wc);

/**
 * @brief Post an MRC operation
 *
 * Posts an RDMA Write or RDMA Write with Immediate operation
 *
 * @param qp[in]      - MRC QP
 * @param wr[in]      - Work request to send
 * @param bad_wr[out] - Error WR
 *
 * @return
 * Returns 0 on success, and -1 on failure. Error semantics like
 * ibv_post_send().
 */
int mrc_post_send(struct mrc_qp *qp,
		  struct ibv_send_wr *wr,
		  struct ibv_send_wr **bad_wr);

// TBD... Next step is we need to incorporate the EV APIs

