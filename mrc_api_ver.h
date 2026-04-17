/*
 * SPDX-FileCopyrightText: Copyright (c) 2024, 2025, 2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 *
 * Copyright (c) 2024, 2025, 2026, Broadcom. All rights reserved. The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Copyright (c) 2024, 2025, 2026, Advanced Micro Devices (AMD), Inc.  All rights
 * reserved.
 */

#ifndef _MRC_API_VER_H_
#define _MRC_API_VER_H_

/*
 * Versioning follows semantic versioning (MAJOR.MINOR.SUBMINOR):
 *
 *   MAJOR    - Incremented on incompatible API/ABI changes. Upgrading across
 *              major versions will likely break existing code and requires
 *              manual migration.
 *
 *   MINOR    - Incremented when backward-compatible functionality is added.
 *              Safe to update; existing code continues to work without
 *              modification.
 *
 *   SUBMINOR - Incremented for backward-compatible bug fixes, security patches,
 *              and internal-only changes. Safest to update; no behavioral
 *              changes to the public API.
 */

/* Version field bit offsets */
#define MRC_API_VER_MAJOR_OFFSET        24
#define MRC_API_VER_MINOR_OFFSET        16
#define MRC_API_VER_SUBMINOR_OFFSET     0

/* Version encoding macro */
#define MRC_API_VER(_major, _minor, _sub_minor) \
	(((_major) << MRC_API_VER_MAJOR_OFFSET) | \
	 ((_minor) << MRC_API_VER_MINOR_OFFSET) | \
	 ((_sub_minor)) << MRC_API_VER_SUBMINOR_OFFSET)

/* Special version value for applications that want to use the latest API
 * version without tracking specific version numbers */

#define MRC_API_VER_LATEST              MRC_API_VER(0xFF, 0xFF, 0xFFFF)

/* API versions */
#define MRC_API_CURRENT_VERSION         MRC_API_VER(1, 0, 1)
#define MRC_API_LAST_SUPPORTED_VERSION  MRC_API_VER(0, 0, 0)

/* Version selection macros */
#ifndef MRC_API_VER_USED
    #define MRC_API_VER_USED MRC_API_CURRENT_VERSION
#elif MRC_API_VER_USED == MRC_API_VER_LATEST
    #undef MRC_API_VER_USED
    #define MRC_API_VER_USED MRC_API_CURRENT_VERSION
#endif

/* Version validation */
#if MRC_API_VER_USED > MRC_API_CURRENT_VERSION
	#error "MRC_API_VER_USED is greater than MRC_API_CURRENT_VERSION"
#elif MRC_API_VER_USED < MRC_API_LAST_SUPPORTED_VERSION
    #error "MRC_API_VER_USED is less than MRC_API_LAST_SUPPORTED version"
#elif MRC_API_VER_USED == MRC_API_LAST_SUPPORTED_VERSION
    #warning "MRC_API_VER_USED is equal to MRC_API_LAST_SUPPORTED \
version, may become obsolete"
#endif

/*
 * Vendor-specific API versioning
 *
 * These macros track a vendor-defined API surface that evolves independently
 * of the core MRC API.  The same MAJOR.MINOR.SUBMINOR semantics apply.
 */

/* Vendor API versions */
#define MRC_VENDOR_API_CURRENT_VERSION         MRC_API_VER(1, 0, 0)
#define MRC_VENDOR_API_LAST_SUPPORTED_VERSION  MRC_API_VER(0, 0, 0)

/* Vendor API version selection macros */
#ifndef MRC_VENDOR_API_VER_USED
    #define MRC_VENDOR_API_VER_USED MRC_VENDOR_API_CURRENT_VERSION
#elif MRC_VENDOR_API_VER_USED == MRC_API_VER_LATEST
    #undef MRC_VENDOR_API_VER_USED
    #define MRC_VENDOR_API_VER_USED MRC_VENDOR_API_CURRENT_VERSION
#endif

/* Vendor API version validation */
#if MRC_VENDOR_API_VER_USED > MRC_VENDOR_API_CURRENT_VERSION
    #error "MRC_VENDOR_API_VER_USED is greater than MRC_VENDOR_API_CURRENT_VERSION"
#elif MRC_VENDOR_API_VER_USED < MRC_VENDOR_API_LAST_SUPPORTED_VERSION
    #error "MRC_VENDOR_API_VER_USED is less than MRC_VENDOR_API_LAST_SUPPORTED_VERSION"
#elif MRC_VENDOR_API_VER_USED == MRC_VENDOR_API_LAST_SUPPORTED_VERSION
    #warning "MRC_VENDOR_API_VER_USED is equal to MRC_VENDOR_API_LAST_SUPPORTED_VERSION, \
may become obsolete"
#endif

#endif /* _MRC_API_VER_H_ */
