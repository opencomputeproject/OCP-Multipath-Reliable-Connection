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

/* Maximum size of opaque vendor configuration data */
#define MRC_MAX_VENDOR_CFG_SIZE 128

enum mrc_version {
	MRC_VERSION_0	= 0, /* MRC not supported */
	MRC_VERSION_1	= (1 << 0),
};

struct mrc_context;
struct mrc_qp_group;
struct mrc_qp_hint;
struct mrc_qp;
struct mrc_cq;
struct mrc_comp_channel;

struct mrc_attr {
	/* bitmap of all versions supported (see enum mrc_version) */
	uint32_t mrc_version;
};

/**
 * @brief Query Device
 *
 * Query the device to check MRC support and other attributes.
 *
 * @param context[in] - IB Verbs context
 * @param attrs[out]  - MRC attributes
 *
 * @return 0 on success.
 * @return Errors like ibv_query_device().
 */
int mrc_query_device(struct ibv_context *context,
			 struct mrc_attr *attr);

/* Context attributes declare the application's usage of MRC */
struct mrc_context_attr {
	/* Value from mrc_version */
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
 * @return 0 on success.
 * @return -1 on failure.
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
 * @return 0 on success.
 * @return -1 on failure.
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
 * @param cq_context[in]  - application context
 * @param channel[in]     - completion channel
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
 * @brief MRC QP group attribute mask
 */
enum mrc_qp_group_attr_mask {
	MRC_QP_GROUP_NUM_QPS	= (1<<0),
};

/**
 * @brief MRC QP group attributes
 */
struct mrc_qp_group_attr {
	/* The number of QPs that will be using the group. */
	int num_qps;
};

/**
 * @brief MRC QP group initialization attributes
 */
struct mrc_qp_group_init_attr {
	struct mrc_qp_group_attr attr;
};

/**
 * @brief Create an MRC QP group
 *
 * A QP group consists of a set of QPs that are active simultaneously. The
 * group used by a QP is assigned when the QP is created.
 *
 * The usage of QP groups is optional and serves as a hint to the provider
 * to optimize the allocation of underlying resources given the application's
 * desired usage scenario.
 *
 * The number of QPs in a group is specified when the group is created. QPs
 * are added one at a time to the group via the QP hints handle specified when
 * a QP is created. It is an error to add more QPs to the group than num_qps
 * specified during group creation. The QP group can be modified to change
 * the number of QPs within the group. If the number of QPs is made smaller,
 * it cannot be changed to a value less than the current number of QPs
 * assigned to the group.
 *
 * If a QP from a group was destroyed, such as due to network failure, then
 * another QP replacing it can be added to the group.
 *
 * @param mrc_ctx[in]   - MRC context
 * @param init_attr[in] - MRC_QP group init attributes
 *
 * @return Pointer to the created QP group on success.
 * @return NULL if the request fails.
 */
struct mrc_qp_group *mrc_create_qp_group(
	struct mrc_context *mrc_ctx,
	struct mrc_qp_group_init_attr *init_attr);

/**
 * @brief Destroy an MRC QP group
 *
 * Destroy an MRC QP group.
 *
 * @param qp_group[in] - MRC QP group
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EBUSY Profile is still being used by a QP.
 */
int mrc_destroy_qp_group(struct mrc_qp_group *qp_group);

/**
 * @brief Query an MRC QP group
 *
 * Query an MRC QP group.
 *
 * @param qp_group[in]           - MRC QP group
 * @param qp_group_attr[out]     - MRC QP group attributes
 * @param qp_group_attr_mask[in] - MRC QP group attributes to query
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 */
int mrc_query_qp_group(struct mrc_qp_group *qp_group,
			   struct mrc_qp_group_attr *qp_group_attr,
			   int qp_group_attr_mask);

/**
 * @brief Modify an MRC QP group
 *
 * Modify an MRC QP group.
 *
 * @param qp_group[in]           - MRC QP group
 * @param qp_group_attr[in]      - MRC QP group attributes to modify
 * @param qp_group_attr_mask[in] - MRC QP group attributes to modify
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 */
int mrc_modify_qp_group(struct mrc_qp_group *qp_group,
			struct mrc_qp_group_attr *qp_group_attr,
			int qp_group_attr_mask);

/**
 * @brief MRC QP hint attribute mask
 */
enum mrc_qp_hint_attr_mask {
	MRC_QP_HINT_QP_GROUP			= (1<<0),
	MRC_QP_HINT_NUM_QPS_PER_PEER		= (1<<1),
	MRC_QP_HINT_NUM_SEND_PEERS		= (1<<2),
	MRC_QP_HINT_NUM_REMOTE_RECV_PEERS	= (1<<3),
};

/**
 * @brief MRC QP hint attributes
 */
struct mrc_qp_hint_attr {
	/*
	 * The QP group this QP hint belongs to. QP group assignement for a
	 * QP hint is mandatory.
	 *
	 * When the application makes calls to mrc_create_qp() and the QP hint
	 * attribute is specified, the QP hint and associated QP group are
	 * used by the provider to better identify and assign internal
	 * resources for the QP.
	 */
	struct mrc_qp_group *qp_group;

