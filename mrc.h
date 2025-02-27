/*
 * SPDX-FileCopyrightText: Copyright (c) 2024, 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 *
 * Copyright (c) 2024, 2025, Broadcom. All rights reserved. The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Copyright (c) 2024, 2025, Advanced Micro Devices (AMD), Inc.  All rights
 * reserved.
 */

#ifndef _MRC_API_H_
#define _MRC_API_H_

#include <stdint.h>
#include <stdbool.h>
#include <infiniband/verbs.h>

#include <mrc_api_ver.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MRC_API_CURRENT_VERSION	MRC_API_VER(1, 0, 0)

#define MRC_API_LAST_SUPPORTED_VERSION	MRC_API_VER(0, 0, 0)

#ifndef MRC_API_VER_USED
#define MRC_API_VER_USED MRC_API_CURRENT_VERSION
#elif MRC_API_VER_USED == MRC_API_VER_LATEST
#undef MRC_API_VER_USED
#define MRC_API_VER_USED MRC_API_CURRENT_VERSION
#endif

#if MRC_API_VER_USED < MRC_API_LAST_SUPPORTED_VERSION
#error "MRC_API_VER_USED is less than MRC_API_LAST_SUPPORTED version"
#elif MRC_API_VER_USED == MRC_API_LAST_SUPPORTED_VERSION
#warning "MRC_API_VER_USED is equal to MRC_API_LAST_SUPPORTED version, may become obsolete"
#endif

enum mrc_version {
	MRC_VERSION_0	= 0, /* MRC not supported */
	MRC_VERSION_1	= (1 << 0),
};

struct mrc_context;
struct mrc_qp;
struct mrc_cq;
struct mrc_comp_channel;

/**
 * Optional features supported by the implementation.
 */
enum mrc_attr_opt {
	/*
	 * The implementation supports dynamic MPR (requestor and/or
	 * responder role).
	 */
	MRC_OPT_CAP_DYNAMIC_MPR =	(1<<0),
};

struct mrc_attr {
	/* bitmap of all versions supported (see enum mrc_version) */
	uint32_t mrc_version;
	struct {
		/* Max configurable wimm value as requestor. */
		uint16_t max_wimm;
		/* Max configurable wimm value as responder. */
		uint16_t max_wimm_dest;
	} wimm_attr;
	struct {
		/*
		 * Maximum supported MPR value as requestor or responder.
		 * units = 128 PSNs
		 */
		uint8_t max_mpr;
		/* MPR resource allocation resolution; unit = 128 PSNs. */
		uint8_t mpr_res;
	} mpr_attr;
	/* bitmap of all optional features supported (mrc_attr_opt) */
	uint32_t opt_attr;
};

/**
 * @brief Query Device
 *
 * Query the device to check MRC support and other attributes.
 *
 * @param context[in] - IB Verbs context
 * @param attrs[out]  - MRC attributes
 *
 * @return
 * Returns 0 on success. Error codes as per ibv_query_device().
 */
int mrc_query_device(struct ibv_context *context,
		     struct mrc_attr *attr);

/* Context attributes declare the application's usage of MRC */
struct mrc_context_attr {
	/* API version used */
	uint32_t mrc_api_version_used;
};

/**
 * @brief Create a MRC lib context
 *
 * Create an MRC library context. `struct mrc_context` provides an instance
 * of the MRC library. It is the parent object for all other objects.
 * The primary function of this is to track the objects and clean up.
 * The context structure eliminates the need for any global objects
 * within the MRC library and supports multiple user libraries using MRC.
 * Additionally, it provides the implementation an opportunity to allocate
 * any system resources.
 *
 * The application provides the version of the API that is in use.
 * This allows the underlying implementation to adapt accordingly.
 *
 * An application can choose to provide NULL as context_attr allowing the
 * provider to choose the defaults.
 *
 * @param vcontext[in]  - IB Verbs context
 * @param context_attr[in] - MRC version used by the application
 *
 * @return
 * Returns a pointer to the allocated context on success or NULL if
 * the request fails.
 */
struct mrc_context *mrc_create_context(struct ibv_context *vcontext,
				       struct mrc_context_attr *context_attr);

/**
 * @brief Destroy the MRC lib context
 *
 * Destroy the MRC lib context.
 *
 * @param[in] mrc_ctx - MRC context
 *
 * @return
 * Returns 0 on success or -1 on failure.
 */
int mrc_destroy_context(struct mrc_context *mrc_ctx);

/**
 * @brief Create a completion channel
 *
 * Create a completion channel
 *
 * @param mrc_ctx[in] - MRC context
 * @param channel[out] - Created MRC channel
 *
 * @return
 * Returns 0 on success. Errors like ibv_create_comp_channel().
 */
int mrc_create_comp_channel(struct mrc_context *mrc_ctx,
			    struct mrc_comp_channel **channel);

