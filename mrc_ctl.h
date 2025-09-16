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

/* MRC Controller API.
 * -------------------
 * This API is used by the network controller for configuration, management
 * and monitoring of MRC QPs.
 *
 * Controller processes obtain an mrc_context via the mrc_open_context()
 * function.
 *
 * Controller processes MUST possess CAP_NET_ADMIN capability privileges.
 */

#ifndef _MRC_CTL_API_H_
#define _MRC_CTL_API_H_

#include <stdint.h>
#include <infiniband/verbs.h>

#include <mrc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Unpopulated (unset) EV entry definition.
 */
#define MRC_CTL_EV_UNPOPULATED (struct mrc_ctl_ev){ .val = 0, .port = 0 }

/**
 * @brief Maximum number of bytes in an EV value
 */
#define MRC_CTL_EV_MAX_BYTES 32

/**
 * @brief Version for the MRC control APIs
 */
enum mrc_ctl_version {
	MRC_CTL_VERSION_0	= 0, /* MRC not supported */
	MRC_CTL_VERSION_1	= (1 << 0),
};

/**
 * @brief Profile state value used for all MRC control profiles
 */
enum mrc_ctl_profile_state {
	MRC_CTL_PROFILE_INIT,    /* Initialized and ready for config. */
	MRC_CTL_PROFILE_OFFLINE, /* Configured but not usable. */
	MRC_CTL_PROFILE_ONLINE,  /* Is usable. */
};

/*****************************************************************************
 * Device Query
 *****************************************************************************/

/**
 * @brief Optional control features supported by the implementation
 */
enum mrc_ctl_attr_opt {
	/* Device supports modifying ONLINE EV profiles */
	MRC_CTL_OPT_CAP_EV_PROFILE_MODIFY_ONLINE	= (1<<0),
	/* Device supports modifying ONLINE CC profiles */
	MRC_CTL_OPT_CAP_CC_PROFILE_MODIFY_ONLINE	= (1<<1),
	/* The implementation supports EV Events */
	MRC_CTL_OPT_CAP_EV_EVENT			= (1<<2),
	/*
	 * Only contiguous ranges supported in explicit mode. First EV value
	 * is the base; last is 'base_ev_val + (ev_count - 1)'
	 */
	MRC_CTL_OPT_CAP_EV_EXPLICIT_RANGE		= (1<<3),
	/* The implementation supports EV Probes. */
	MRC_CTL_OPT_CAP_EV_PROBE			= (1<<4),
	/* The implementation supports precise EV Event drop counts. */
	MRC_CTL_OPT_CAP_EV_EVENT_PRECISE_DROP_CNT	= (1<<5),
	/* A single segment SRH is supported with SRv6 */
	MRC_CTL_OPT_CAP_SRV6_SRH			= (1<<6),
};

/**
 * @brief Control feature values supported by the implementation
 */
struct mrc_ctl_attr {
	/* bitmap of all versions supported (see enum mrc_ctl_version) */
	uint32_t mrc_ctl_version;

	/* EV attributes */
	struct {
		/*
		 * Maximum number of EV Format profiles supported by the
		 * device.
		 */
		uint32_t ev_max_fmt_profiles;

		/* Maximum number of EV profiles supported by the device. */
		uint32_t ev_max_profiles;

		/* Maximum number of EVs available across all profiles. */
		uint32_t ev_max_count;

		/* Free number of EV resources avilable across all profiles. */
		uint32_t ev_free_count;

		/*
		 * Maximum number of EVs supported per profile. If the
		 * controller is supplying an explicit EV array, then that
		 * array can contain at most this many EVs.
		 */
		uint32_t ev_max_count_profile;

		/*
		 * Alignment requirements for the number of EVs that are
		 * required in an explicit EV array. The alignment value
		 * implies the minimum count required and it provides the
		 * array sizing requirements. The array size should be:
		 *   (ev_count_align + (k * ev_count_align))
		 * where 'k' is a multiple chosen by the application. For
		 * example, if a provider supports EVs in multiples of 8, it
		 * would set the values 'ev_count_align = 8'. The total number
		 * of EVs is subject to a maximum of ev_max_count_profile.
		 * Value of 0 means any EV count increment is supported.
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

		/*
		 * Bitmask indicating which EV modes are supported by the
		 * device. Each bit corresponds to mode defined in the
		 * mrc_ctl_ev_mode enum.
		 */
		uint32_t ev_mode_mask;
	} ev;

