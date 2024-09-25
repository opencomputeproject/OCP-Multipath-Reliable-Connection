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
 *
 * Copyright (c) 2024, Broadcom. All rights reserved. The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Copyright (c) 2024, Advanced Micro Devices (AMD), Inc.
 */

#ifndef _MRC_API_H_
#define _MRC_API_H_

#include <stdint.h>
#include <stdbool.h>
#include <infiniband/verbs.h>

#include <mrc_api_ver.h>

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
	MRC_VERSION_0 = 0, /* MRC not supported */
	MRC_VERSION_1 = (1 << 0),
};

/**
 * Optional features supported by the implementation.
 */
enum mrc_attr_opt {
	/* The implementation supports the capability to update EV values after
	 * the QP has transitioned past the RTR stage */
	MRC_OPT_CAP_UPDATE_EV_VAL_RTS = (1<<0)
};

struct mrc_context;
struct mrc_qp;
struct mrc_cq;
struct mrc_comp_channel;
struct mrc_ev_array;

struct mrc_attr {
	enum mrc_version version; /* see enum mrc_version */
	uint16_t max_wimm_dest;
	enum mrc_attr_opt opt_attr;
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
			uint32_t mrc_api_version,
			struct mrc_context **mrc_ctx);


/**
 * @brief Destroy the MRC lib context
 *
 * Destroy the MRC lib context.
 *
 * @param[in] mrc_ctx - MRC context
 * @param[in] ev_cq   - CQ where EV Events are reported
 * @return
 * Returns 0 on success, and -1 on failure. Error code in errno.
 */
int mrc_context_destroy(struct mrc_context *mrc_ctx);

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
	MRC_QP_ATTR_MAX_WIMM		  = (1<<0),
	// maximum inflight WriteIMM operations as Responder
	MRC_QP_ATTR_MAX_WIMM_DEST	  = (1<<1),
	// maximum retry count in exponential range
	MRC_QP_ATTR_RETRY_CNT_EXP	  = (1<<2),
	// EV array to use for the MODIFY or QUERY operation
	MRC_QP_ATTR_EV_ARRAY		  = (1<<3),
	// maximum count of EVs for the QP
	MRC_QP_ATTR_MAX_EV_COUNT	  = (1<<4),
	// maximum value of the EV for the QP
	MRC_QP_ATTR_MAX_EV_VALUE	  = (1<<5),
	// manipulate EV monitored state mask
	MRC_QP_ATTR_EV_STATE_MONITOR_MASK = (1<<6),
        // vendor specific configuration data
	MRC_QP_ATTR_VENDOR_CFG		  = (1<<31)
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

/**
 * @brief Supported EV states.
 *
 */
enum mrc_ev_state {
	MRC_EV_GOOD             = 0x01,
	MRC_EV_ASSUMED_BAD      = 0x02,
	MRC_EV_DENIED	        = 0x04,
};

int mrc_destroy_cq(struct mrc_cq *cq);

/**
 * @brief Create an array of EVs
 * 
 * Allocates an array of EVs
 * 
 * @param mrc_ctx[in] - MRC context
 * @param count[in] - Number of EVs to allocate
 * @param state_array[in] - States to assign to the EVs.
 *				Passing NULL assigns MRC_EV_GOOD as the state.
 *				The state_array contains `count` elements.
 * @param val_array[in] - Values to assign each EV. Passing NULL assigns 0 as the value.
 * 			  The value_array array contains `count` elements.
 * @param ev_array[out] - EV array that was allocated
 * 
 * @return
 * Returns 0 on success.
 */
int mrc_create_ev_array(struct mrc_context *mrc_ctx, int count, enum mrc_ev_state *state_array,
				uint32_t *val_array, struct mrc_ev_array **ev_array);

/**
 * @brief Destroy an EV array
 * 
 * Destroy an EV array
 * 
 * @param ev_array[in] - EV array to destroy
 * 
 * @return
 * Returns 0 on success.
 */
int mrc_destroy_ev_array(struct mrc_ev_array *ev_array);

/**
 * @brief Get the state of an EV
 * 
 * @param ev_array[in] - EV array
 * @param index[in] - Index of the EV entry
 * @param state[out] - State of the EV
 * 
 * @return
 * Returns 0 on success, -1 on error.
 */
int mrc_get_ev_state(struct mrc_ev_array *ev_array, int index, enum mrc_ev_state *state);

/**
 * @brief Get the value of an EV
 * 
 * @param ev_array[in] - EV array
 * @param index[in] - Index of the EV entry
 * @param val[out] - Value of the EV
 * 
 * @return
 * Returns 0 on success, -1 on error.
 */
int mrc_get_ev_val(struct mrc_ev_array *ev_array, int index, uint32_t *val);

/**
 * @brief Update the state of an EV
 *
 * The valid values of the state are: MRC_EV_GOOD and MRC_EV_DENIED.
 * 
 * @param ev_array[in] - EV array
 * @param index[in] - Index of the EV entry
 * @param state[in] - State to set to
 * 
 * @return
 * Returns 0 on success.
 */
int mrc_update_ev_state(struct mrc_ev_array *ev_array, int index, enum mrc_ev_state state);

