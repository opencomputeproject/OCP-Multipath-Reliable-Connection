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

/* Maximum number of CC algorithms that can be supported by a device.*/
#define MRC_CTL_CC_NUM_MAX (31)

enum mrc_ctl_version {
	MRC_CTL_VERSION_0	= 0, /* MRC not supported */
	MRC_CTL_VERSION_1	= (1 << 0),
};

/**
 * @brief Optional control features supported by the implementation
 */
enum mrc_ctl_attr_opt {
	/*
	 * The implementation supports the capability to update EV profiles
	 * after one or more QPs using the profile has transitioned to
	 * the RTR stage.
	 */
	MRC_CTL_OPT_CAP_EV_UPDATE_RTS			= (1<<0),
	/* The implementation supports EV Events */
	MRC_CTL_OPT_CAP_EV_EVENT			= (1<<1),
	/* The implementation supports explicit EV arrays */
	MRC_CTL_OPT_CAP_EV_EXP_ARRAY			= (1<<2),
	/*
	 * The implementation supports generated EV arrays where the values
	 * are generated using bitmasks.
	 */
	MRC_CTL_OPT_CAP_EV_GEN_ARRAY			= (1<<3),
	/*
	 * The implementation only supports ranges of explicit EV values in
	 * the explicit mode. In this mode, the first EV value supplied is
	 * the base, and the last EV value is 'first_ev_val + (ev_count - 1)',
	 * where ev_count is defined by the EV profile.
	 */
	MRC_CTL_OPT_CAP_EV_EXP_ARRAY_RANGE		= (1<<4),
	/* The implementation supports EV Probes. */
	MRC_CTL_OPT_CAP_EV_PROBE			= (1<<5),
	/* The implementation supports precise EV Event drop counts. */
	MRC_CTL_OPT_CAP_EV_EVENT_PRECISE_DROP_CNT	= (1<<6),
};

/**
 * @brief Control feature values supported by the implementation
 */
struct mrc_ctl_attr {
	/* bitmap of all versions supported (see enum mrc_ctl_version) */
	uint32_t mrc_ctl_version;

	struct {
		/* Maximum number of EV profiles supported by the device. */
		uint32_t ev_max_profiles;

		/* Number of active EV profiles programmed on the device. */
		uint32_t ev_active_profiles;

		/*
		 * Maximum number of EVs supported per profile. If the
		 * controller is supplying an EV array, then that array can
		 * contain at most this many EVs.
		 */
		uint32_t ev_max_count;

		/*
		 * Alignment requirements for the number of EVs that are
		 * required in an EV array. The alignment value implies the
		 * minimum count required and it provides the EV array sizing
		 * requirements. The EV array size should be:
		 *   (ev_count_align + (k * ev_count_align))
		 * where 'k' is a multiple chosen by the application. For
		 * example, if a provider supports EVs in multiples of 8, it
		 * would set the values 'ev_count_align = 8'. The total number
		 * of EVs is subject to a maximum of ev_max_count. Value of 0
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

	struct {
		/* Maximum number of CC profiles supported by the device. */
		uint32_t cc_max_profiles;

		/* Number of active CC profiles programmed on the device. */
		uint32_t cc_active_profiles;

		/* Array of string pointers listing available CC algorithms.
		 * Consumers may stop iterating at the first NULL value.
		 * The final element is always NULL.
		 *
		 * The following common strings are defined:
		 *   "mrc-smtrk-1.00" - MRCv1 SmaRTTrack 1.00
		 */
		const char *cc_algorithms[MRC_CTL_CC_NUM_MAX + 1];
	} cc;

	/* bitmap of all optional features supported (mrc_ctl_attr_opt) */
	uint32_t opt_attr;
};

/**
 * @brief Query Device
 *
 * Query the device to check MRC support and other attributes.
 *
 * @param context[in]    - IB Verbs context
 * @param ctl_attrs[out] - MRC Control attributes
 *
 * @return
 * Returns 0 on success. Error codes as per ibv_query_device().
 */
int mrc_ctl_query_device(struct ibv_context *context,
			 struct mrc_ctl_attr *ctl_attr);

