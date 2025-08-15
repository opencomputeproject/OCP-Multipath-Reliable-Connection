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

#ifndef _MRC_CTL_API_H_
#define _MRC_CTL_API_H_

#include <stdint.h>
#include <infiniband/verbs.h>

#include <mrc.h>
#include <mrc_ctl_api_ver.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MRC_CTL_API_CURRENT_VERSION	MRC_CTL_API_VER(1, 0, 0)

#define MRC_CTL_API_LAST_SUPPORTED_VERSION	MRC_CTL_API_VER(0, 0, 0)

#ifndef MRC_CTL_API_VER_USED
#define MRC_CTL_API_VER_USED MRC_CTL_API_CURRENT_VERSION
#elif MRC_CTL_API_VER_USED == MRC_CTL_API_VER_LATEST
#undef MRC_CTL_API_VER_USED
#define MRC_CTL_API_VER_USED MRC_CTL_API_CURRENT_VERSION
#endif

#if MRC_CTL_API_VER_USED < MRC_CTL_API_LAST_SUPPORTED_VERSION
#error "MRC_CTL_API_VER_USED is less than MRC_CTL_API_LAST_SUPPORTED version"
#elif MRC_CTL_API_VER_USED == MRC_CTL_API_LAST_SUPPORTED_VERSION
#warning "MRC_CTL_API_VER_USED is equal to MRC_CTL_API_LAST_SUPPORTED version, may become obsolete"
#endif

/* Invalid EV definition */
#define MRC_CTL_EV_INVALID (struct mrc_ctl_ev){.val = 0, .port = 0}

enum mrc_ctl_version {
	MRC_CTL_VERSION_0	= 0, /* MRC not supported */
	MRC_CTL_VERSION_1	= (1 << 0),
};


/*****************************************************************************
 * Device Query
*****************************************************************************/
/**
 * @brief Optional control features supported by the implementation
 */
enum mrc_ctl_attr_opt {
	/* Device supports modifying ONLINE EV profiles */
	MRC_CTL_OPT_CAP_EV_PROFILE_MODIFY_ONLINE = (1<<0),
	/* Device supports modifying ONLINE CC profiles */
	MRC_CTL_OPT_CAP_CC_PROFILE_MODIFY_ONLINE = (1<<1),
	/* The implementation supports EV Events */
	MRC_CTL_OPT_CAP_EV_EVENT			= (1<<2),
	/* The implementation supports explicit EV arrays */
	MRC_CTL_OPT_CAP_EV_EXPLICIT			= (1<<3),
	/* The implementation supports generated EV arrays */
	MRC_CTL_OPT_CAP_EV_GENERATED		= (1<<4),
	/*
	* Only contiguous ranges supported in explicit mode. First EV value is
	* base; last is 'base_ev_val + (ev_count - 1)'
	 */
	MRC_CTL_OPT_CAP_EV_EXPLICIT_RANGE	= (1<<5),
	/* The implementation supports EV Probes. */
	MRC_CTL_OPT_CAP_EV_PROBE			= (1<<6),
	/* The implementation supports precise EV Event drop counts. */
	MRC_CTL_OPT_CAP_EV_EVENT_PRECISE_DROP_CNT	= (1<<7),
};

/**
 * @brief Control feature values supported by the implementation
 */
struct mrc_ctl_attr {
	/* bitmap of all versions supported (see enum mrc_ctl_version) */
	uint32_t mrc_ctl_version;

	/* EV attributes */
	struct {
		/* Maximum number of EV profiles supported by the device. */
		uint32_t ev_max_profiles;

		/* Maximum number of EVs available across all profiles. */
		uint32_t ev_max_count;

		/* Free number of EV resources avilable across all profiles. */
		uint32_t ev_free_count;

		/*
		 * Maximum number of EVs supported per profile. If the
		 * controller is supplying an explicit EV array, then that array can
		 * contain at most this many EVs.
		 */
		uint32_t ev_max_count_profile;

