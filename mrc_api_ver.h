#ifndef _MRC_API_VER_H_
#define _MRC_API_VER_H_

#define MRC_API_VER_MAJOR_OFFSET   24
#define MRC_API_VER_MINOR_OFFSET   16

#define MRC_API_VER(_major, _minor, _sub_minor) \
	(((_major) << MRC_API_VER_MAJOR_OFFSET) | \
	 ((_minor) << MRC_API_VER_MINOR_OFFSET) | \
	 (_sub_minor))

/* This is a special value used by the application
 * and the library to know that the latest API version
 * was used. This helps by not having to track every
 * version number if the most common use case is to
 * recompile the application to the latest API */

#define MRC_API_VER_LATEST MRC_API_VER(0xFF, 0xFF, 0xFFFF)

#endif /* _MRC_API_VER_H_ */