/**
 * @brief Set the EV generation fields
 *
 * Used to configure the EV generation fields format used by the provider.
 * The generation fields can only be modified if there are not any existing
 * mrc_ctl_ev_profile's of type MRC_CTL_EV_MODE_GEN_ARRAY allocated.
 *
 * A realistic example in source routed mode:
 *
 *   - 1st hop: 0x7(3b)   max = 7    (8 planes)
 *   - 2nd hop: 0xff(8b)  max = 179  (180 links)
 *   - 3rd hop: 0xf(4b)   max = 15   (16 links)
 *   - 4th hop: 0xf(4b)   max = 15   (16 links)
 *
 *   // set the EV generation fields
 *   mrc_ctl_ev_gen_fields_set(mrc_ctx, [ 3, 8, 4, 4 ], 4);
 *
 *   // create an EV profile for generated EVs
 *   mrc_ctl_modify_ev_profile(mrc_ctx,
 *                             { ev_prof_id,
 *                               MRC_CTL_EV_MODE_GEN_ARRAY,
 *                               ... });
 *
 *   // get the current set of generated EVs
 *   mrc_ctl_query_ev_profile(mrc_ctx, ev_prof_id, &ev_prof);
 *
 *   // deny some paths in field 2
 *   for (i = 180; i <= 255; i++) {
 *       for (j = 0; j < ev_prof.ev_count; j++) {
 *           mrc_ctl_ev_t *ev = &ev_prof.u.ev_gen.ev_gen_array[j].val;
 *           if ((ev->val & (0xff << 3)) == (i << 3)) {
 *               mrc_ctl_update_ev(mrc_ctx, ev_prof_id, ev, MRC_CTL_EV_DENIED);
 *           }
 *       }
 *   }
 *
 * @param mrc_ctx[in]      - MRC context
 * @param field_widths[in] - Array of bit widths defining each field
 * @param num_fields[in]   - Number of fields in the array
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_ev_gen_fields_set(struct mrc_context *mrc_ctx,
			      uint8_t *field_widths,
			      int num_fields);

/**
 * @brief Get the EV generation fields
 *
 * Used to get the current configuration of the EV generation fields format
 * used by the provider.
 *
 * Upon return the caller is responsible for freeing the ev_fields array.
 *
 * @param mrc_ctx[in]       - MRC context
 * @param field_widths[out] - Array of bit widths defining each field
 * @param num_fields[out]   - Number of fields in the array
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_ev_gen_fields_get(struct mrc_context *mrc_ctx,
			      uint8_t **field_widths,
			      int *num_fields);

/**
 * @brief Supported EV modes
 */
enum mrc_ctl_ev_mode {
	/* Controller will not provide any EVs (vendor defined e.g., ECMP) */
	MRC_CTL_EV_MODE_AUTO		= 0,
	/* Explicit EVs (MRC_CTL_OPT_CAP_EV_EXP_ARRAY) */
	MRC_CTL_EV_MODE_EXP_ARRAY	= 1,
	/* Generated EVs (MRC_CTL_OPT_CAP_EV_GEN_ARRAY) */
	MRC_CTL_EV_MODE_GEN_ARRAY	= 2,
};

/**
 * @brief Supported EV states
 */
enum mrc_ctl_ev_state {
	MRC_CTL_EV_GOOD		= (1<<0),
	MRC_CTL_EV_ASSUMED_BAD	= (1<<1),
	MRC_CTL_EV_DENIED	= (1<<2),
};

/**
 * @brief EV types and structures
 */
typedef uint32_t mrc_ctl_ev_type_t;
struct mrc_ctl_ev {
	mrc_ctl_ev_type_t val;
	uint8_t port;
};

/**
 * @brief EV entry
 */
struct mrc_ctl_ev_entry {
	/* State of the EV */
	enum mrc_ctl_ev_state state;
	/* EV value and port */
	struct mrc_ctl_ev ev;
};

/**
 * @brief EV profile
 */
struct mrc_ctl_ev_profile {
	/*
	 * The controller specified EV profile identifier. The available EV
	 * profile IDs are in the range of [0..(ev_max_profiles - 1)].
	 */
	uint64_t ev_profile_id;

	/*
	 * EV port mask for this profile; a bitmask of active ports for this
	 * profile. The port mask is used as to determine the active ports
	 * for generated arrays and to validate ports in explicit arrays.  The
	 * numbers used in the mask correspond to port numbers used in 
	 * ibv_query_port() (i.e., 1-based).
	*/
	uint64_t port_mask;