	/*
	 * Number of QPs in this group that are sending data to the same
	 * peer (i.e., same destination IP address) while this QP is sending
	 * data.
	 *
	 * For example, in a collective where the application maintains
	 * 2 QPs per destination IP, the value of this field is 2.
	 */
	int num_qps_per_peer;

	/*
	 * Number of different peers (i.e., different destination IP
	 * addresses) that QPs in this group are sending data to
	 * simultaneously.
	 */
	int num_send_peers;

	/*
	 * Number of remote QPs that will target the same peer (i.e., same
	 * destination IP address) when this QP is sending data.
	 *
	 * For example, this parameter can be considered as the incast
	 * experienced by the target of this QP.
	 */
	int num_remote_recv_peers;
};

/**
 * @brief MRC QP hint initialization attributes
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
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EBUSY Profile is still being used by a QP.
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
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
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
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 */
int mrc_modify_qp_hint(struct mrc_qp_hint *qp_hint,
			   struct mrc_qp_hint_attr *qp_hint_attr,
			   int qp_hint_attr_mask);

/**
 * @brief MRC QP initialization attributes
 */
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
 * Next State  	Required Attributes
 * ----------  	-------------------
 * INIT			MRC_QP_PROFILE_ID
 *				MRC_QP_HINT
 *
 * RTS			MRC_QP_TIMEOUT
 */

enum mrc_qp_attr_mask {
	MRC_QP_PROFILE	= (1<<0),	/* QP profile ID */
	MRC_QP_HINT		= (1<<1),	/* QP hint */
	MRC_QP_TIMEOUT	= (1<<2),	/* Local ACK timeout */
	MRC_QP_VENDOR_CFG = (1<<3),	/* Vendor configuration data */
};

struct mrc_qp_attr {
	uint64_t qp_profile_id;

	/* QP hint, if NULL then no hint is assigned */
	struct mrc_qp_hint *qp_hint;

	/* Local ACK timeout; 1.024 * 2^timeout us. Max val = 24 (17.17s) */
	uint8_t timeout;

	/* Vendor-specific configuration data */
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
 * The following IBV field masks are NOT supported:
 *     IBV_QP_PORT
 *     IBV_QP_TIMEOUT (use MRC_QP_TIMEOUT)
 *     IBV_QP_RETRY_CNT
 *     IBV_QP_RNR_RETRY
 *     IBV_QP_MIN_RNR_TIMER
 *     IBV_QP_MAX_QP_RD_ATOMIC
 *     IBV_QP_MAX_DEST_RD_ATOMIC
 *     IBV_QP_ALT_PATH
 *     IBV_QP_PATH_MIG_STATE
 *
 * The following IBV fields are modified:
 *     IBV_QP_AV:
 *		ibv_ah_attr.port_num: unused
 *		ibv_ah_attr.grh.sgid_index: index passed into mrc_query_gid()
 *		ibv_ah_attr.grh.dgid: from mrc_query_gid()
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
 * @return 0 on success.
 * @return -1 on error.
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

/**
 * @brief MRC QP profile status structure
 */
struct mrc_qp_profile_status {
	/* QP profile identifier. */
	uint64_t profile_id;
	/* Non-zero if the QP profile is allocated and online. */
	int online;
};

/**
 * @brief Query QP profile validity and allocation status
 *
 * Checks if the provided profile ID is valid and allocated, and retrieves
 * basic profile status information.
 *
 * @param mrc_ctx[in]		MRC context handle
 * @param status[in, out]	Profile status information
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EIO    Implementation specific error occurred.
 */
int mrc_query_qp_profile(struct mrc_context *mrc_ctx,
	struct mrc_qp_profile_status *status);

/**
 * @brief Query an MRC device's GID table
 *
 * Retrieves the GID entry at the specified index.
 * All GIDs returned by this function are guaranteed to be configured and
 * available on every port included in the profile's port mask.
 *
 * @param mrc_ctx[in]       MRC context handle
 * @param index[in]         GID table index to query
 * @param gid[out]          Output GID pointer
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EIO    Implementation specific error occurred.
 */
int mrc_query_gid(struct mrc_context *mrc_ctx,
		int index, union ibv_gid *gid);

#ifdef __cplusplus
}
#endif

#endif /* _MRC_API_H_ */
