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

struct mrc_context;
struct mrc_qp;
struct mrc_cq;
struct mrc_comp_channel;
struct mrc_ev_array;

/**
 * Optional features supported by the implementation.
 */
enum mrc_attr_opt {
	/* The implementation supports the capability to update EV after
	 * the QP has transitioned past the RTR stage */
	MRC_OPT_CAP_UPDATE_EV_RTS 	= (1<<0),
	/* The implementation supports EV Events */
	MRC_OPT_CAP_EV_EVENT    	= (1<<1),
	/* The implementation supports explicit EV array */
	MRC_OPT_CAP_EV_EXP_ARRAY	= (1<<2),
	/* The implementation supports generated EV array,
	   where the values are generated using bitmasks */
	MRC_OPT_CAP_EV_GEN_ARRAY 	= (1<<3),
	/* The implementation supports generated EV array
	   where the initial values are primed explicitly, but other
	   values are generated using the bitmask */
	MRC_OPT_CAP_EV_PRIMED_GEN_ARRAY = (1<<4),
	/* The implementation supports updating EV allowed bits
	 * after the QP has transitioned to RTS */
	MRC_OPT_CAP_UPDATE_EV_ALLOWED_BITS_RTS = (1<<5),
	/* The implementation supports updating EV deny list
	 * after the QP has transitioned to RTS */
	MRC_OPT_CAP_UPDATE_EV_DENY_LIST_RTS = (1<<6),
	/* The implementation only supports ranges of explicit
	 * EV values in the explicit mode. In this mode, the
	 * first EV value supplied is the base, and the last
	 * EV used is first_ev_val + (num_ev - 1). Where
	 * num_ev is the argument to mrc_create_ev_array_*(). */
	MRC_OPT_CAP_EV_EXP_ARRAY_RANGE = (1<<7),
	/* The implementation supports ev_min_allowed_vals in
	 * mrc_ev_gen_allow_fmt. */
	MRC_OPT_CAP_EV_MIN_ALLOWED_VALS = (1<<8),
	/* The implementation supports EV Probe */
	MRC_OPT_CAP_EV_PROBE = (1<<9),
	/* The implementation supports accurate counting of dropped EV
	 * Events. */
	MRC_OPT_CAP_EV_EVENT_PRECISE_DROP_CNT = (1<<10),
};

struct mrc_attr {
	/* bitmap indicating all versions supported. see enum mrc_version */
	uint32_t mrc_version;
	uint16_t max_wimm_dest;
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

struct mrc_ev_gen_allow_fmt {
	/* EV allow mask.
	 * The bitmask can contain several fields.
	 * Bitmask encoding rules are described below.
	 * - The field transitions are marked using alternating 0s and 1s.
	 * - When the number of fields is even, the field described in LSB should be 0s.
	 * - When the number of fields is odd, the field described in LSB should be 1s.
	 * - The above rules result in unused bits in MSB always being 0s.
	 *
	 * For example:
	 * 1. 0xF0F0F0F0 indicates 8 bit fields, each 4 bits wide.
	 * 2. 0xF0F0F0F indicates 7 bit fields, each 4 bits wide.
	 */
	uint32_t		ev_allow_mask;

	/* The max value of each field that is allowed.
	 * The ev_allowed_bits operate in powers of 2, however, there may
	 * be a max value that each field can support.
	 * For example, for a bitmask of 0xF0 (two bitfields),
	 * 0xEC indicates a maximum of 12 for field 1 and 14 for field 2.
	 */
	uint32_t		ev_max_allowed_vals;
	/* The min value of each field that is allowed.
	 * While the ev_max_allowed_vals controls the maximum value,
	 * the ev_min_allowed_vals controls the minimum value the
	 * field may start with. This enables starting the iteration
	 * on the EVs from the provided threshold.
	 * For example, for a bitmask of 0xF0 (two bitfields),
	 * 0xDB indicates a minimum of 11 for field 1 and 13 for field 2.
	 */
	uint32_t		ev_min_allowed_vals;