	/*
	 * The EV mode to use for this profile.
	 * - MRC_CTL_EV_MODE_AUTO: No explicit or generated EVs will be
	 *   specified in this profile. Entropy is vendor defined (ECMP).
	 * - MRC_CTL_EV_MODE_EXP_ARRAY: Caller must provide the EV array in
	 *   'ev_exp.ev_exp_array'.
	 * - MRC_CTL_EV_MODE_GEN_ARRAY: the caller must specify the generation
	 *   format profile ID.
	 */
	enum mrc_ctl_ev_mode ev_mode;

	/*
	 * For explicit EVs this field specifies the length of the explicit
	 * EV array. For generated EVs it is the number of EVs that the
	 * provider will generate.
	 */
	uint32_t ev_count;

	/*
	 * Min number of EVs that must remain active to avoid the situation of
	 * marking too many EVs as ASSUMED_BAD. This value cannot be greater
	 * then ev_count.
	 */
	uint32_t ev_min_active;

	union {
		/* For explicit EVs, the following fields are valid. */
		struct {
			/*
			 * The explicit array of EVs programmed in this
			 * profile. The length of the array is specified by
			 * ev_count.
			 * - mrc_ctl_modify_ev_profile() - this field is used
			 *   by the controller to set the EVs to be used
			 * - mrc_ctl_query_ev_profile() - this field is
			 *   allocated and returned by the provider
			 * In both cases, upon return this array can be freed
			 * safely by the caller.
			 */
			struct mrc_ctl_ev_entry *ev_exp_array;
		} ev_exp;

		/* For generated EVs, the following fields are valid. */
		struct {
			/*
			 * The generated array of EVs programmed in this
			 * profile. The length of the array is specified by
			 * ev_count.
			 * - mrc_ctl_modify_ev_profile() - ignored always
			 * - mrc_ctl_query_ev_profile() - this field is
			 *   allocated and returned by the provider
			 * In both cases, upon return this array can be freed
			 * safely by the caller.
			 */
			struct mrc_ctl_ev_entry *ev_gen_array;
		} ev_gen;
	} u;

	/*
	 * EV event mask for EV state change notifications on this profile.
	 * Only EV_ASSUMED_BAD and EV_GOOD is supported.
	 */
	int ev_event_mask;
};

/**
 * @brief Create or modify an EV profile
 *
 * Used to configure an EV profile. Once configured, the specified
 * ev_profile_id can be used by any to be created QP. Some fields
 * may be modified after the EV profile has been created (see
 * mrc_ctl_ev_profile).
 *
 * NOTE: The ev_profile_id is defined by the calling controller application.
 *
 * @param mrc_ctx[in]    - MRC context
 * @param ev_profile[in] - Profile to create/update
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval ENOMEM Unable to create a new profile (max reached).
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_modify_ev_profile(struct mrc_context *mrc_ctx,
			      struct mrc_ctl_ev_profile *ev_profile);

/**
 * @brief Get an EV profile.
 *
 * Get an EV profile configuration.
 *
 * @param mrc_ctx[in]             - MRC context
 * @param ev_profile_id[in]       - Profile to get
 * @param mrc_ctl_ev_profile[out] - Profile's configuration
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_query_ev_profile(struct mrc_context *mrc_ctx,
			     uint64_t ev_profile_id,
			     struct mrc_ctl_ev_profile *ev_profile);

/**
 * @brief Destroy an EV profile
 *
 * Destroy an EV profile. All QPs that are using this EV profile must be
 * destroyed before the profile can be destroyed.
 *
 * @param mrc_ctx[in]       - MRC context
 * @param ev_profile_id[in] - Profile to destroy
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EBUSY Profile is still being used by a QP.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_destroy_ev_profile(struct mrc_context *mrc_ctx,
			       uint64_t ev_profile_id);

/**
 * @brief CC profile
 */
struct mrc_ctl_cc_profile {
	/*
	 * The controller specified CC profile identifier. The available CC
	 * profile IDs are in the range of [0..(cc_max_profiles - 1)].
	 */
	uint64_t cc_profile_id;

	/*
	 * String describing CC algorithm to associate with this profile.
	 * Available algorithms are listed in the mrc_ctl_attr.cc.cc_algorithms
	 * structure. A NULL pointer indicates no CC algorithm is active for this
	 * profile.
	 *
	 * The following common strings are defined:
	 *   "mrc-smtrk-1.00" - MRCv1 SmaRTTrack 1.00
	 */
	const char *cc_algorithm;

	/*
	 * Configuration structure to use for this profile.  The structure is
	 * algorithm specific.
	 *
	 * The following common structures are defined:
	 *   mrc_ctl_cc_smtrk_cfg - SmaRTTrack config. structure
	 */
	const void *cc_config;
};

