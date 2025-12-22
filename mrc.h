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
#include <infiniband/verbs.h>

#include <mrc_api_ver.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Max bytes for opaque vendor configuration data.
 */
#define MRC_MAX_VENDOR_CFG_SIZE 128

/**
 * @brief MRC wire (transport) protocol version bit values.
 *
 * Each non-zero constant denotes a distinct on-the-wire protocol version.
 * Devices advertise a bitmap (OR of these bits) via
 * `mrc_device_attr.mrc_protocol_version`. Applications request exactly one
 * bit (or 0 for provider default) in `mrc_qp_init_attr.protocol_version`.
 */
enum mrc_protocol_version {
	/* MRC version unspecified / request provider default */
	MRC_PROTOCOL_VERSION_0	= 0,
	MRC_PROTOCOL_VERSION_1	= 1 << 0,
};

struct mrc_context;
struct mrc_qp_hint;
struct mrc_qp;
struct mrc_cq;
struct mrc_comp_channel;

/**
 * @brief Features supported by the implementation
 */
enum mrc_device_cap {
	/* Device supports sending TRIM NACKs */
	MRC_DEVICE_CAP_TRIM_NACK	= (1<<0),
	/* Device supports Dynamic MPR */
	MRC_DEVICE_CAP_DYNAMIC_MPR	= (1<<1),
};

/**
 * @brief MRC device attributes
 */
struct mrc_device_attr {
	/* Bitmap of all versions supported (see enum mrc_protocol_version).
	 * The value 0 indicates the provider will choose an appropriate
	 * version.
	 */
	uint32_t mrc_protocol_version;

	struct {
		/* Max configurable WIMM value as requestor */
		uint16_t max_wimm;
		/* Max configurable WIMM value as responder */
		uint16_t max_wimm_dest;
	} wimm_attr;

	struct {
		/* Maximum supported MPR (req or rsp) (units = 128 PSNs) */
		uint8_t max_mpr;
		/* Allocation granularity (units = 128 PSNs) */
		uint8_t mpr_resolution;
		/* Non-zero if Dynamic MPR is supported */
		uint8_t dynamic_mpr;
	} mpr_attr;

	struct {
		/* Max number of QP hints supported */
		uint32_t max_qp_hint;
	} qp_attr;

	/* bitmap of additional capabilities (mrc_device_cap) */
	uint32_t cap;
};

/**
 * @brief Query Device
 *
 * Query the device to check MRC support and other attributes.
 * The value returned in `supported` is 0 when MRC support is not
 * available.
 *
 * @param context[in]     - Verbs context
 * @param attrs[out]      - MRC attributes
 * @param supported[out]  - Non-zero if MRC supported
 * @return 0 on success, -1 on failure (errno set)
 */
int mrc_query_device(struct ibv_context *context,
		     struct mrc_device_attr *attr,
		     int *supported);

/**
 * @brief Application-provided context initialization attributes.
 */
struct mrc_context_attr {
	/* API version used */
	uint32_t mrc_api_version_used;
};

/**
 * @brief Create an MRC context
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
 * @param vcontext[in]     - IB Verbs context
 * @param context_attr[in] - optional version selection (may be NULL)
 *
 * @return Pointer to the allocated context on success.
 * @return NULL if the request fails.
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
 * @return 0 on success, -1 on failure (errno set).
 */
int mrc_destroy_context(struct mrc_context *mrc_ctx);

/**
 * @brief Create a completion channel
 *
 * Create a completion channel
 *
 * @param mrc_ctx[in]  - MRC context
 * @param channel[out] - Created MRC channel
 *
 * @return 0 on success.
 * @return Errors like ibv_create_comp_channel().
 */
int mrc_create_comp_channel(struct mrc_context *mrc_ctx,
			    struct mrc_comp_channel **channel);

/**
 * @brief Retrieve the completion channel's file descriptor
 *
 * @param channel[in] - MRC completion channel
 * @param fd[out]     - Returned file descriptor
 *
 * @return 0 on success, -1 on failure (errno set).
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
 * @return 0 on success.
 * @return Errors like ibv_destroy_comp_channel().
 */