	/* CC attributes */
	struct {
		/* Maximum number of CC profiles supported by the device. */
		uint32_t cc_max_profiles;

		/*
		 * Array of CC algorithm strings.
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
 * EV Format Profile
 *****************************************************************************/

/**
 * @brief Supported EV Format modes
 *
 * The FLOW based EV modes place the EV value in the IPv6 flow label and UDP
 * source port.
 *
 * The SRv6 based EV modes use an IPv6 header encap with an optional single
 * segment SRH header. The lower 8b of the UDP source port is used to holds
 * the EV index of the SRv6 EV entry that was used in the packet. EV index
 * values are equivalent to the EV array indices of the array returned by a
 * MRC_CTL_EV_OP_QUERY_EV_ARRAY operation on an EV profile.
 */
enum mrc_ctl_ev_mode {
	/* Controller will not provide any EVs (vendor managed e.g., ECMP) */
	MRC_CTL_EV_MODE_AUTO			= 1 << 0,
	/* Explicit EVs */
	MRC_CTL_EV_MODE_FLOW_EXPLICIT		= 1 << 1,
	/* Generated EVs */
	MRC_CTL_EV_MODE_FLOW_GENERATED		= 1 << 2,
	/* Compressed Explicit EVs */
	MRC_CTL_EV_MODE_FLOW_COMP_EXPLICIT	= 1 << 3,
	/* Compressed Generated EVs */
	MRC_CTL_EV_MODE_FLOW_COMP_GENERATED	= 1 << 4,
	/* Explicit SRv6 */
	MRC_CTL_EV_MODE_SRV6_EXPLICIT		= 1 << 5,
	/* Geneerated SRv6 */
	MRC_CTL_EV_MODE_SRV6_GENERATED		= 1 << 6,
	/* Compressed explicit SRv6 */
	MRC_CTL_EV_MODE_SRV6_COMP_EXPLICIT	= 1 << 7,
	/* Compressed generated SRv6 */
	MRC_CTL_EV_MODE_SRV6_COMP_GENERATED	= 1 << 8,
};

/**
 * @brief Supported EV Format operations
 */
enum mrc_ctl_ev_fmt_op {
	MRC_CTL_EV_FMT_OP_MODIFY_FIELDS,
	MRC_CTL_EV_FMT_OP_QUERY_FIELDS,
};

/**
 * @brief EV Format profile attribute mask
 */
enum mrc_ctl_ev_fmt_profile_attr_mask {
	MRC_CTL_EV_FMT_PROFILE_STATE		= 1 << 0,
	MRC_CTL_EV_FMT_PROFILE_CUR_STATE	= 1 << 1,
	MRC_CTL_EV_FMT_PROFILE_MODE		= 1 << 2,
	MRC_CTL_EV_FMT_PROFILE_SRV6_SRH		= 1 << 3,
	MRC_CTL_EV_FMT_PROFILE_VENDOR_MD	= 1 << 4,
	MRC_CTL_EV_FMT_PROFILE_OP		= 1 << 5,
};

/**
 * @brief EV Format field width structures
 */
struct mrc_ctl_ev_fmt_field {
	uint8_t width;		/* Field width in bits */
	uint8_t min_val;	/* Minimum supported value */
	uint8_t max_val;	/* Maximam supported value */
};

/**
 * @brief EV Format profile attributes
 *
 * EVs are abstract representations of network paths. The EV Format profile
 * defines the EV format paramaters used for explicit or generated EVs. It
 * provides guidance on the structure of the EV and how it's interpreted. This
 * profile is referenced by individual EV profiles and every EV profile
 * assigned to the same EV Format profile will be using the same EV formats.
 *
 * The fixed_field_width is used to reserve an initial fixed number of bits
 * in the EV value. The value placed in these fixed bits will likely be the
 * same across all EVs in a profile referencing this EV Format profile. For
 * example, with SRv6 the fixed_field_width can be set to 32b to hold the
 * locator value. Vendor implemenations may or may not impose restrictions on
 * the fixed value having to be the same across all EVs in a profile.
 */
struct mrc_ctl_ev_fmt_profile_attr {
	/* Move the profile to this state. */
	enum mrc_ctl_profile_state profile_state;
	/* Current profile state. */
	enum mrc_ctl_profile_state cur_profile_state;