		/*
		 * Alignment requirements for the number of EVs that are
		 * required in an explicit EV array. The alignment value implies the
		 * minimum count required and it provides the array sizing
		 * requirements. The array size should be:
		 *   (ev_count_align + (k * ev_count_align))
		 * where 'k' is a multiple chosen by the application. For
		 * example, if a provider supports EVs in multiples of 8, it
		 * would set the values 'ev_count_align = 8'. The total number
		 * of EVs is subject to a maximum of ev_max_count_profile. Value of 0
		 * means any EV count increment is supported.
		 */
		uint32_t ev_count_align;

		/*
		 * Maximum EV value supported per profile. This represents
		 * the number of consecutive bits in an EV value that are
		 * valid. Applies to both explicit and generated EVs. It's
		 * an error if an ev_format_mask in an EV profile defining
		 * generated EVs contains a set of fields that extends past
		 * ev_max_bits.
		 */
		uint32_t ev_max_bits;
	} ev;

	/* CC attributes */
	struct {
		/* Maximum number of CC profiles supported by the device. */
		uint32_t cc_max_profiles;

		/* Array of CC algorithm strings.
		* Last element is NULL; stop iterating at NULL.
		* Consumers must NOT free these pointers.
		 *
		 * The following common strings are defined:
		 *   "uet-1.0-nscc" - UET 1.0 NSCC
		 */
		const char **cc_algorithms;
	} cc;

	/*
	 * Port mask for ports owned by this this function; each bit represents
	 * an active port.  Port numbers match ibv_query_port() (1-based).
	 */
	uint64_t port_mask;

	/* bitmap of all optional features supported (mrc_ctl_attr_opt) */
	uint32_t opt_attr;
};

/**
 * @brief Query Device
 *
 * Query the device to check MRC support and other attributes.
 * Should be called after EV generation fields are configured.
 *
 * @param context[in]    - IB Verbs context
 * @param ctl_attrs[out] - MRC Control attributes
 *
 * @return
 * Returns 0 on success. Error codes as per ibv_query_device().
 */
int mrc_ctl_query_device(struct ibv_context *context,
			 struct mrc_ctl_attr *ctl_attr);


/*****************************************************************************
 * EV Field Widths
*****************************************************************************/
/**
 * @brief EV field width structures
 */
struct mrc_ctl_ev_field {
	uint8_t width;		/* Field width in bits */
	uint8_t min_val;	/* Min. supported val */
	uint8_t max_val;	/* Max. supported val */
};

/**
 * @brief Modify EV field widths
 *
 * Sets hardware EV field widths. Not allowed if any EV or CC profile is
 * not in INIT state.
 *
 * @param mrc_ctx[in]         - MRC context
 * @param ev_fields[in]       - Array containing EV field widths and bounds
 * @param ev_field_count[in]  - Length of ev_fields argument
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EIO Implementation specific error occurred.
 * @retval EBUSY One or more EV or CC profiles are in ONLINE state.
 * @retval E2BIG Supplied parameter combination is unsupportable.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_modify_ev_field_widths(struct mrc_context *mrc_ctx,
			      struct mrc_ctl_ev_field *ev_fields,
			      int ev_field_count);

/**
 * @brief Query the EV field widths.
 *
 * Retrieves the hardware EV field widths.
 *
 * If ev_field_count is less than the total number of EV fields, the function
 * returns an error and sets cur_ev_field_count to the required array size.
 *
 * @param mrc_ctx[in]             - MRC context
 * @param ev_fields[out]          - Populated with EV field widths and bounds
 * @param ev_field_count[in]      - Length of provided ev_fields argument
 * @param cur_ev_field_count[out] - Number of configued EV fields
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EIO Implementation specific error occurred.
 * @return E2BIG EV field count is less than total number of EV fields.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_query_ev_field_widths(struct mrc_context *mrc_ctx,
				  struct mrc_ctl_ev_field *ev_fields,
                  int ev_field_count,
                  int *cur_ev_field_count);


/*****************************************************************************
 * EV Structures
*****************************************************************************/
/**
 * @brief EV types and structures
 */