int mrc_destroy_comp_channel(struct mrc_comp_channel *channel);

/**
 * @brief Create a CQ
 *
 * Create a CQ
 *
 * @param mrc_ctx[in]     - MRC context to use
 * @param cqe[in]         - Minimum number of entries required for CQ
 * @param cq_context[in]  - Application context
 * @param channel[in]     - Completion channel
 * @param comp_vector[in] - Completion vector to signal completion events
 *
 * @return Pointer to the allocated CQ on success.
 * @return NULL if the request fails. Errors like ibv_create_cq().
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
 * @return Returns the number of completions found on success. If the return
 *         value is >=0 and less than num_entries, then the CQ was emptied.
 * @return -1 on error.
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
 * @return Returns 0 on success.
 * @return Errors like ibv_destroy_cq().
 */
int mrc_destroy_cq(struct mrc_cq *cq);

/**
 * @brief Traffic pattern hint attributes describing expected QP usage.
 */
struct mrc_qp_hint_attr {
	/* Number of QPs using this hint that are sending data to the same
	 * peer (i.e., same destination IP address) while this QP is sending
	 * data.
	 *
	 * For example, in a collective where the application maintains
	 * 2 QPs per destination IP, the value of this field is 2.
	 */
	int num_qps_per_peer;

	/* Number of different peers (i.e., different destination IP
	 * addresses) that QPs using this hint are sending data to
	 * simultaneously.
	 */
	int num_send_peers;
	/* Number of remote QPs that will target the same peer (i.e., same
	 * destination IP address) when this QP is sending data.
	 *
	 * For example, this parameter can be considered as the incast
	 * experienced by the target of this QP.
	 */
	int num_remote_recv_peers;

	/* vendor specific hint */
	uint8_t vendor_cfg[MRC_MAX_VENDOR_CFG_SIZE];
};

/**
 * @brief MRC QP hint attribute mask
 */
enum mrc_qp_hint_attr_mask {
	MRC_QP_HINT_NUM_QPS_PER_PEER		= 1 << 0,
	MRC_QP_HINT_NUM_SEND_PEERS		= 1 << 1,
	MRC_QP_HINT_NUM_REMOTE_RECV_PEERS	= 1 << 2,
	MRC_QP_HINT_VENDOR_CFG			= 1 << 31
};

/**
 * @brief Container for QP hint initialization attributes.
 */
struct mrc_qp_hint_init_attr {
	struct mrc_qp_hint_attr attr;
};

/**
 * @brief Create an MRC QP hint
 *
 * A QP hint consists of a set attributes that give some hints to the provider
 * regarding the expected traffic patterns across a set of QPs that will be
 * assigned to the QP hint resource. Usage of the QP hint is optional though
 * helps the provider optimize the allocation of underlying resources given
 * the application's desired usage scenario.
 *
 * @param mrc_ctx[in]   - MRC context
 * @param init_attr[in] - MRC_QP hint init attributes
 *
 * @return Pointer to the created QP hint on success.
 * @return NULL if the request fails.
 */
struct mrc_qp_hint *mrc_create_qp_hint(
	struct mrc_context *mrc_ctx,
	struct mrc_qp_hint_init_attr *init_attr);

/**
 * @brief Destroy an MRC QP hint
 *
 * Destroy an MRC QP hint.
 *
 * @param qp_hint[in] - MRC QP hint
 *
 * @return 0 on success, -1 on failure (errno set: EINVAL invalid,
 *         EBUSY in use).
 */
int mrc_destroy_qp_hint(struct mrc_qp_hint *qp_hint);

/**
 * @brief Query an MRC QP hint
 *
 * Query an MRC QP hint.
 *
 * @param qp_hint[in]           - MRC QP hint
 * @param qp_hint_attr[out]     - MRC QP hint attributes
 * @param qp_hint_attr_mask[in] - MRC QP hint attributes to query
 *
 * @return 0 on success, -1 on failure (errno=EINVAL).
 */
int mrc_query_qp_hint(struct mrc_qp_hint *qp_hint,
		      struct mrc_qp_hint_attr *qp_hint_attr,
		      int qp_hint_attr_mask);