	/* An realistic example in source routed mode:
	 * Construct an ev_allow_mask and ev_max_allowed_vals for 4 hops (includes
	 * the plane). Since the number of hops is even, the first field mask is 0's
	 * and the last is 1's.
	 *   - 1st hop: 0x7   max = 7   (8 planes)
	 *   - 2nd hop: 0xff  max = 179 (180 links)
	 *   - 3rd hop: 0xf   max = 15  (16 links)
	 *   - 4th hop: 0xf   max = 15  (16 links)
	 *
	 * ev_allow_mask       = 0x000787f8
	 * ev_max_allowed_vals = 0x0003fedf
	 * ev_min_allowed_vals = 0x00000000
	 */
};

enum mrc_ev_mode {
	MRC_EV_MODE_NONE	= 0, /* App will not provide any EVs */
	MRC_EV_MODE_EXP_ARRAY	= 1, /* App will use explicit EVs 
					(see MRC_OPT_CAP_EV_EXP_ARRAY)*/
	MRC_EV_MODE_GEN_ARRAY	= 2, /* App will use EVs via generated arrays
					(see MRC_OPT_CAP_EV_GEN_ARRAY) */
	MRC_EV_MODE_PRIMED_GEN_ARRAY = 3, /* App will use primed EV generated arrays
					(see MRC_OPT_CAP_EV_PRIMED_GEN_ARRAY)*/
};

/* Context attributes declare the application's usage of MRC */
struct mrc_context_attr {
	/* API version used */
	uint32_t mrc_api_version_used;
	enum mrc_ev_mode ev_mode;
	/* EV Gen allow format, required only if Generated EVs are used.
	 *
	 * The mrc_ev_gen_allow_format is chosen by the system administrator,
	 * particularly, for fields such as ev_allow_mask.
	 * The application is expected to consult system vendor and use the
	 * right ev_allow_mask, otherwise, mrc_create_context() may fail with
	 * an error.
	 */
	struct mrc_ev_gen_allow_fmt allow_fmt;

	/* Specify the number of LSB bits in the EV that denote the plane.
	 * When the value is 0: the application is not identifying the plane bits.
	 * When the value is non-zero, the plane bits and their location in the
	 * ev are denoted using 1s.
	 * For example: 0xF indicates LSB 4 bits indicating a plane.
	 */
	uint32_t mrc_ev_num_lsb_plane_bits;
};

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
 * An application can choose to provide NULL as context_attr, to indicate
 * allowing the provider to choose to operate using ECMP or other default
 * modes. In this case, the application cannot call any other EV specific
 * APIs.
 *
 * @param vcontext[in]  - IB Verbs context
 * @param context_attr[in] - MRC version used by the application
 *
 * @return
 * Returns a pointer to the allocated context on success or NULL if
 * the request fails.
 */
struct mrc_context* mrc_create_context(struct ibv_context *vcontext,
			struct mrc_context_attr *context_attr);


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
 * Destroy a completion or EV Event CQ
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

/**
 * @brief Supported EV states.
 *
 */
enum mrc_ev_state {
	MRC_EV_GOOD             = (1<<0),
	MRC_EV_ASSUMED_BAD      = (1<<1),
	MRC_EV_DENIED	        = (1<<2),
};

struct mrc_ev_deny {
	/* The EV bitmasks that should not be used.
	 * The length of this array is given using ev_deny_list_len.
	 * The EVs that are denied will be given by:
	 * (ev & deny_mask) == (deny_value & deny_mask)
	 *
	 * The ev_deny_mask/value_arrays can be freed after the mrc_ev_array is
	 * created, or the associated MODIFY operation returns.
	 */
	uint32_t		deny_mask;
	uint32_t		deny_value;
};

struct mrc_ev_gen_attr {
	/* EV allowed bits.
	 * The bits that a provider can flip to generate valid EV values.
	 * The fields are identified using the ev_allow_fmt.
	 * When using a generated EV arrays, the actual EV values in use
	 * are not generated until the associated QP is in the RTR state.
	 */
	uint32_t		  ev_allowed_bits;