/**
 * @brief Update the value of an EV
 * 
 * NOTE: Updating the value of an EV that is attached to a QP that has
 * transitioned past the RTR stage is an optional feature.
 * See MRC_OPT_CAP_UPDATE_EV_VAL_RTS.
 *
 * @param ev_array[in] - EV array
 * @param index[in] - Index of the EV entry
 * @param val[in] - value to set for the entry
 * 
 * @return
 * Returns 0 on success, -ENOTSUP when not supported and -1 on error.
 */
int mrc_update_ev_val(struct mrc_ev_array *ev_array, int index, uint32_t val);

struct mrc_qp_attr {
	uint16_t max_wimm;
	uint16_t max_wimm_dest;
	uint8_t  retry_cnt_exp;
	uint16_t max_ev_per_qp; /* max number of EVs per QP */
	uint32_t max_ev_val; /* maximum value of each EV */
	struct mrc_ev_array *ev_array;
	/** Hardware generates an event when any EV's state *
	 transitions to a monitored state in the mask.  Only
	 EV_ASSUMED_BAD and EV_GOOD masking is supported. */
	enum mrc_ev_state  ev_state_monitor_mask;
	uint8_t  vendor_cfg[MRC_MAX_VENDOR_CFG_SIZE];
};

/**
 * @brief Query a QP attributes
 *
 * Queries a QP.
 * 
 * MRC_QP_ATTR_EV_ARRAY mask is used as follows:
 * 
 * 1. Only supported on a QP after it is in RTR state.
 * 2. When a QP is in RTR or RTS state, the ev_array
 *    should point to an array that is appropriately sized
 *    to contain the EV entries. The EV values and state
 *    will be copied into the EV entries in the array. The
 *    application can read the state/values using
 *    mrc_ev_get_state() / mrc_ev_get_val().
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
 * MRC_QP_ATTR_EV_ARRAY is supported during the following transitions:
 * 1. INIT -> RTR - provides the initial set of EVs for the QP
 *    Note: after the QP has been modified to RTR state, the number of
 *    EVs used by this QP is fixed to the number of entries in the EV
 *    array.
 * 2. RTS -> RTS - provides the updated set of EVs for the QP.
 *    Note: updating the EV values in this stage is an optional feature.
 *    See MRC_OPT_CAP_UPDATE_EV_VAL_RTS.
 * 
 * The array entries are copied by value during the modify operations,
 * such that the EV array can be destroyed if so desired by the application.
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
 */
uint32_t mrc_get_qpn(struct mrc_qp *qp);


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
 * @param channel[in] 	- MRC completion channel
 * @param fd[out] 	- fd underlying the completion channel
 *
 * @return
 * Returns 0 on success, -1 on error.
 */
int mrc_get_comp_channel_fd(struct mrc_comp_channel *channel, int *fd);

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
 * @param mrc_ctx[in]    - MRC context to use
 * @param sup_overrun[in] - Detect and report CQ overruns if true
 * @param cqe[in]        - Minimum number of entries required for CQ
 * @param cq_context[in] - application context
 * @param channel[in]	 - completion channel
 * @param comp_vector[in] - Completion vector to signal completion events
 * @param cq[out]        - Created CQ
 *
 * @return
 * Returns 0 on success. Errors like ibv_create_cq()
 */

int mrc_create_cq(struct mrc_context *mrc_ctx,
		  bool sup_overrun,
		  int cqe,
		  void *cq_context,
		  struct mrc_comp_channel *channel,
		  int comp_vector,
		  struct mrc_cq **cq);

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

struct mrc_async_event {
	union {
		struct mrc_cq  *cq;
		struct mrc_qp  *qp;
		int		port_num;
	} element;
	enum ibv_event_type	event_type;
};

/**
 * @brief Request notifications
 *
 * Requests notifications on the CQ, like ibv_req_notify_cq().
 *
 * @param cq[in]	- MRC CQ
 * @param solicited_only[in]	- Request event only on "solicited" events
 *
 * @return
 * Returns 0 on success, and -1 on failure. Error semantics like
 * ibv_req_notify_cq().
 */
int mrc_req_notify_cq(struct mrc_cq *cq, int solicited_only);

/**
 * @brief Get next event
 *
 * Obtain the next event. All events must be acknowledged by
 * mrc_ack_async_event().
 *
 * @param context[in]	- MRC context
 * @param event[out]	- Reported event
 *
 * @return
 * Returns 0 on success, and -1 on failure. Error semantics like
 * ibv_get_async_event().
 */
int mrc_get_async_event(struct mrc_context *mrc_ctx,
			struct mrc_async_event *event);


/**
 * @brief Ack the asynchronous event
 *
 * All async events returned by mrc_get_async_event() should be
 * acknowledged.
 *
 * @param event[in]		- MRC async event
 *
 * @return
 * This function does not return any value
 */
void mrc_ack_async_event(struct mrc_async_event *event);

/**
 * @brief EV Event structure.
 *
 * EV Event structure. Hardware generates an EV Event for every EV
 * state change that matches monitored EV states in the QP's EV monitored
 * state mask field.
 */
struct mrc_ev_event {
  uint32_t qpn;
  uint32_t ev;
  enum mrc_ev_state state;
  bool drop; /**< True if one or more events before this one were dropped. */
};

/**
 * @brief Poll for EV Events
 * 
 * @return
 * Like mrc_poll_cq().
 */

int mrc_poll_ev_event(struct mrc_cq *cq, int num_entries, struct mrc_ev_event *ev_evt);

#endif /* _MRC_API_H_ */