	/* The EV mode for this format profile. */
	enum mrc_ctl_ev_mode ev_mode;

	/*
	 * If non-zero, a single segment SRH is also included with each SRv6
	 * EV. This field only applies to SRv6 based EV modes.
	 */
	uint8_t srv6_use_srh;

	/*
	 * For compressed EV profiles, this field is vendor defined and
	 * contains information relevant to the vendor's compreession scheme.
	 * This field only applies to compressed EV modes.
	 */
	uint8_t vendor_metadata[MRC_CTL_EV_MAX_BYTES];

	struct {
		enum mrc_ctl_ev_fmt_op op;

		union {
			struct {
				/* Array of format field */
				struct mrc_ctl_ev_fmt_field *fmt_fields;
				/* Format field array length */
				int fmt_field_count;
				/* Width for a fixed EV prefix */
				int fixed_field_width;
			} modify_fmt_fields;

			struct {
				/* Array of empty format fields */
				struct mrc_ctl_ev_fmt_field *fmt_fields;
				/* Format field array length */
				int fmt_field_count;
				/* Configured number of format fields in use */
				int *cur_fmt_field_count;
				/* Configured width for a fixed EV prefix */
				int *fixed_field_width;
			} query_fmt_fields;
		};
	} ev_fmt_op;
};

/**
 * @brief Modify an EV Format profile
 *
 * EV Format profile state machine:
 *   INIT -> OFFLINE -> ONLINE -> OFFLINE -> INIT
 *
 * States:
 *   INIT:    Profile created; not yet configured.
 *   OFFLINE: Configured but inactive; can be modified.
 *   ONLINE:  Active and usable; only limited modifications allowed.
 *
 * State transition requirements:
 *   To OFFLINE: MODE
 *   To ONLINE: MODIFY_FIELDS (for generated modes)
 *
 * Allowed:
 *   OFFLINE state:
 *     - Modify: STATE(ONLINE/INIT), MODE, SRV6_SRH, VENDOR_MD,
 *               OP_MODIFY_FIELDS, OP_QUERY_FIELDS
 *     - Query: STATE, MODE, SRV6_SRH, VENDOR_MD, OP_QUERY_FIELDS
 *   ONLINE state:
 *     - Modify: STATE(OFFLINE), OP_QUERY_FIELDS
 *     - Query: STATE, MODE, SRV6_SRH, VENDOR_MD, OP_QUERY_FIELDS
 *
 * The following restrictions apply for the generated EV modes:
 *   MRC_CTL_EV_MODE_GENERATED
 *     - The sum of the fixed_field_width and the widths for each of the
 *       format fields must NOT exceeed 32b.
 *   MRC_CTL_EV_MODE_SRV6_GENERATED
 *     - If SRH is NOT enabled, the sum of the fixed_field_width and the
 *       widths for each of the format fields must not exceeed 128b.
 *     - If SRH is enabled, the sum of the (fixed_field_width * 2) and the
 *       widths for each of the format fields must not exceed 256b.
 *   MRC_CTL_EV_MODE_SRV6_COMP_GENERATED
 *     - Vendor defined; refer to vendor documentation.
 *
 * @param mrc_ctx[in]           - MRC context
 * @param ev_fmt_profile_id[in] - EV Format Profile ID
 * @param attr[in]              - EV Profile attribute structure
 * @param attr_mask[in]         - Bitmask of EV Profile attribute mask
 *
 * @return 0 on success.
 * @retval -EINVAL One or more supplied arguments are invalid.
 * @retval -EBUSY One or more associated EV profiles are in the ONLINE state.
 * @retval -E2BIG Supplied combination of format fields is unsupportable.
 * @retval -EIO Implementation specific error occurred.
 * @retval -EPERM Process lacks sufficient permissions.
 * @retval -EBUSY One or more active QPs are associated with this profile.
 */
int mrc_ctl_modify_ev_fmt_profile(struct mrc_context *mrc_ctx,
				  uint64_t ev_fmt_profile_id,
				  struct mrc_ctl_ev_fmt_profile_attr *attr,
				  int attr_mask);

/**
 * @brief Query an EV Format profile
 *
 * If MRC_CTL_EV_FMT_OP_QUERY_FIELDS is specified, an array of empty format
 * fields (fmt_fieds) must be supplied to be filled in upon return. If the
 * number of entries in the array (fmt_field_count) is not large enough then
 * an -E2BIG error is returned and the required array size is set in
 * cur_fmt_field_count.
 *
 * The MRC_CTL_EV_FMT_OP_MODIFY_FIELDS operation is not allowed.
 *
 * @param mrc_ctx[in]       - MRC context
 * @param ev_profile_id[in] - EV Profile ID
 * @param attr[out]         - EV Profile attribute structure
 * @param attr_mask[in]     - Bitmask of EV Profile attribute mask
 *
 * @return 0 on success.
 * @retval -EINVAL One or more supplied arguments are invalid.
 * @return -E2BIG Format field count is less than the total number of fields.
 * @retval -EIO Implementation specific error occurred.
 * @retval -EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_query_ev_fmt_profile(struct mrc_context *mrc_ctx,
				 uint64_t ev_fmt_profile_id,
				 struct mrc_ctl_ev_fmt_profile_attr *attr,
				 int attr_mask);

/*****************************************************************************
 * EV Structures
 *****************************************************************************/

/**
 * @brief EV value
 *
 * For SRv6 EV types, the first 128b/16B holds the SRv6 address and the second
 * 128b/16B holds the single segment SRH address.
 *
 * For non-SRv6 EV types, the first 32b/4B holds the value.
 */
typedef uint8_t mrc_ctl_ev_t[MRC_CTL_EV_MAX_BYTES];

/**
 * @brief EV entry (value and port)
 */
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

/*****************************************************************************
 * EV Profile
 *****************************************************************************/

/**
 * @brief Supported EV operations
 */
enum mrc_ctl_ev_op {
	MRC_CTL_EV_OP_REPLACE_EV,
	MRC_CTL_EV_OP_MODIFY_EV_STATE,
	MRC_CTL_EV_OP_QUERY_EV_STATE,
	MRC_CTL_EV_OP_QUERY_EV_ARRAY,
};

/**
 * @brief EV profile attribute mask
 */
enum mrc_ctl_ev_profile_attr_mask {
	MRC_CTL_EV_PROFILE_STATE	= 1 << 0,
	MRC_CTL_EV_PROFILE_CUR_STATE	= 1 << 1,
	MRC_CTL_EV_PROFILE_FMT_ID	= 1 << 2,
	MRC_CTL_EV_PROFILE_COUNT	= 1 << 3,
	MRC_CTL_EV_PROFILE_MIN_ACTIVE	= 1 << 4,
	MRC_CTL_EV_PROFILE_EVENT_MASK	= 1 << 5,
	MRC_CTL_EV_PROFILE_EV_OP	= 1 << 6,
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
	 * The EV Format profile to use that specifies EV paramaters.
	 * - AUTO: Vendor-defined mode
	 * - EXPLICIT: Caller provides explicit EV values
	 * - GENERATED: Hardware generated within EV bounds
	 * - COMP_EXPLICIT: Caller provides compressed explicit EV values
	 * - COMP_GENERATED: Hardware generated compressed within EV bounds
	 */
	uint64_t ev_fmt_profile_id;