typedef uint32_t mrc_ctl_ev_t;

struct mrc_ctl_ev {
	mrc_ctl_ev_t val;
	uint8_t port;
};

/**
 * @brief Supported EV states
 */
enum mrc_ctl_ev_state {
	MRC_CTL_EV_GOOD		= (1<<0),
	MRC_CTL_EV_ASSUMED_BAD	= (1<<1),
	MRC_CTL_EV_DENIED	= (1<<2),
	MRC_CTL_EV_UNKNOWN	= (1<<31)
};

/**
 * @brief Supported EV modes
 */
enum mrc_ctl_ev_mode {
	/* Controller will not provide any EVs (vendor managed e.g., ECMP) */
	MRC_CTL_EV_MODE_AUTO		= 0,
	/* Explicit EVs (MRC_CTL_OPT_CAP_EV_EXPLICIT) */
	MRC_CTL_EV_MODE_EXPLICIT	= 1,
	/* Generated EVs (MRC_CTL_OPT_CAP_EV_GENERATED) */
	MRC_CTL_EV_MODE_GENERATED	= 2,
};

/**
 * @brief Replace the value of an EV in an Explicit EV array
 *
 * All replacement EV fields must be within EV field bounds.
 *
 * If the device does not support EV_PROFILE_MODIFY_ONLINE,
 * EV replacement is only allowed when the profile is OFFLINE.
 *
 * If duplicates exist, only one instance of the EV is replaced.
 *
 * When an Explicit EV array profile transitions from INIT to OFFLINE,
 * all EVs are set to MRC_CTL_EV_INVALID. All EVs must be replaced with
 * valid values before moving the profile to ONLINE.  When an EV is replaced
 * its initial state is EV_GOOD.
 *
 * @param mrc_ctx[in]       - MRC context
 * @param profile_id[in]    - QP Profile ID
 * @param cur_ev[in]        - Current EV
 * @param new_ev[in]        - New EV
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval ENOENT EV not found.
 * @retval EIO Implementation specific error occurred.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_replace_ev(struct mrc_context *mrc_ctx,
		       uint64_t ev_profile_id,
		       struct mrc_ctl_ev cur_ev,
		       struct mrc_ctl_ev new_ev);

/**
 * @brief Query an EV's state
 *
 * Supported when a profile is OFFLINE and ONLINE state.
 *
 * @param mrc_ctx[in]       - MRC context
 * @param profile_id[in]    - QP Profile ID
 * @param ev[in]            - EV
 * @param state[out]        - Returned EV state
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval ENOENT EV not found.
 * @retval EIO Implementation specific error occurred.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_query_ev_state(struct mrc_context *mrc_ctx,
		   uint64_t ev_profile_id,
		   struct mrc_ctl_ev ev,
		   enum mrc_ctl_ev_state *state);

/**
 * @brief Modify the state of an explicit or generated EV.
 *
 * All matching EV instances are updated.
 *
 * @param mrc_ctx[in]       - MRC context
 * @param ev_profile_id[in] - EV profile
 * @param ev[in]            - EV to update
 * @param state[in]         - EV state (EV_GOOD or EV_DENIED)
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval ENOENT EV not found.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_modify_ev_state(struct mrc_context *mrc_ctx,
		uint64_t ev_profile_id,
		struct mrc_ctl_ev ev,
		enum mrc_ctl_ev_state state);

/*****************************************************************************
 * EV Profile
*****************************************************************************/
enum mrc_ctl_profile_state {
	MRC_CTL_PROFILE_UNKNOWN,        /* State cannot be reliably determined. */
	MRC_CTL_PROFILE_INIT,           /* Initialized and ready for config. */
	MRC_CTL_PROFILE_OFFLINE,        /* Configured but not usable. */
	MRC_CTL_PROFILE_ONLINE,         /* Is usable. */
};

/**
 * @brief EV profile attribute mask
 */
