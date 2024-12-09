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
 * Copyright (c) 2024, Advanced Micro Devices (AMD), Inc.  All rights
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
	MRC_VERSION_0 	= 0, /* MRC not supported */
	MRC_VERSION_1 	= (1 << 0),
};

/**
 * Optional features supported by the implementation.
 */
enum mrc_attr_opt {
	/* The implementation supports the capability to update EV after
	 * the QP has transitioned past the RTR stage */
	MRC_OPT_CAP_UPDATE_EV_RTS 	= (1<<0),
	/* The implementation supports EV Event CQs */
	MRC_OPT_CAP_EV_EVENT_CQ 	= (1<<1),
	/* The implementation supports dynamic MPR (requestor or responder role). */
	MRC_OPT_CAP_DYNAMIC_MPR 	= (1<<2),
};

struct mrc_context;
struct mrc_qp;
struct mrc_cq;
struct mrc_comp_channel;
struct mrc_ev_array;

struct mrc_attr {
	/* bitmap indicating all versions supported. see enum mrc_version */
	uint32_t mrc_version;
	struct {
		uint16_t max_wimm; /**< Max configurable wimm value as requestor.  */
		uint16_t max_wimm_dest; /**< Max configurable wimm value as responder. */
	} wimm_attr;
	struct {
		uint16_t default_mpr; /**< Default MPR as requestor and/or responder; unit = 128 PSNs. */
		uint16_t max_mpr; /**< Max configurable MPR as requestor and/or responder; unit = 128 PSNs. */
	} mpr_attr;
	/* bitmap indicating all optional features supported. see mrc_attr_opt */
	uint32_t opt_attr;
};

/**
 * @brief Query Device
 *
 * Query the device to check MRC support and other
 * attributes.
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
 * @brief Create a MRC lib context
 *
 * Create an MRC library context.
 * `struct mrc_context` provides an instance of the MRC library.
 * It is the parent object for all other objects.
 * The primary function of this is to track the objects and clean up.
 * The context structure eliminates the need for any global objects
 * within the MRC library and supports multiple user libraries using MRC.
 * Additionally, it provides the implementation an opportunity to allocate
 * any system resources.
 *
 * The application provides the version of the API that is in use.
 * This allows the underlying implementation to adapt accordingly.
 *
 * @param context[in]  - IB Verbs context
 * @param mrc_api_version_used[in] - MRC version used by the application
 *
 * @return
 * Returns a pointer to the allocated context on success or NULL if
 * the request fails.
 */
struct mrc_context* mrc_create_context(struct ibv_context *context,
			uint32_t mrc_api_version_used);


/**
 * @brief Destroy the MRC lib context
 *
 * Destroy the MRC lib context.
 *
 * @param[in] mrc_ctx - MRC context
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
 * @param channel[in] 	- MRC completion channel
 * @param fd[out]	- Returned file descriptor
 *
 * @return
 * Returns 0 on success or -1 on error.
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
 * @param cqe[in]        - Minimum number of entries required for CQ
 * @param cq_context[in] - application context
 * @param channel[in]	 - completion channel
 * @param comp_vector[in] - Completion vector to signal completion events
 *
 * @return
 * Returns a pointer to the allocated CQ on success or NULL if
 * the request fails. Errors like ibv_create_cq().
 */
