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
 * Copyright (c) 2024, 2025, Broadcom. All rights reserved. The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Copyright (c) 2024, 2025, Advanced Micro Devices (AMD), Inc.  All rights
 * reserved.
 */

#ifndef _MRC_CTL_API_VER_H_
#define _MRC_CTL_API_VER_H_

#define MRC_CTL_API_VER_MAJOR_OFFSET   24
#define MRC_CTL_API_VER_MINOR_OFFSET   16

#define MRC_CTL_API_VER(_major, _minor, _sub_minor) \
	(((_major) << MRC_CTL_API_VER_MAJOR_OFFSET) | \
	 ((_minor) << MRC_CTL_API_VER_MINOR_OFFSET) | \
	 (_sub_minor))

/* This is a special value used by the application
 * and the library to know that the latest API version
 * was used. This helps by not having to track every
 * version number if the most common use case is to
 * recompile the application to the latest API */

#define MRC_CTL_API_VER_LATEST MRC_CTL_API_VER(0xFF, 0xFF, 0xFFFF)

#endif /* _MRC_CTL_API_VER_H_ */