enum mrc_ctl_ev_profile_attr_mask {
	MRC_CTL_EV_PROFILE_STATE       = 1 << 0,
	MRC_CTL_EV_PROFILE_CUR_STATE   = 1 << 1,
	MRC_CTL_EV_PROFILE_MODE        = 1 << 2,
	MRC_CTL_EV_PROFILE_COUNT       = 1 << 3,
	MRC_CTL_EV_PROFILE_MIN_ACTIVE  = 1 << 4,
	MRC_CTL_EV_PROFILE_EVENT_MASK  = 1 << 5,
};

/**
 * @brief EV Profile attributes
 */
struct mrc_ctl_ev_profile_attr {
	/* Move the profile to this state. */
	enum mrc_ctl_profile_state profile_state;
	/* Current profile state. */
	enum mrc_ctl_profile_state cur_profile_state;

	/*
	 * The EV mode for this profile:
	 * - MRC_CTL_EV_MODE_AUTO: Vendor-defined mode.
	 * - MRC_CTL_EV_MODE_EXPLICIT: Caller provides explicit EV values.
	 * - MRC_CTL_EV_MODE_GENERATED: HW generated within EV field bounds.
	 */
	enum mrc_ctl_ev_mode ev_mode;

	/*
	 * For explicit EVs: the size of the explicit EV array.
	 * For generated EVs: the number of generated EVs.
	 */
	uint32_t ev_count;

	/*
	 * Min number of EVs that must remain active to avoid the situation of
	 * marking too many EVs as ASSUMED_BAD. This value cannot be greater
	 * than ev_count.
	 */
	uint32_t ev_min_active;

	/*
	 * EV event mask for EV state change notifications on this profile.
	 * Only EV_ASSUMED_BAD and EV_GOOD is supported. May be modified when
	 * the profile is in ONLINE state if the provider advertises
	 * EV_PROFILE_MODIFY_ONLINE capability.
	 */
	int ev_event_mask;
};

/*****************************************************************************
 * CC Profile
*****************************************************************************/
/**
 * @brief CC profile attribute mask
 */
enum mrc_ctl_cc_profile_attr_mask {
	MRC_CTL_CC_PROFILE_STATE     = 1 << 0,
	MRC_CTL_CC_PROFILE_CUR_STATE = 1 << 1,
	MRC_CTL_CC_PROFILE_ALGORITHM = 1 << 2,
	MRC_CTL_CC_PROFILE_CONFIG    = 1 << 3,
};

/**
 * @brief CC Profile attributes
 */
struct mrc_ctl_cc_profile_attr {
	/* Move the profile to this state. */
	enum mrc_ctl_profile_state profile_state;
	/* Current profile state. */
	enum mrc_ctl_profile_state cur_profile_state;

	/* String describing CC algorithm to associate with this profile. */
	const char *cc_algorithm;

	/* Algorithm-specific configuration structure. */
	const void *cc_config;
};

/*****************************************************************************
 * CC Configuration Structures
*****************************************************************************/

/**
 * @brief NSCC configuration structure
 */
struct mrc_ctl_cc_nscc_cfg {
	uint8_t disable_qa;                /* QA disabled if non-zero */
	uint32_t adjust_bytes_threshold;   /* unit = 1B */
	uint32_t adjust_period_threshold;  /* unit = 1ns */
	uint32_t base_rtt;                 /* unit = 1ns */
	float eta;
	float fi;
	float fi_scale;
	float gamma;
	float max_md_jump;
	uint32_t max_cwnd;                 /* unit = 1B */
	uint8_t qa_gate;
	uint32_t qa_threshold;             /* unit = 1ns */
	uint32_t target_delay;             /* unit = 1ns */
};

/*****************************************************************************
 * EV Profile Management
*****************************************************************************/