/**
 * @brief Modify an MRC QP hint
 *
 * Modify an MRC QP hint.
 *
 * @param qp_hint[in]           - MRC QP
 * @param qp_hint_attr[in]      - MRC QP hint attributes
 * @param qp_hint_attr_mask[in] - MRC QP hint attributes to modify
 *
 * @return 0 on success, -1 on failure (errno=EINVAL).
 */
int mrc_modify_qp_hint(struct mrc_qp_hint *qp_hint,
		       struct mrc_qp_hint_attr *qp_hint_attr,
		       int qp_hint_attr_mask);

/**
 * @brief Attributes required to create an MRC QP.
 */
struct mrc_qp_init_attr {
	void               *qp_context;
	struct mrc_cq      *send_cq;
	struct mrc_cq      *recv_cq;
	struct ibv_qp_cap   cap;
	int                 sq_sig_all;
	struct ibv_pd      *pd;
	/* see enum ibv_qp_create_send_ops_flags */
	uint64_t            send_ops_flags;
	/* The version of MRC wire protocol to use. The value `0` here refers
	 * to the provider's default version.
	 */
	enum mrc_protocol_version    protocol_version;
};

/**
 * @brief Create an MRC QP
 *
 * Create an MRC QP.
 *
 * @param mrc_ctx[in]     - MRC context
 * @param mrc_qp_attr[in] - MRC QP init attributes
 *
 * @return Pointer to the created QP on success.
 * @return NULL if the request fails. Errors like ibv_create_qp().
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
 * @return 0 on success.
 * @return Errors like ibv_destroy_qp().
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
 * INIT        MRC_QP_HINT
 *             MRC_QP_EV_PROFILE_ID
 *             MRC_QP_CC_PROFILE_ID
 *
 * RTR         MRC_QP_MAX_WIMM_DEST
 *             MRC_QP_MPR_DEST
 *             MRC_QP_DYNAMIC_MPR
 *
 * RTS         MRC_QP_MAX_WIMM
 *             MRC_QP_MPR
 *             MRC_QP_RETRY_CNT
 *             MRC_QP_TIMEOUT
 */
enum mrc_qp_attr_mask {
	/* Max WIMM as requestor */
	MRC_QP_MAX_WIMM			= 1 << 0,
	/* Max WIMM as responder */
	MRC_QP_MAX_WIMM_DEST		= 1 << 1,
	/* MPR as requestor */
	MRC_QP_MPR			= 1 << 2,
	/* MPR as responder */
	MRC_QP_MPR_DEST			= 1 << 3,
	/* Dynamic MPR (req/rsp) */
	MRC_QP_DYNAMIC_MPR		= 1 << 4,
	/* QP ACK timeout */
	MRC_QP_TIMEOUT			= 1 << 5,
	/* EV Profile */
	MRC_EV_PROFILE_ID		= 1 << 6,
	/* CC Profile */
	MRC_CC_PROFILE_ID		= 1 << 7,
	/* QP hint */
	MRC_QP_HINT			= 1 << 8,
	/* Linear + exponential retry counter */
	MRC_QP_RETRY_CNT		= (1<<9),
	/* Additional capabilities */
	MRC_QP_CAP			= (1<<10),
	/* Vendor specific configuration data */
	MRC_QP_VENDOR_CFG		= 1 << 31
};

enum mrc_qp_cap {
	/* Responder TRIM NACK support */
	MRC_QP_TRIM_NACK_DEST		= (1<<0),
	/* Enable Dynamic MPR on this QP (req & rsp role). */
	MRC_QP_DYNAMIC_MPR		= (1<<1),
};

/**
 * @brief Runtime / modifiable MRC QP attributes (query/modify interface).
 */
struct mrc_qp_attr {
	struct {
		/* Requestor MPR value; unit=128 PSNs */
		uint8_t mpr;
		/* Responder MPR value; unit=128 PSNs */
		uint8_t mpr_dest;
		/* if 1/true, enable Dynamic MPR support */
		uint8_t dynamic_mpr;
	} mpr;

	struct {
		/* Max inflight WIMMs as requestor */
		uint16_t max_wimm;
		/* Max inflight WIMMs as responder */
		uint16_t max_wimm_dest;
	} wimm;