	/*
	 * Number of EVs in the profile's EV array.
	 *  - For explicit and generated mode: caller sets the array size,
	 *    subject to system configuration and device alignment and maximum
	 *    limits.
	 *  - When queried: returns the implementation-adjusted,
	 *    alignment-compliant EV count.
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

	struct {
		enum mrc_ctl_ev_op op;

		union {
			struct {
				/* Current EV; Match occurs on val and port */
				struct mrc_ctl_ev cur_ev;
				/* New EV (formatted according to EV fields) */
				struct mrc_ctl_ev new_ev;
				/*
				 * If zero only one instance is replaced, else
				 * allow matches are replaced. Entry selected
				 * is implemenation specific.
				 */
				int all_copies;
			} replace_ev;

			struct {
				/* EV to modify */
				struct mrc_ctl_ev ev;
				/* EV's new state */
				enum mrc_ctl_ev_state state;
			} modify_ev_state;

			struct {
				/* EV to query */
				struct mrc_ctl_ev ev;
				/* EV's current state; output-only */
				enum mrc_ctl_ev_state state;
			} query_ev_state;

			struct {
				/* Array of EVs; pointer, length >= ev_count */
				struct mrc_ctl_ev *ev;
			} query_ev_array;
		};
	} ev_op;
};

/**
 * @brief Modify an EV profile
 *
 * EV profile state machine:
 *   INIT -> OFFLINE -> ONLINE -> OFFLINE -> INIT
 *
 * States:
 *   INIT:    Profile created; not yet configured.
 *   OFFLINE: Configured but inactive; can be modified.
 *   ONLINE:  Active and usable; only limited modifications allowed.
 *
 * State transition requirements:
 *   To OFFLINE:
 *     - FMT_ID, COUNT
 *   To ONLINE:
 *     - MIN_ACTIVE, EVENT_MASK, REPLACE_EV (for explicit modes)
 *
 * Allowed:
 *   OFFLINE state:
 *     - Modify: STATE(ONLINE/INIT), FMT_ID, COUNT, MIN_ACTIVE, EVENT_MASK
 *     - Query: STATE, FMT_ID, COUNT, MIN_ACTIVE, EVENT_MASK
 *     - EV_OP: REPLACE_EV, MODIFY_EV_STATE, QUERY_EV_STATE, QUERY_EV_ARRAY
 *   ONLINE state:
 *     - Modify: STATE(OFFLINE)
 *     - Query: STATE, FMT_ID, COUNT, MIN_ACTIVE, EVENT_MASK
 *     - EV_OP: MODIFY_EV_STATE, QUERY_EV_STATE, QUERY_EV_ARRAY
 *       If EV_PROFILE_MODIFY_ONLINE supported: EVENT_MASK, REPLACE_EV
 *
 * Restrictions:
 *   On INIT -> OFFLINE, Explicit array EVs are all EV_UNPOPULATED; MUST be
 *   replaced before moving to ONLINE, implementation/device constraints may
 *   apply; refer to vendor documentation.
 *
 * @param mrc_ctx[in]       - MRC context
 * @param ev_profile_id[in] - EV Profile ID
 * @param attr[in]          - EV Profile attribute structure
 * @param attr_mask[in]     - Bitmask of EV Profile attribute mask
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval ENOENT EV not found.
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
 * @param mrc_ctx[in]       - MRC context
 * @param ev_profile_id[in] - EV Profile ID
 * @param attr[out]         - EV Profile attribute structure
 * @param attr_mask[in]     - Bitmask of EV Profile attribute mask
 *
 * @return 0 on success.
 * @retval EINVAL One or more supplied arguments are invalid.
 * @retval ENOENT EV not found.
 * @retval EIO Implementation specific error occurred.
 * @retval EPERM Process lacks sufficient permissions.
 */