/**
 * @brief Modify an EV profile
 *
 * EV profile state machine:
 *   INIT -> OFFLINE -> ONLINE -> OFFLINE -> INIT
 *
 * States:
 *   INIT:    Profile created, not yet configured.
 *   OFFLINE: Configured but inactive; can be modified.
 *   ONLINE:  Active and usable; only limited modifications allowed.
 *
 * Required attributes for state transitions:
 *   To OFFLINE: EV_MODE, EV_COUNT
 *   To ONLINE:  EV_MIN_ACTIVE, EV_EVENT_MASK, EVs (for Explicit EV Array)
 *
 * Allowed in ONLINE state (if EV_PROFILE_MODIFY_ONLINE advertised):
 *   EV_EVENT_MASK, mrc_ctl_replace_ev()
 *
 * @param mrc_ctx[in]     - MRC context
 * @param ev_profile_id[in] - EV Profile ID
 * @param attr[in]        - EV Profile attribute structure
 * @param attr_mask[in]   - Bitmask of EV Profile attribute mask
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EIO Implementation specific error occurred.
 * @retval EPERM Process lacks sufficient permissions.
 * @retval EBUSY One or more active QPs are associated with this profile.
 */
int mrc_ctl_modify_ev_profile(struct mrc_context *mrc_ctx,
				uint64_t ev_profile_id,
				struct mrc_ctl_ev_profile_attr *attr,
				int attr_mask);

/**
 * @brief Query an EV profile
 *
 * Query an EV profile configuration.
 *
 * @param mrc_ctx[in]     - MRC context
 * @param ev_profile_id[in] - EV Profile ID
 * @param attr[out]       - EV Profile attribute structure
 * @param attr_mask[in]   - Bitmask of EV Profile attribute mask
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EIO Implementation specific error occurred.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_query_ev_profile(struct mrc_context *mrc_ctx,
				uint64_t ev_profile_id,
				struct mrc_ctl_ev_profile_attr *attr,
				int attr_mask);

/*****************************************************************************
 * CC Profile Management
*****************************************************************************/

/**
 * @brief Modify a CC profile
 *
 * CC profile state machine:
 *   INIT -> OFFLINE -> ONLINE -> OFFLINE -> INIT
 *
 * States:
 *   INIT:    Profile created, not yet configured.
 *   OFFLINE: Configured but inactive; can be modified.
 *   ONLINE:  Active and usable; only limited modifications allowed.
 *
 * Required attributes for state transitions:
 *   To OFFLINE: CC_ALGORITHM
 *   To ONLINE:  CC_CONFIG
 *
 * Allowed in ONLINE state (if CC_PROFILE_MODIFY_ONLINE advertised):
 *   CC_CONFIG
 *
 * @param mrc_ctx[in]     - MRC context
 * @param cc_profile_id[in] - CC Profile ID
 * @param attr[in]        - CC Profile attribute structure
 * @param attr_mask[in]   - Bitmask of CC Profile attribute mask
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EIO Implementation specific error occurred.
 * @retval EPERM Process lacks sufficient permissions.
 * @retval EBUSY One or more active QPs are associated with this profile.
 */
int mrc_ctl_modify_cc_profile(struct mrc_context *mrc_ctx,
                  uint64_t cc_profile_id,
                  struct mrc_ctl_cc_profile_attr *attr,
                  int attr_mask);

/**
 * @brief Query a CC profile
 *
 * Query a CC profile configuration.
 *
 * @param mrc_ctx[in]     - MRC context
 * @param cc_profile_id[in] - CC Profile ID
 * @param attr[out]       - CC Profile attribute structure
 * @param attr_mask[in]   - Bitmask of CC Profile attribute mask
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EIO Implementation specific error occurred.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_query_cc_profile(struct mrc_context *mrc_ctx,
                uint64_t cc_profile_id,
                struct mrc_ctl_cc_profile_attr *attr,
                int attr_mask);

/****************************************************************************
 * EV Events
*****************************************************************************/
/**
 * @brief EV Event structure
 *
 * EV Event structure. Hardware generates an EV Event for every EV state
 * change that matches monitored EV states in the EV profile's event
 * mask field.
 */