	/* Local ACK timeout; 1.024 * 2^timeout us. Max val = 24 (17.17s) */
	uint8_t timeout;

	/* Application specified profile. The profile is learned OOB by the
	 * application and is used by the provider to associate the QP with
	 * an EV and CC profile that was previously programmed by a system
	 * controller.
	 */
	struct {
		uint64_t ev_profile_id;
		uint64_t cc_profile_id;
	} profile;

	struct {
		/* Linear (fixed interval) retry limit. Max: 7 */
		uint8_t linear_cnt;
		/* Exponential backoff retry limit. Max: 25 (25 = infinite) */
		uint8_t exp_cnt;
	} retry;

	/* QP hint, if NULL then no hint is assigned */
	struct mrc_qp_hint *qp_hint;

	/* bitmap of additional features (mrc_qp_cap) */
	uint32_t cap;

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
 * @param mrc_attr[out]     - MRC QP attributes returned
 * @param mrc_attr_mask[in] - MRC QP attributes requested
 * @param init_attr[out]    - Additional MRC attributes returned
 *
 * @return 0 on success.
 * @return Errors like ibv_query_qp().
 */
int mrc_query_qp(struct mrc_qp *qp,
		 struct ibv_qp_attr *vattr,
		 int vattr_mask,
		 struct mrc_qp_attr *mrc_attr,
		 int mrc_attr_mask,
		 struct mrc_qp_init_attr *init_attr);

/**
 * @brief Modify a QP
 *
 * Modify a QP. Caller provides ibv_qp_attr and mrc_qp_attr structures and
 * masks.
 *
 * The following IBV attributes are NOT supported:
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
 * The following IBV attributes are modified:
 *     IBV_QP_AV: SGID is retrieved using ibv_query_gid() on the ibv_context
 *                associated with mrc_context
 *
 * QP lifecycle: set/query via IBV_QP_STATE; assert via IBV_QP_CUR_STATE.
 *
 * Capabilities:
 *  - Dynamic MPR: Device advertises `MRC_DEVICE_CAP_DYNAMIC_MPR`.
 *    Enable per-QP via `MRC_QP_DYNAMIC_MPR`; effective only if both peers
 *    enable it.
 *
 *  - TRIM NACK: Device advertises `MRC_DEVICE_CAP_TRIM_NACK`.
 *    Enable responder per-QP via `MRC_QP_TRIM_NACK_DEST` (peer must support
 *    generation).
 *
 * @param qp[in]            - MRC QP
 * @param vattr[in]         - Libibverbs attributes to modify
 * @param vattr_mask[in]    - Libibverbs attribute mask
 * @param mrc_attr[in]      - MRC QP attributes to modify
 * @param mrc_attr_mask[in] - MRC QP attributes to modify
 *
 * @return 0 on success.
 * @return Errors like ibv_modify_qp().
 */
int mrc_modify_qp(struct mrc_qp *qp,
		  struct ibv_qp_attr *vattr,
		  int vattr_mask,
		  struct mrc_qp_attr *mrc_attr,
		  int mrc_attr_mask);

/**
 * @brief Retrieve the QP number
 *
 * @param qp[in]   - MRC QP
 * @param qpn[out] - Returned QP number
 *
 * @return 0 on success, -1 on failure (errno set).
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
 * @return 0 on success.
 * @return -1 on failure. Errors like ibv_post_recv().
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
 * @return 0 on success.
 * @return Errors like ibv_post_send().
 */
int mrc_post_send(struct mrc_qp *qp,
		  struct ibv_send_wr *wr,
		  struct ibv_send_wr **bad_wr);

/**
 * @brief Asynchronous event record
 */
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
 * @return 0 on success.
 * @return Errors like ibv_get_async_event().
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
 * @return void
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
 * @return 0 on success.
 * @return Errors like ibv_req_notify_cq().
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
 * @return 0 on success.
 * @return -1 on failure. Errors like ibv_get_cq_event().
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
 * @return void
 */
void mrc_ack_cq_events(struct mrc_cq *cq,
		       unsigned int nevents);

#ifdef __cplusplus
}
#endif

#endif /* _MRC_API_H_ */