int mrc_ctl_query_ev_profile(struct mrc_context *mrc_ctx,
			     uint64_t ev_profile_id,
			     struct mrc_ctl_ev_profile_attr *attr,
			     int attr_mask);

/*****************************************************************************
 * CC Profile
 *****************************************************************************/

/**
 * @brief CC profile attribute mask
 */
enum mrc_ctl_cc_profile_attr_mask {
	MRC_CTL_CC_PROFILE_STATE	= 1 << 0,
	MRC_CTL_CC_PROFILE_CUR_STATE	= 1 << 1,
	MRC_CTL_CC_PROFILE_ALGORITHM	= 1 << 2,
	MRC_CTL_CC_PROFILE_CONFIG	= 1 << 3,
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

/**
 * @brief Modify a CC profile
 *
 * CC profile state machine:
 *   INIT -> OFFLINE -> ONLINE -> OFFLINE -> INIT
 *
 * States:
 *   INIT:    Profile created; not yet configured.
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
 * @param mrc_ctx[in]       - MRC context
 * @param cc_profile_id[in] - CC Profile ID
 * @param attr[in]          - CC Profile attribute structure
 * @param attr_mask[in]     - Bitmask of CC Profile attribute mask
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
 * @param mrc_ctx[in]       - MRC context
 * @param cc_profile_id[in] - CC Profile ID
 * @param attr[out]         - CC Profile attribute structure
 * @param attr_mask[in]     - Bitmask of CC Profile attribute mask
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
 * @brief SRv6 Probe paramaters
 *
 * If SRv6 is enabled for a probe, both the req_ev and rsp_ev fields must
 * contain the full locator and uSID stack to use. If SRH is also enabled,
 * the SRH segment must also include the full locator and uSID stack. No
 * compression is supported for probes.
 */
struct mrc_ctl_srv6_probe {
	uint8_t srv6_enable;
	uint8_t srv6_use_srh;
};

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
	/* SRv6 probe parameters. */
	struct mrc_ctl_srv6_probe srv6;
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