/* SmaRTTrack configuration structure. */
struct mrc_ctl_cc_smtrk_cfg {
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

/**
 * @brief Create or modify a CC profile
 *
 * Used to configure a CC profile. Once configured, the specified
 * cc_profile_id can be used by any to be created QP. Fields may
 * not be modified while any active QPs are associated with the profile.
 *
 * NOTE: The cc_profile_id is defined by the calling controller application.
 *
 * @param mrc_ctx[in]    - MRC context
 * @param cc_profile[in] - Profile to create/update
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval ENOMEM Unable to create a new profile (max reached).
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_modify_cc_profile(struct mrc_context *mrc_ctx,
	struct mrc_ctl_cc_profile *cc_profile);

/**
* @brief Get a CC profile.
*
* Get a CC profile configuration.
*
* @param mrc_ctx[in]             - MRC context
* @param cc_profile_id[in]       - Profile to get
* @param mrc_ctl_cc_profile[out] - Profile's configuration
*
* @return 0 on success.
* @retval EINVAL One or more supplied arguments are invalid.
* @retval EPERM Process lacks sufficient permissions.
*/
int mrc_ctl_query_cc_profile(struct mrc_context *mrc_ctx,
   uint64_t cc_profile_id,
   struct mrc_ctl_cc_profile *cc_profile);

/**
* @brief Destroy ac CC profile
*
* Destroy an CC profile. All QPs that are using this CC profile must be
* destroyed before the profile can be destroyed.
*
* @param mrc_ctx[in]       - MRC context
* @param ev_profile_id[in] - Profile to destroy
*
* @return 0 on success.
* @retval EINVAL One or more supplied arguments are invalid.
* @retval EBUSY Profile is still being used by a QP.
* @retval EPERM Process lacks sufficient permissions.
*/
int mrc_ctl_destroy_cc_profile(struct mrc_context *mrc_ctx,
	 uint64_t cc_profile_id);


/**
 * @brief Get an EV's state
 *
 * This can be performed immediately after an EV profile using explicit EVs
 * has been created. For generated EVs, this can be performed after the first
 * QP associated with the EV profile has reached the RTR state.
 *
 * @param mrc_ctx[in]       - MRC context
 * @param ev_profile_id[in] - EV profile
 * @param ev[in/out]        - EV value [in], state [out]
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_get_ev(struct mrc_context *mrc_ctx,
		   uint64_t ev_profile_id,
		   struct mrc_ctl_ev_entry *ev);

/**
 * @brief Update the state of an EV
 *
 * The valid values of the state are MRC_CTL_EV_GOOD and MRC_CTL_EV_DENIED.
 * If the device does not advertise the MRC_CTL_OPT_CAP_EV_UPDATE_RTS
 * capability, an EV update can only be performed BEFORE any QPs associated
 * with the profile have been modified using mrc_modify_qp().
 *
 * An EV's state can be set for both explicit and generated EVs.
 *
 * @param mrc_ctx[in]       - MRC context
 * @param ev_profile_id[in] - EV profile
 * @param ev[in]            - EV value and port to update
 * @param ev_state[in]      - State to set
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_update_ev(struct mrc_context *mrc_ctx,
		      uint64_t ev_profile_id,
		      struct mrc_ctl_ev *ev,
		      enum mrc_ctl_ev_state ev_state);

/**
 * @brief Replace the value of an EV in an explicit EV array
 *
 * If the device does not advertise the MRC_CTL_OPT_CAP_EV_UPDATE_RTS
 * capability, an EV update can only be performed BEFORE any QPs associated
 * with the profile have been modified using mrc_modify_qp().
 *
 * An EV's value can only be set for explicit EVs.
 *
 * @param mrc_ctx[in]       - MRC context
 * @param ev_profile_id[in] - EV profile
 * @param ev_old[in]        - Old EV value and port to replace
 * @param ev_new[in]        - New EV value and port
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_replace_ev(struct mrc_context *mrc_ctx,
		       uint64_t ev_profile_id,
		       struct mrc_ctl_ev *ev_old,
		       struct mrc_ctl_ev *ev_new);

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
 * @brief EV Event structure
 *
 * EV Event structure. Hardware generates an EV Event for every EV state
 * change that matches monitored EV states in the EV profile's event
 * mask field.
 */
struct mrc_ctl_ev_event {
	uint64_t ev_profile_id;
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
	mrc_ctl_ev_type_t rsp_ev;
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