struct mrc_ctl_ev_event {
	uint64_t profile_id;
	struct mrc_ctl_ev ev;
	/*
	 * If MRC_CTL_OPT_CAP_EV_EVENT_PRECISE_DROP_CNT is set, this field
	 * contains the number of EV Events dropped between the previous and
	 * current event delivered to the queue. If not set, this field is
	 * 1/true if any events were dropped between the previous and current
	 * event, and 0 otherwise.
	 */
	uint32_t drop_count;
};

/**
 * @brief Create an EV Event CQ
 *
 * EV CQs are used to obtain EV Events. They differ from other CQs in that
 * they do not support CQ overruns.
 *
 * @param mrc_ctx[in]     - MRC context
 * @param cqe[in]         - Minimum number of entries required for CQ
 * @param cq_context[in]  - application context
 * @param channel[in]     - completion channel
 * @param comp_vector[in] - Completion vector to signal completion events
 *
 * @return
 * Returns a pointer to the allocated CQ on success or NULL if the request
 * fails. Errors like ibv_create_cq().
 */
struct mrc_cq *mrc_ctl_create_ev_event_cq(struct mrc_context *mrc_ctx,
					  int cqe,
					  void *cq_context,
					  struct mrc_comp_channel *channel,
					  int comp_vector);

/**
 * @brief Poll for EV Events
 *
 * Polls the EV Event CQ for EV Events and returns the first num_entries (or
 * all events if the CQ contains fewer than num_entries) in the array pointed
 * to by ev_event.
 *
 * @param ev_cq[in]       - EV Event CQ to poll
 * @param num_entries[in] - Number of EV Events to poll
 * @param ev_event[out]   - Array of EV Event structures
 *
 * @return
 * On success a non-negative value indicating the number of entries written
 * to the ev_event array is returned. On failure, a negative value
 * corresponding to the errno is returned.
 */
int mrc_ctl_poll_ev_event(struct mrc_cq *ev_cq,
			  int num_entries,
			  struct mrc_ctl_ev_event *ev_event);


/****************************************************************************
 * EV Probes
*****************************************************************************/
/**
 * @brief EV Probe Request
 */
struct mrc_ctl_ev_probe_req {
	/* Application provided (request) probe ID. */
	uint16_t probe_id;
	/* Source GID; only ROCE_V2 GID type supported. */
	union ibv_gid sgid;
	/* Destination GID; only ROCE_V2 GID type supported. */
	union ibv_gid dgid;
	/* Probe request EV value and port. */
	struct mrc_ctl_ev req_ev;
	/* Probe response EV value. */
	mrc_ctl_ev_t rsp_ev;
};

/**
 * @brief EV Probe Response
 */
struct mrc_ctl_ev_probe_rsp {
	/* Associated request probe ID for this response. */
	uint16_t probe_id;
	/* RTT; units = 1ns. */
	unsigned int rtt;
	/* 1/true if rtt has been adjusted for responder service time. */
	uint8_t adj_svc_time;
};

/**
 * @brief Send EV Probe requests and wait for responses
 *
 * This non-interruptible function blocks the caller until all responses are
 * received or timeout occurs. Responses are delivered into the response
 * structure in order of arrival. Responses are not buffered between
 * invocations.
 *
 * @param mrc_ctx[in]     - MRC context
 * @param req_tc[in]      - Request (DSCP) traffic class
 * @param req[in]         - An array of requests
 * @param num_req[in]     - length of request array
 * @param rsp_timeout[in] - Waiting period for responses; units = 1ns
 * @param rsp[out]        - An array of response structures
 * @param num_rsp[out]    - Number of responses returned
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
int mrc_ctl_probe_ev(struct mrc_context *mrc_ctx,
		     uint8_t req_tc,
		     struct mrc_ctl_ev_probe_req *req,
		     int num_req,
		     uint32_t rsp_timeout,
		     struct mrc_ctl_ev_probe_rsp *rsp,
		     int *num_rsp);

#ifdef __cplusplus
}
#endif

#endif /* _MRC_CTL_API_H_ */