	/* Entries to be denied are specified in ev_deny.
	 * ev_deny_len contains the number of deny entries. */
	struct mrc_ev_deny  	  *ev_deny;
	uint32_t		  ev_deny_list_len;
};

struct mrc_ev_entry {
	/* State of the EV */
	enum mrc_ev_state 	  state;
	/* Value of the EV */
	uint32_t		  val;
};

/**
 * @brief Create an array of EVs in Explicit mode
 *
 * Allocates an array of EVs using explicit EV entries.
 * See capability: MRC_OPT_CAP_EV_EXP_ARRAY.
 *
 * The supplied `entries` array can be freed after the
 * call returns.
 *
 * @param mrc_ctx[in] - MRC context
 * @param num_ev[in]  - Number of EVs
 * @param entries[in] - Array of EVs (should be >= num_ev)
 *
 * @return
 * Returns a pointer to the created array on success or NULL if
 * the request fails.
 */
struct mrc_ev_array* mrc_create_ev_array_explicit(struct mrc_context *mrc_ctx,
			int num_ev,
			struct mrc_ev_entry *entries);

/**
 * @brief Create an array of EVs in Generated mode
 *
 * Allocates an array of EVs using the generated mode.
 * See capability: MRC_OPT_CAP_EV_GEN_ARRAY.
 *
 * @param mrc_ctx[in] - MRC context
 * @param num_ev[in]  - Number of EVs that will be generated and used by
 *                      the provider according to the bitmask.
 * @param gen_attr[in] - EV generation attributes
 *
 * @return
 * Returns a pointer to the created array on success or NULL if
 * the request fails.
 */
struct mrc_ev_array* mrc_create_ev_array_generated(struct mrc_context *mrc_ctx,
			int num_ev,
			struct mrc_ev_gen_attr *gen_attr);

/**
 * @brief Create an array of EVs in Primed and Generated mode
 * 
 * Allocates an array of EVs using the generated mode.
 * See capability: MRC_OPT_CAP_EV_PRIMED_GEN_ARRAY.
 *
 * The supplied `entries` array can be freed after the
 * call returns.
 *
 * @param mrc_ctx[in] - MRC context
 * @param num_ev[in]  - Number of EVs that will be generated and used by
 *                      the provider according to the bitmask.
 * @param entries[in] - Primed array of EVs (should be >= num_ev)
 * @param gen_attr[in] - EV generation attributes
 * 
 * @return
 * Returns a pointer to the created array on success or NULL if
 * the request fails.
 */
struct mrc_ev_array* mrc_create_ev_array_primed_generated(struct mrc_context *mrc_ctx,
			int num_ev,
			struct mrc_ev_entry *entries,
			struct mrc_ev_gen_attr *gen_attr);

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
 * @brief Get the EV entry
 * 
 * This is performed after the EV array has been populated using
 * mrc_query_qp().
 *
 * @param ev_array[in] - EV array
 * @param index[in] - Index of the EV entry
 * @param entry[out] - State of the EV
 * 
 * @return
 * Returns 0 on success or -1 on error.
 */
int mrc_get_ev_entry(struct mrc_ev_array *ev_array, int index, struct mrc_ev_entry *entry);

/**
 * @brief Get the EV deny list entry
 *
 * Retrieve the deny list entry from the EV array after calling mrc_query_qp().
 * See QP attribute MRC_QP_ATTR_EV_DENY_LIST_LEN.
 * 
 * @param ev_array[in] - EV array
 * @param index[in] - Index of deny entry to read (must be less than deny list length)
 * @param deny[out] - Deny list entry
 * 
 * @return
 * Returns 0 on success or -1 on error.
 */
int mrc_get_ev_deny(struct mrc_ev_array *ev_array, int index, struct mrc_ev_deny *deny);

/**
 * @brief Update the EV entry
 *
 * The valid values of the state are: MRC_EV_GOOD and MRC_EV_DENIED.
 * This is performed before the QP is modified using mrc_modify_qp().
 *
 * @param ev_array[in] - EV array
 * @param index[in] - Index of the EV entry
 * @param entry[in] - Updated values for the EV entry
 * 
 * @return
 * Returns 0 on success or -1 on error.
 */
int mrc_update_ev_entry(struct mrc_ev_array *ev_array, int index, struct mrc_ev_entry *entry);

/**
 * @brief Update the generation bits of an EV array
 *
 * Provide an update to the rules of generation of EVs.
 * The new EVs will be generated according to the ev_allowed_bits after
 * the updated mrc_ev_array is provided to the QP using mrc_modify_qp().
 *
 * NOTE: Support for updating the EV generation bits after a QP has transitioned
 * to RTS is indicated using the MRC_OPT_CAP_UPDATE_EV_ALLOWED_BITS_RTS.
 *
 * @param ev_array[in] - EV array
 * @param ev_allowed_bits [in] - Updated allowed_bits
 *
 * @return
 * Returns 0 on success or -1 on error. When the feature is not supported,
 * returns -ENOTSUP.
 */
int mrc_update_ev_array_allowed_bits(struct mrc_ev_array *ev_array,
				uint32_t ev_allowed_bits);

/**
 * @brief Update the deny list
 *
 * Provide an update to the deny list of EVs.
 * The new deny values will be used after the updated EV deny list is
 * provided to the QP using mrc_modify_qp().
 *
 * NOTE: Support for updating the EV deny list after a QP has transitioned
 * to RTS is indicated using the MRC_OPT_CAP_UPDATE_EV_DENY_LIST_RTS.
 * NOTE: Deny lists are supported only for Generated / Primed EV modes. In
 * the explicit EV mode, the application is expected to remove any EVs it
 * does not want to use.
 *
 * @param ev_array[in] - EV array
 * @param deny [in] - Updated array of deny values
 * @param length[in] - Length of the deny list
 *
 * @return
 * Returns 0 on success or -1 on error. When the feature is not supported,
 * returns -ENOTSUP.
 */
int mrc_update_ev_deny_list(struct mrc_ev_array *ev_array,
				struct mrc_ev_deny *deny, uint32_t length);

enum mrc_qp_attr_mask {
	/* maximum inflight WriteIMM operations as Requester */
	MRC_QP_ATTR_MAX_WIMM		  = (1<<0),
	/* maximum inflight WriteIMM operations as Responder */
	MRC_QP_ATTR_MAX_WIMM_DEST	  = (1<<1),
	/* maximum retry count in exponential range */
	MRC_QP_ATTR_RETRY_CNT_EXP	  = (1<<2),
	/* EV array to use for the MODIFY or QUERY operation */
	MRC_QP_ATTR_EV_ARRAY		  = (1<<3),
	/* maximum count of EVs for the QP */
	MRC_QP_ATTR_MAX_EV_COUNT	  = (1<<4),
	/* (Query only) maximum value of the EV for the QP */
	MRC_QP_ATTR_MAX_EV_VAL	          = (1<<5),
	/* manipulate EV monitored state mask */
	MRC_QP_ATTR_EV_STATE_MONITOR_MASK = (1<<6),
	/* (Modify only) EV array values are updated */
	MRC_QP_ATTR_EV_ARRAY_VALUES	  = (1<<7),
	/* (Modify only) EV generation bitmasks are updated */
	MRC_QP_ATTR_EV_ARRAY_ALLOWED_BITS = (1<<8),
	/* (Modify only) EV deny list is updated */
	MRC_QP_ATTR_EV_DENY_LIST	 = (1<<9),
	/* (Query only) Minimum number of EVs that are required to be
	   active, not ASSUMED_BAD for operation of the QP */
	MRC_QP_ATTR_EV_MIN_ACTIVE	 = (1<<10),
	/* (Query only) EV array size being used by the QP.
	 * See mrc_create_ev_array_*() and mrc_get_ev_array_len(). */
	MRC_QP_ATTR_EV_ARRAY_SIZE 	 = (1<<11),
	/* (Query only) Deny list length currently in use */
	MRC_QP_ATTR_EV_DENY_LIST_LEN	 = (1<<12),
	/* (Query only) Maximum number of primed EVs for the QP */
	MRC_QP_ATTR_MAX_EV_PRIMED_COUNT	 = (1<<13),
	/* (Query only) Minimum number of EVs for the QP that the
	 * application must provide if it is supplying an EV array. */
	MRC_QP_ATTR_MIN_NUM_EV		 = (1<<14),
	/* (Query only) Number of EVs for alignment.
	 * If the application is supplying an EV array, then the
	 * array should be sized as: min_num_ev + (k * num_ev_align) */
	MRC_QP_ATTR_NUM_EV_ALIGN	 = (1<<15),
        /* vendor specific configuration data */
	MRC_QP_ATTR_VENDOR_CFG		  = (1<<31)
};

struct mrc_qp_attr {
	uint16_t max_wimm;
	uint16_t max_wimm_dest;
	uint8_t  retry_cnt_exp;
	uint32_t max_ev_per_qp; /* max number of EVs per QP */
	uint32_t max_primed_ev_per_qp; /* max number of primed EVs per QP (QUERY)*/
	uint32_t max_ev_bits; 	/* maximum number of valid bits EV.
				   This is an output value that the provider
				   sets during QUERY operation.
				   This applies to explict as well as generated EVs. */
	uint32_t min_active_ev_per_qp; /* min number of active EVs per QPs
					to avoid the situation of marking many
					EVs to ASSUMED_BAD. The mrc_ev_array
					provided to the QP must have num_evs
					greater than this value. */
	uint32_t num_ev;	/* EV array size being used by the QP (QUERY) */
	uint32_t ev_deny_list_len; /* EV deny list length currently in use (QUERY)*/
	uint32_t min_num_ev;	/* Minimum number of EVs that the EV array should contain.
				   If the application is using EV APIs, then each array
				   should contain at least these many EVs.
				   Value of 0 means any EV count is supported by the provider. */
	uint32_t num_ev_align;  /* Alignment requirements for number of EVs required by the provider.
				   Together with min_num_ev, it provides the EV array sizing requirements
				   The EV array size should be = min_num_ev + (k*num_ev_align),
				   where 'k' is a multiple chosen by the application.
				   For example, if a provider supports EVs in multiples of 8, it would
				   set the values min_num_ev = 8, and num_ev_align = 8.
				   The total number of EVs is subject to a maximum of max_ev_per_qp.

				   Value of 0 means any EV count increment is supported by the provider. */
	struct mrc_ev_array *ev_array;
	/** Events are generated when a QP's EV state transitions to a 
	* state set in ev_event_mask.  For MRC_VERSION_1 QPs, only 
	* EV_ASSUMED_BAD and EV_GOOD is supported.
	* Bit offsets in ev_event_mask match those in mr_ev_state.*/
	unsigned int ev_event_mask;
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
 *
 * 2. When a QP is in RTR or RTS state, the ev_array
 *    should point to an array that is appropriately sized
 *    to contain the EV entries (mrc_qp_attr.num_ev_in_use).
 *    The provider copies the EV values and state
 *    that are in use into the mrc_ev_array. The
 *    application can read the state/values using
 *    mrc_get_ev_state() / mrc_get_ev().
 *
 * 3. For the case of generated EVs, QUERY_QP returns the
 *    EVs that are currently under use.
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
 *
 * 1. INIT -> RTR - provides the initial set of EVs for the QP
 *    Note: after the QP has been modified to RTR state, the number of
 *    EVs used by this QP is fixed to the number of entries in the EV
 *    array it was initially assigned.
 *
 * 2. RTS -> RTS - provides the updated set of EVs for the QP.
 *    Note: updating the EV values in this stage is an optional feature.
 *    See MRC_OPT_CAP_UPDATE_EV_RTS.
 *    The number of EVs in the supplied mrc_ev_array must be the same as
 *    supplied during RTR->RTR transition.
 *
 *    a. If qp_attr_mask is MRC_QP_ATTR_EV_ARRAY_VALUES, then all the
 *       values and states have been explicitly updated using
 *       mrc_update_ev() and mrc_update_ev_state().
 *
 *       This overwrites the ev_array that is currently being used by the
 *       explicitly supplied states/values that are supplied in the mrc_ev_array.
 *
 *    b. If the qp_attr_mask is MRC_QP_ATTR_EV_ALLOWED_BITS_RTS, then the
 *       generation bits have been updated using mrc_update_ev_array_allowed_bits().
 *
 *    c. If the qp_attr_mask is MRC_QP_ATTR_EV_DENY_LIST_RTS, then the
 *       generation bits have been updated using mrc_update_ev_deny_list().
 * 
 * The array entries are copied by value during the modify operations,
 * such that the EV array can be destroyed if so desired by the application.
 *
 * If the same mrc_ev_array is shared between multiple QPs, then the provider can
 * update the EVs associated with the other QPs to reflect any changes before
 * the application modifies the other QPs.
 *
 * modify_qp() uses the full set of EV entries (if provided) to use for the QP.
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
       /**
	* If MRC_OPT_CAP_ACC_DROP_CNT is set, this field contains the
	* number of EV Events dropped between the last and current
	* event delivered to the queue.  If not set, this field is 1
	* if any events were dropped between the last and current
	* event and 0 otherwise.
	*/
	uint32_t drop_count;
};

/**
 * @brief Create an EV Event CQ.
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
 * Polls an EV Event CQ for EV Events.
 *
 * @param ev_cq[in]       	- EV Event CQ to poll
 * @param num_entries[in] 	- Number of EV Events to poll
 * @param ev_event[out]  	- Array of EV Event structures
 *
 * @return
 * Polls the EV Event CQ @c ev_cq for EV Events and returns the first
 * @c num_entries (or all Events if the CQ contains fewer than @c num_entries)
 * in the array ev_event.
 *
 * On success a non-negative value indicating the number of entries written
 * to @c ev_event is returned.  On failure, a negative value corresponding
 * to the @c errno is returned.
 */
int mrc_poll_ev_event(struct mrc_cq *ev_cq, int num_entries, struct mrc_ev_event *ev_event);

/**
 * @brief EV Probe Request
 */
struct mrc_ev_probe_req {
	uint16_t probe_id;  /**< Application provided (request) probe ID. */
	union ibv_gid sgid; /**< Source GID; only ROCE_V2 GID type supported. */
	union ibv_gid dgid; /**< Destination GID; only ROCE_V2 GID type supported. */
	uint32_t req_ev;    /**< Probe request EV. */
	uint32_t rsp_ev;    /**< Probe response EV. */
};

/**
 * @brief EV Probe Response
 */
struct mrc_ev_probe_rsp {
	uint16_t probe_id; /**< Associated request probe ID for this response. */
	unsigned int rtt;  /**< RTT; units = 1ns. */
	bool adj_svc_time; /**< True if rtt has been adjusted for responder service time. */
};

/**
 * @brief Send EV Probe requests and wait for responses.
 *
 * This non-interruptible function blocks the caller until all responses are 
 * received or timeout occurs.  Responses are delivered into the response 
 * structure in order of arrival.  Responses are not buffered between 
 * invocations.
 *
 * @param mrc_ctx[in]      - MRC context to use
 * @param req_tc[in]       - Request (DSCP) traffic class
 * @param req[in]          - An array of requests
 * @param num_req[in]      - length of request array
 * @param rsp_timeout[in]  - Waiting period for responses; units = 1ns
 * @param rsp[out]         - An array of response structures
 * @param num_rsp[out]     - Number of responses returned
 *
 * @retval 0 Success
 * @retval EAGAIN Resource temporarily unavailable; retry later.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EIO Implementation specific error occurred.
 * @retval ENOMEM Error allocating memory for function.
 * @retval ENOTSUP Function not supported.
 * @retval EPERM Process lacks sufficient permissions.
 * @retval ETIMEDOUT Timeout occurred before all responses received.
 */
int mrc_probe_ev(struct mrc_context *mrc_ctx,
		 uint8_t req_tc,
		 struct mrc_ev_probe_req *req,
		 int num_req,
		 uint32_t rsp_timeout,
		 struct mrc_ev_probe_rsp *rsp,
		 int *num_rsp);

#ifdef __cplusplus
}
#endif

#endif /* _MRC_API_H_ */