/**
 * @brief Retrieve the completion channel's file descriptor
 *
 * @param channel[in] - MRC completion channel
 * @param fd[out]     - Returned file descriptor
 *
 * @return
 * Returns 0 on success or -1 on error.
 */
int mrc_get_comp_channel_fd(struct mrc_comp_channel *channel,
			    int *fd);

/**
 * @brief Destroy a completion channel
 *
 * Destroy a completion channel
 *
 * @param channel[in] - Completion channel
 *
 * @return
 * Returns 0 on success. Errors like ibv_destroy_comp_channel().
 */
int mrc_destroy_comp_channel(struct mrc_comp_channel *channel);

/**
 * @brief Create a CQ
 *
 * Create a CQ
 *
 * @param mrc_ctx[in]     - MRC context to use
 * @param cqe[in]         - Minimum number of entries required for CQ
 * @param cq_context[in]  - application context
 * @param channel[in]     - completion channel
 * @param comp_vector[in] - Completion vector to signal completion events
 *
 * @return
 * Returns a pointer to the allocated CQ on success or NULL if the request
 * fails. Errors like ibv_create_cq().
 */
struct mrc_cq *mrc_create_cq(struct mrc_context *mrc_ctx,
			     int cqe,
			     void *cq_context,
			     struct mrc_comp_channel *channel,
			     int comp_vector);

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
 * Returns the number of completions found on success or -1 on error.
 * If the return value is >=0 and less than num_entries, then the CQ
 * was emptied.
 */
int mrc_poll_cq(struct mrc_cq *cq,
		int num_entries,
		struct ibv_wc *wc);

/**
 * @brief Destroy a CQ
 *
 * Destroy a completion CQ
 *
 * @param cq[in] - MRC CQ
 *
 * @return
 * Returns 0 on success. Errors like ibv_destroy_cq()
 */
int mrc_destroy_cq(struct mrc_cq *cq);

struct mrc_qp_init_attr {
	void               *qp_context;
	struct mrc_cq      *send_cq;
	struct mrc_cq      *recv_cq;
	struct mrc_cq      *ev_cq;
	struct ibv_qp_cap   cap;
	int                 sq_sig_all;
	struct ibv_pd      *pd;
	/* see enum ibv_qp_create_send_ops_flags */
	uint64_t            send_ops_flags;
};

/**
 * @brief Create an MRC QP
 *
 * Create an MRC QP.
 *
 * @param context[in]     - IB Verbs context
 * @param qp_init_attr_ex - QP init attributes
 *
 * @return
 * Returns a pointer to the created QP on success or NULL if the request
 * fails. Errors like ibv_create_qp().
 */
struct mrc_qp *mrc_create_qp(struct mrc_context *mrc_ctx,
			     struct mrc_qp_init_attr *mrc_qp_attr);

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
 * @brief MRC QP attribute mask
 *
 * The list of attributes that may be changed upon transitioning QP
 * state from Reset->Init->RTR->RTS are:
 *
 * Next State  Required Attributes
 * ----------  -------------------
 * RTR         MRC_QP_MAX_WIMM_DEST
 *             MRC_QP_MPR_DEST
 *             MRC_QP_DYNAMIC_MPR_DEST
 *
 * RTS         MRC_QP_MAX_WIMM
 *             MRC_QP_MPR
 *             MRC_QP_RETRY_CNT
 *             MRC_QP_TIMEOUT
 */
enum mrc_qp_attr_mask {
	/* Max WIMM as requestor */
	MRC_QP_MAX_WIMM			= (1<<0),
	/* Max WIMM as responder */
	MRC_QP_MAX_WIMM_DEST		= (1<<1),
	/* Requestor MPR */
	MRC_QP_MPR			= (1<<2),
	/* Responder MPR */
	MRC_QP_MPR_DEST			= (1<<3),
	/* Responder dynamic MPR support */
	MRC_QP_DYNAMIC_MPR_DEST		= (1<<4),
// TODO: Uncomment after HW spec is updated (1.09)
//	/* QP (fixed+exponential) retry counter */
//	MRC_QP_RETRY_CNT		= (1<<5),
	/* QP ACK timeout */
	MRC_QP_TIMEOUT			= (1<<6),
	MRC_QP_VENDOR_CFG		= (1<<31)
};

#define MRC_MAX_VENDOR_CFG_SIZE 128

struct mrc_qp_attr {

	struct {
		/* Requestor MPR value; unit=128 PSNs */
		uint8_t mpr;
		/* Responder MPR value; unit=128 PSNs */
		uint8_t mpr_dest;
		/* if true, enable Responder dynamic MPR support */
		bool dynamic_mpr_dest;
	} mpr;