struct mrc_cq* mrc_create_cq(struct mrc_context *mrc_ctx,
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
 * Destroy a CQ
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
 * Returns a pointer to the created QP on success or NULL if
 * the request fails. Errors like ibv_create_qp().
 */
struct mrc_qp* mrc_create_qp(struct mrc_context *mrc_ctx,
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

#define MRC_MAX_VENDOR_CFG_SIZE 128

enum mrc_qp_attr_mask {
	/**
	 * @brief Attribute to set the maximum number of inflight WIMM operations when the QP is acting in requestor role.
	 *
	 * Modifiable in RTS state only. Implementations may optionally pre-populate the field with a value.
	 */
	MRC_QP_ATTR_MAX_WIMM		  = (1<<0),
	/**
	 * @brief Attribute to set the maximum number of inflight WIMM operations when the QP is acting in responder role.
	 *
	 * Modifiable in RTR state only. Implementations may optionally pre-populate the field with a value.
	 */
	MRC_QP_ATTR_MAX_WIMM_DEST	  = (1<<1),
	/* maximum retry count in exponential range */
	MRC_QP_ATTR_RETRY_CNT_EXP	  = (1<<2),
	/* EV array to use for the MODIFY or QUERY operation */
	MRC_QP_ATTR_EV_ARRAY		  = (1<<3),
	/* maximum count of EVs for the QP */
	MRC_QP_ATTR_MAX_EV_COUNT	  = (1<<4),
	/* maximum value of the EV for the QP */
	MRC_QP_ATTR_MAX_EV_VAL	          = (1<<5),
	/* manipulate EV monitored state mask */
	MRC_QP_ATTR_EV_STATE_MONITOR_MASK = (1<<6),
	/* MPR attributes */
	MRC_QP_ATTR_MPR =               (1<<7),
	/* QP fixed/exponential retry counter */
	MRC_QP_RETRY_CNT =              (1<<8),
	/* QP ack timeout */
	MRC_QP_ACK_TIMEOUT =            (1<<9),
        /* vendor specific configuration data */
	MRC_QP_ATTR_VENDOR_CFG		  = (1<<31)
};


/**
 * @brief Supported EV states.
 *
 */
enum mrc_ev_state {
	MRC_EV_GOOD             = (1<<0),
	MRC_EV_ASSUMED_BAD      = (1<<1),
	MRC_EV_DENIED	        = (1<<2),
};

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
 * 
 * @return
 * Returns a pointer to the created array on success or NULL if
 * the request fails.
 */
struct mrc_ev_array* mrc_create_ev_array(struct mrc_context *mrc_ctx, int count,
				enum mrc_ev_state *state_array,
				uint32_t *val_array);

/**
 * @brief Destroy an EV array
 * 
 * Destroy an EV array
 * 
 * @param ev_array[in] - EV array to destroy
 * 
 * @return
 * Returns 0 on success or -1 on failure.
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
 * Returns 0 on success or -1 on error.
 */
int mrc_get_ev_state(struct mrc_ev_array *ev_array, int index, enum mrc_ev_state *state);

/**
 * @brief Get the value of an EV
 * 
 * @param ev_array[in] - EV array
 * @param index[in] - Index of the EV entry
 * @param ev[out] - Value of the EV
 * 
 * @return
 * Returns 0 on success or -1 on error.
 */
int mrc_get_ev(struct mrc_ev_array *ev_array, int index, uint32_t *ev);

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
 * Returns 0 on success or -1 on error.
 */
int mrc_update_ev_state(struct mrc_ev_array *ev_array, int index, enum mrc_ev_state state);

/**
 * @brief Update the value of an EV
 * 
 * NOTE: Updating the value of an EV that is attached to a QP that has
 * transitioned past the RTR stage is an optional feature.
 * See MRC_OPT_CAP_UPDATE_EV_RTS.
 *
 * @param ev_array[in] - EV array
 * @param index[in] - Index of the EV entry
 * @param ev[in] - value to set for the entry
 * 
 * @return
 * Returns 0 on success or -1 on error. When the feature is not supported,
 * returns -ENOTSUP.
 */
int mrc_update_ev(struct mrc_ev_array *ev_array, int index, uint32_t ev);

struct mrc_qp_attr {
	uint16_t max_wimm; /**< Must be in the range [0, mrc_attr.wimm]. */
	uint16_t max_wimm_dest; /**Must be in the range [0, mrc_attr.wimm_dest]. */
	struct {
		uint8_t fixed_retry_cnt; /**< Fixed interval retry count. Max value = 8. */
		uint8_t exp_retry_cnt; /**< Exponential retry count. Max val = 32 (infinite retry) */
	} retry_cnt;
	uint8_t ack_timeout; /**< Local ack timeout for all paths in 1.024us units. Max val = 26 (68.7s) */
	uint16_t max_ev_per_qp; /* max number of EVs per QP */
	uint32_t max_ev; 	/* maximum value of each EV */
	struct mrc_ev_array *ev_array;
	/** An event is generated when any EV's state *
	 transitions to a monitored state in the mask.  Only
	 EV_ASSUMED_BAD and EV_GOOD masking is supported.  Bit offsets
	 for states match the corresponding value in mrc_ev_state.*/
	int ev_state_monitor_mask;
	uint16_t mpr; /**< Requestor and responder MPR value; unit=128 PSNs. */
	bool rsp_dyn_mpr_en; /**< Responder dynamic MPR support. */
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
 *    mrc_get_ev_state() / mrc_get_ev().
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
 * Modify a QP.
 * 
 * MRC_QP_ATTR_EV_ARRAY is supported during the following transitions:
 * 1. INIT -> RTR - provides the initial set of EVs for the QP
 *    Note: after the QP has been modified to RTR state, the number of
 *    EVs used by this QP is fixed to the number of entries in the EV
 *    array.
 * 2. RTS -> RTS - provides the updated set of EVs for the QP.
 *    Note: updating the EV values in this stage is an optional feature.
 *    See MRC_OPT_CAP_UPDATE_EV_RTS.
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
 * @param qp[in] 	- MRC QP
 * @param qpn[out] 	- Returned QP number
 *
 * @return
 * Returns 0 on success or -1 on error
 */
int mrc_get_qpn(struct mrc_qp *qp, uint32_t *qpn);


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
		struct mrc_cq  *cq;
		struct mrc_qp  *qp;
		int		port_num;
	} element;
	enum ibv_event_type	event_type;
};

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
 * @param event[in]		- MRC async event
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
 * @param cq[in]	- MRC CQ
 * @param solicited_only[in]	- Request event only on "solicited" events
 *
 * @return
 * Returns 0 on success. Error semantics like ibv_req_notify_cq().
 */

int mrc_req_notify_cq(struct mrc_cq *cq, int solicited_only);

/**
 * @brief Get next CQ event
 *
 * Read the next CQ event.
 * All completion events returned by mrc_get_cq_event() must
 * eventually be acknowledged with mrc_ack_cq_events().
 *
 * @param channel[in] 	- MRC channel
 * @param cq[out] 	- Returned CQ that has the event
 * @param cq_context[out] - Returned application CQ context
 *
 * @return
 * Returns 0 on success, and -1 on failure. Error semantics like
 * ibv_get_cq_event().
 */
int mrc_get_cq_event(struct mrc_comp_channel *channel,
			struct mrc_cq **cq, void **cq_context);

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
 * @param cq[in]	- MRC CQ
 * @param nevents[in]	- Number of events to acknowledge
 *
 * @return
 * Returns no value
 */
void mrc_ack_cq_events(struct mrc_cq *cq, unsigned int nevents);

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
 * @brief Create an EV Event CQ
 *
 * EV CQs are used to obtain EV Events. They differ
 * from other CQs in that they do not support CQ overruns.
 *
 * @param mrc_ctx[in]    - MRC context to use
 * @param cqe[in]        - Minimum number of entries required for CQ
 * @param cq_context[in] - application context
 * @param channel[in]	 - completion channel
 * @param comp_vector[in] - Completion vector to signal completion events
 *
 * @return
 * Returns a pointer to the allocated CQ on success or NULL if
 * the request fails. Errors like ibv_create_cq().
 */
struct mrc_cq* mrc_create_ev_event_cq(struct mrc_context *mrc_ctx,
			   int cqe,
			   void *cq_context,
			   struct mrc_comp_channel *channel,
			   int comp_vector);

/**
 * @brief Poll for EV Events
 * 
 * Polls for an EV event
 *
 * @param ev_cq[in]       - EV event completion queue
 * @param num_entries[in] - Number of completion entries
 * @param ev_event[out]   - Obtained EV event completion entries
 *
 * @return
 * Returns the number of completions found on success or -1 on error.
 * If the return value is >=0 and less than num_entries, then the CQ
 * was emptied.
 */
int mrc_poll_ev_event(struct mrc_cq *ev_cq, int num_entries, struct mrc_ev_event *ev_event);

#ifdef __cplusplus
}
#endif

#endif /* _MRC_API_H_ */