	struct {
		/* Max inflight WIMMs as requestor */
		uint16_t max_wimm;
		/* Max inflight WIMMs as responder */
		uint16_t max_wimm_dest;
	} wimm;

// TODO: Uncomment after HW spec is updated (1.09)
//	struct {
//		/* Fixed interval retry count; Max value = 8 */
//		uint8_t retry_cnt_fixed;
//		/* Exponential retry count; Max val = 32 (infinite retry) */
//		uint8_t retry_cnt_exp;
//	} retry_cnt;

	/* Local ACK timeout; 1.024 * 2^timeout us. Max val = 24 (17.17s) */
	uint8_t timeout;

	uint8_t vendor_cfg[MRC_MAX_VENDOR_CFG_SIZE];
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
 * Returns 0 on success and errors like ibv_query_qp()
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
 * Modify a QP. Caller provides ibv_qp_attr and mrc_qp_attr structures and
 * masks.
 *
 * The following IBV field masks are NOT supported:
 *     IBV_QP_PORT
 *     IBV_QP_TIMEOUT (use MRC_QP_TIMEOUT)
 *     IBV_QP_RETRY_CNT (use MRC_QP_RETRY_CNT)
 *     IBV_QP_RNR_RETRY
 *     IBV_QP_MIN_RNR_TIMER
 *     IBV_QP_MAX_QP_RD_ATOMIC
 *     IBV_QP_MAX_DEST_RD_ATOMIC
 *     IBV_QP_ALT_PATH
 *     IBV_QP_PATH_MIG_STATE
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

/**
 * @brief Retrieve the QP number
 *
 * @param qp[in]   - MRC QP
 * @param qpn[out] - Returned QP number
 *
 * @return
 * Returns 0 on success or -1 on error
 */
int mrc_get_qpn(struct mrc_qp *qp,
		uint32_t *qpn);

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
 * Returns 0 on success or -1 on failure. Error semantics like
 * ibv_post_recv().
 */
int mrc_post_recv(struct mrc_qp *qp,
		  struct ibv_recv_wr *wr,
		  struct ibv_recv_wr **bad_wr);

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
 * Returns 0 on success. Error semantics like ibv_post_send().
 */
int mrc_post_send(struct mrc_qp *qp,
		  struct ibv_send_wr *wr,
		  struct ibv_send_wr **bad_wr);

struct mrc_async_event {
	union {
		struct mrc_cq *cq;
		struct mrc_qp *qp;
		int            port_num;
	} element;
	enum ibv_event_type    event_type;
};

/**
 * @brief Get next event
 *
 * Obtain the next event. All events must be acknowledged by
 * mrc_ack_async_event().
 *
 * @param context[in] - MRC context
 * @param event[out]  - Reported event
 *
 * @return
 * Returns 0 on success. Error semantics like ibv_get_async_event().
 */
int mrc_get_async_event(struct mrc_context *mrc_ctx,
			struct mrc_async_event *event);

/**
 * @brief Ack the asynchronous event
 *
 * All async events returned by mrc_get_async_event() should be
 * acknowledged.
 *
 * @param event[in] - MRC async event
 *
 * @return
 * This function does not return any value
 */
void mrc_ack_async_event(struct mrc_async_event *event);

/**
 * @brief Request notifications
 *
 * Requests notifications on the CQ, like ibv_req_notify_cq().
 *
 * @param cq[in]             - MRC CQ
 * @param solicited_only[in] - Request event only on "solicited" events
 *
 * @return
 * Returns 0 on success. Error semantics like ibv_req_notify_cq().
 */
int mrc_req_notify_cq(struct mrc_cq *cq,
		      int solicited_only);

/**
 * @brief Get next CQ event
 *
 * Read the next CQ event. All completion events returned by
 * mrc_get_cq_event() must eventually be acknowledged with
 * mrc_ack_cq_events().
 *
 * @param channel[in]     - MRC channel
 * @param cq[out]         - Returned CQ that has the event
 * @param cq_context[out] - Returned application CQ context
 *
 * @return
 * Returns 0 on success, and -1 on failure. Error semantics like
 * ibv_get_cq_event().
 */
int mrc_get_cq_event(struct mrc_comp_channel *channel,
		     struct mrc_cq **cq,
		     void **cq_context);

/**
 * @brief Acknowledge CQ completion events
 *
 * All completion events which are returned by mrc_get_cq_event() must
 * be acknowledged.  To avoid races, mrc_destroy_cq() will wait for
 * all completion events to be acknowledged, so there should be a
 * one-to-one correspondence between acks and successful gets.  An
 * application may accumulate multiple completion events and
 * acknowledge them in a single call to mrc_ack_cq_events() by passing
 * the number of events to ack in @nevents.
 *
 * @param cq[in]      - MRC CQ
 * @param nevents[in] - Number of events to acknowledge
 *
 * @return
 * Returns no value
 */
void mrc_ack_cq_events(struct mrc_cq *cq,
		       unsigned int nevents);

#ifdef __cplusplus
}
#endif

#endif /* _MRC_API_H_ */
