/*
 * Copyright (c) 2024, 2025, Broadcom. All rights reserved. The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 */

/*
 * This simple application demonstrates EV expansions various EV Format
 * profile and EV profile configurations.
 *
 * Build: gcc ev_expansion.c -o ev_expansion
 *   Run: ./ev_expansion
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>

#define MAX_EVS 8

/****************************************************************************/
/* EV (32b EV, SRv6, SRv6+SRH)                                              */
/****************************************************************************/

#define MAX_EV_LEN 32
typedef uint8_t ev_t[MAX_EV_LEN];

/****************************************************************************/
/* EV Format Profile (wire expansion)                                       */
/****************************************************************************/

enum ev_fmt_type { EV_FMT_TYPE_STEV, EV_FMT_TYPE_SRV6 };
const char *ev_fmt_type_str[] = { "STEV", "SRv6" };

struct ev_fmt_field {
	uint32_t width;
};

struct ev_fmt_profile {
	enum ev_fmt_type     type;
	struct ev_fmt_field *fmt_fields;
	uint32_t             num_fmt_fields;
};

/****************************************************************************/
/* EV Profile                                                               */
/****************************************************************************/

enum ev_type { EV_TYPE_EXP, EV_TYPE_GEN };
const char *ev_type_str[] = { "Explicit", "Generated" };

struct ev_field {
	uint32_t width;
	uint32_t init_val;
	uint32_t min_val;
	uint32_t max_val;
	uint32_t mask;
};

struct ev_profile {
	enum ev_type     type;
	struct ev_field *fields;
	uint32_t         num_fields;
	ev_t            *evs;
	uint32_t         num_evs;
};

/****************************************************************************/
/* Globals and Macros                                                       */
/****************************************************************************/

struct ev_fmt_profile _ev_fmt_profile;
struct ev_profile _ev_profile;

#define CLR_PROFS()                                                   \
	do {                                                          \
		memset(&_ev_fmt_profile, 0, sizeof(_ev_fmt_profile)); \
		memset(&_ev_profile, 0, sizeof(_ev_profile));         \
	} while (0)

#define ALLOC(var, cnt)                                \
	do {                                           \
		(var) = calloc((cnt), sizeof(*(var))); \
		assert(var);                           \
	} while (0)

#define EV_PROF_INIT(fmt_fields_cnt, fields_cnt, evs_cnt)            \
	do {                                                         \
		CLR_PROFS();                                         \
		ALLOC(_ev_fmt_profile.fmt_fields, (fmt_fields_cnt)); \
		_ev_fmt_profile.num_fmt_fields = (fmt_fields_cnt);   \
		ALLOC(_ev_profile.fields, (fields_cnt));             \
		_ev_profile.num_fields = (fields_cnt);               \
		ALLOC(_ev_profile.evs, (evs_cnt));                   \
		_ev_profile.num_evs = (evs_cnt);                     \
	} while (0)

#define EV_PROF_FREE()                            \
	do {                                      \
		free(_ev_fmt_profile.fmt_fields); \
		free(_ev_profile.fields);         \
		free(_ev_profile.evs);            \
		CLR_PROFS();                      \
	} while (0)

#define RAND(min, max) ((random() % ((max) - (min) + 1)) + (min))

/****************************************************************************/
/* EV Expansion and Debug Output                                            */
/****************************************************************************/

/* count the number of bits set in a mask */
uint32_t count_mask_bits(uint32_t mask)
{
	uint32_t count = 0;

	while (mask) {
		if (mask & 1)
			count++;
		mask >>= 1;
	}

	return count;
}

/*
 * Profile configuration ERROR if:
 *  - num_fields > num_fmt_fields
 *  - sum of field widths > sum of fmt_field widths
 *  - a field's init_val is wider than the field's width
 *  - a field's mask contains bits set outside of the field width
 *  - a field's max_val is wider than the field's mask
 */
void check_profs(void)
{
	struct ev_field *field;
	uint32_t fmt_total_width = 0;
	uint32_t field_total_width = 0;
	uint32_t max_val_for_width;
	uint32_t max_val_for_mask;
	uint32_t mask_bit_cnt;
	int i;

	/* check if num_fields > num_fmt_fields */
	assert(_ev_profile.num_fields <= _ev_fmt_profile.num_fmt_fields);

	/* check if sum of field widths > sum of fmt_field widths */
	for (i = 0; i < _ev_fmt_profile.num_fmt_fields; i++)
		fmt_total_width += _ev_fmt_profile.fmt_fields[i].width;
	for (i = 0; i < _ev_profile.num_fields; i++)
		field_total_width += _ev_profile.fields[i].width;
	assert(field_total_width <= fmt_total_width);

	/* check each field for invalid config */
	for (i = 0; i < _ev_profile.num_fields; i++) {
		field = &_ev_profile.fields[i];

		/* calculate maximum value that can fit in this field width */
		assert(field->width <= 32);
		max_val_for_width = (field->width == 32)
					? 0xffffffff
					: ((1U << field->width) - 1);

		/* check if init_val is wider than field width */
		assert(field->init_val <= max_val_for_width);

		/* check if mask contains bits set outside of field width */
		assert(!(field->mask & ~max_val_for_width));

		if (field->mask == 0)
			continue;

		/* number of bits set in mask (bits may not be consecutive) */
		mask_bit_cnt = count_mask_bits(field->mask);

		/* calculate max value possible with mask bits */
		max_val_for_mask = (mask_bit_cnt == 32)
					? 0xffffffff
					: ((1U << mask_bit_cnt) - 1);

		/* check if max_val is wider than set mask bits */
		assert(field->max_val <= max_val_for_mask);
	}
}

/* dump the entire contents of an ev */
void dump_ev(ev_t *ev)
{
	printf("0x");
	for (int i = 0; i < MAX_EV_LEN; i++)
		printf("%02x", ((uint8_t *)ev)[i]);
	printf("\n");
}

/* dump both the EV format profile and EV profile configurations */
void dump_profs(void)
{
	int i;

	printf("\nev_fmt_prof:\n");
	printf("  type: %s\n", ev_fmt_type_str[_ev_fmt_profile.type]);
	printf("  fmt_fields: %d\n", _ev_fmt_profile.num_fmt_fields);
	for (i = 0; i < _ev_fmt_profile.num_fmt_fields; i++) {
		printf("    %02d: width=%d\n",
		       i, _ev_fmt_profile.fmt_fields[i].width);
	}

	printf("\nev_prof:\n");
	printf("  type: %s\n", ev_type_str[_ev_profile.type]);
	printf("  fields: %d\n", _ev_profile.num_fields);
	for (i = 0; i < _ev_profile.num_fields; i++) {
		printf("    %02d: width=%d, init_val=0x%x, "
		       "min=0x%x max=0x%x mask=0x%x\n", i,
		       _ev_profile.fields[i].width,
		       _ev_profile.fields[i].init_val,
		       _ev_profile.fields[i].min_val,
		       _ev_profile.fields[i].max_val,
		       _ev_profile.fields[i].mask);
	}

	printf("  evs: %d\n", _ev_profile.num_evs);
	for (i = 0; i < _ev_profile.num_evs; i++) {
		printf("    %02d: ", i);
		dump_ev(&_ev_profile.evs[i]);
	}
}

/* extract and return num_bits starting at bit_offset from ev_data */
uint32_t ev_extract_bits(uint8_t *ev_data,
			 uint32_t bit_offset,
			 uint32_t num_bits)
{
	uint32_t value = 0;
	uint32_t byte_idx;
	uint32_t bit_idx;
	uint32_t bit;

	for (bit = 0; bit < num_bits; bit++) {
		byte_idx = ((bit_offset + bit) / 8);
		bit_idx = (7 - ((bit_offset + bit) % 8));
		if (ev_data[byte_idx] & (1 << bit_idx))
			value |= (1 << (num_bits - 1 - bit));
	}

	return value;
}

/* overlay the extracted_val onto init_val using the mask */
uint32_t apply_mask_to_value(uint32_t init_val,
			     uint32_t extracted_val,
			     uint32_t mask,
			     uint32_t field_width)
{
	uint32_t result = init_val;
	uint32_t extract_bit = 0;
	uint32_t i;

	/* if the mask is fully set and full width, just return the value */
	if (mask == ((1 << field_width) - 1))
		return extracted_val;

	for (i = 0; i < field_width; i++) {
		if (mask & (1 << i)) {
			result &= ~(1 << i); /* clear bit */
			if (extracted_val & (1 << extract_bit))
				result |= (1 << i);
			extract_bit++;
		}
	}

	return result;
}

/* pack the value into exp_ev at bit_offset with width bits */
void pack_value_into_ev(ev_t *exp_ev,
			uint32_t value,
			uint32_t bit_offset,
			uint32_t width)
{
	uint32_t byte_idx;
	uint32_t bit_idx;
	uint32_t bit;

	for (bit = 0; bit < width; bit++) {
		byte_idx = ((bit_offset + bit) / 8);
		bit_idx = (7 - ((bit_offset + bit) % 8));
		if (value & (1 << (width - 1 - bit)))
			(*exp_ev)[byte_idx] |= (1 << bit_idx);
	}
}

/* verify that all bits in ev_data beyond bit_offset are zero */
void verify_unused_bits_zero(uint8_t *ev_data,
			     uint32_t bit_offset)
{
	uint32_t byte_idx;
	uint32_t bit_idx;
	uint32_t i;

	for (i = bit_offset; i < (MAX_EV_LEN * 8); i++) {
		byte_idx = (i / 8);
		bit_idx = (7 - (i % 8));
		assert(!(ev_data[byte_idx] & (1 << bit_idx)));
	}
}

/* expand the ev at ev_idx and dump it */
void dump_expanded_ev(int ev_idx)
{
	uint8_t *ev_data = _ev_profile.evs[ev_idx];
	struct ev_field *field;
	uint32_t src_bit_offset = 0;
	uint32_t dst_bit_offset = 0;
	uint32_t fmt_width;
	uint32_t exp_val;
	uint32_t mask_bit_cnt;
	uint32_t extracted_ev_bits;
	ev_t exp_ev;

	memset(&exp_ev, 0, sizeof(exp_ev));

	printf("  %02d: ", ev_idx);

	/* iterate over each field and expand its value from the ev */
	for (int i = 0; i < _ev_profile.num_fields; i++) {
		field = &_ev_profile.fields[i];
		fmt_width = _ev_fmt_profile.fmt_fields[i].width;

		/* exp_val is initialized to the init_val */
		exp_val = field->init_val;

		/* mask overlays extracted bits from the ev to exp_val */
		if (field->mask != 0) {
			mask_bit_cnt = count_mask_bits(field->mask);

			/* pull out mask_bit_cnt bits from the ev */
			extracted_ev_bits = ev_extract_bits(ev_data,
							    src_bit_offset,
							    mask_bit_cnt);

			/* overlay mask and extracted bits onto exp_val */
			exp_val = apply_mask_to_value(exp_val,
						      extracted_ev_bits,
						      field->mask,
						      field->width);

			/* skip over the extracted bits pulled from the ev */
			src_bit_offset += mask_bit_cnt;
		}

		/* concat exp_val to the end of the output exp_ev */
		pack_value_into_ev(&exp_ev, exp_val,
				   dst_bit_offset, fmt_width);

		dst_bit_offset += fmt_width;

		printf("fld%d=0x%0*x ", i, ((fmt_width + 3) / 4), exp_val);
	}

	/* verify the ev has no more bits set beyond the used bits */
	verify_unused_bits_zero(ev_data, src_bit_offset);

	printf("\n      ");
	dump_ev(&exp_ev);
}

/* expand and dump all evs in the EV profile */
void dump_expanded_evs(void)
{
	printf("\nEXPANDED EVs:\n");
	for (int i = 0; i < _ev_profile.num_evs; i++)
		dump_expanded_ev(i);
}

#define CHECK_AND_DUMP()             \
	do {                         \
		check_profs();       \
		dump_profs();        \
		dump_expanded_evs(); \
	} while (0)

/****************************************************************************/
/* Example EV Profile Configurations                                        */
/****************************************************************************/

void stev_exp(void)
{
	int i;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("-- Structured EV Explicit ------------------------------\n");
	printf("--------------------------------------------------------\n");

	EV_PROF_INIT(2, 2, MAX_EVS);

	/* EV Format Profile */

	_ev_fmt_profile.type = EV_FMT_TYPE_STEV;

	_ev_fmt_profile.fmt_fields[0].width = 16;
	_ev_fmt_profile.fmt_fields[1].width = 16;

	/* EV Profile */

	_ev_profile.type = EV_TYPE_EXP;

	for (i = 0; i < 2; i++) {
		_ev_profile.fields[i].width    = 16;
		_ev_profile.fields[i].init_val = 0;
		_ev_profile.fields[i].min_val  = 0;
		_ev_profile.fields[i].max_val  = 0;
		_ev_profile.fields[i].mask     = 0xffff;
	}

	/* create some explicit EVs */
	for (i = 0; i < MAX_EVS; i++) {
		uint16_t *p = (uint16_t *)&_ev_profile.evs[i];
		p[0] = RAND(1, 0xffff);
		p[1] = RAND(1, 0xffff);
	}

	CHECK_AND_DUMP();

	EV_PROF_FREE();
}

void stev_gen(void)
{
	int i;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("-- Structured EV Generated -----------------------------\n");
	printf("--------------------------------------------------------\n");

	EV_PROF_INIT(1, 1, MAX_EVS);

	/* EV Format Profile */

	_ev_fmt_profile.type = EV_FMT_TYPE_STEV;

	_ev_fmt_profile.fmt_fields[0].width = 32;

	/* EV Profile */

	_ev_profile.type = EV_TYPE_GEN;

	_ev_profile.fields[0].width    = 32;
	_ev_profile.fields[0].init_val = 0;
	_ev_profile.fields[0].min_val  = 0;
	_ev_profile.fields[0].max_val  = 0;
	_ev_profile.fields[0].mask     = 0xffffffff;

	/* generate some EVs (this is what hardware would do) */
	for (i = 0; i < MAX_EVS; i++) {
		uint32_t *p = (uint32_t *)&_ev_profile.evs[i];
		p[0] = RAND(1, 0xffffffff);
	}

	CHECK_AND_DUMP();

	EV_PROF_FREE();
}

void stev_gen_with_fixed(void)
{
	int i;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("-- Structured EV Generated w/ Fixed --------------------\n");
	printf("--------------------------------------------------------\n");

	EV_PROF_INIT(2, 2, MAX_EVS);

	/* EV Format Profile */

	_ev_fmt_profile.type = EV_FMT_TYPE_STEV;

	_ev_fmt_profile.fmt_fields[0].width = 16;
	_ev_fmt_profile.fmt_fields[1].width = 16;

	/* EV Profile */

	_ev_profile.type = EV_TYPE_GEN;

	for (i = 0; i < 2; i++) {
		_ev_profile.fields[i].width    = 16;
		_ev_profile.fields[i].init_val = ((i + 1) << 8);
		_ev_profile.fields[i].min_val  = 1;
		_ev_profile.fields[i].max_val  = 0xf;
		_ev_profile.fields[i].mask     = 0x000f;
	}

	/* generate some EVs (this is what hardware would do) */
	for (i = 0; i < MAX_EVS; i++) {
		/* pack 2x 4-bit values into 1 byte (8 bits total) */
		uint8_t val1 = RAND(_ev_profile.fields[0].min_val,
				    _ev_profile.fields[0].max_val);
		uint8_t val2 = RAND(_ev_profile.fields[1].min_val,
				    _ev_profile.fields[1].max_val);
		uint8_t packed = ((val1 << 4) | val2);
		uint8_t *p = (uint8_t *)&_ev_profile.evs[i];
		p[0] = packed;
	}

	CHECK_AND_DUMP();

	EV_PROF_FREE();
}

void stev_gen_with_fixed_weird_mask(void)
{
	int i;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("-- Structured EV Generated w/ Fixed & Weird Mask -------\n");
	printf("--------------------------------------------------------\n");

	EV_PROF_INIT(2, 2, MAX_EVS);

	/* EV Format Profile */

	_ev_fmt_profile.type = EV_FMT_TYPE_STEV;

	_ev_fmt_profile.fmt_fields[0].width = 16;
	_ev_fmt_profile.fmt_fields[1].width = 16;

	/* EV Profile */

	_ev_profile.type = EV_TYPE_GEN;

	for (i = 0; i < 2; i++) {
		_ev_profile.fields[i].width    = 16;
		_ev_profile.fields[i].init_val = ((i + 1) << 12);
		_ev_profile.fields[i].min_val  = 1;
		_ev_profile.fields[i].max_val  = 0xff;
		_ev_profile.fields[i].mask     = 0x0f0f;
	}

	/* generate some EVs (this is what hardware would do) */
	for (i = 0; i < MAX_EVS; i++) {
		/* pack 2x 8-bit values into 2 byte (16 bits total) */
		uint8_t val1 = RAND(_ev_profile.fields[0].min_val,
				    _ev_profile.fields[0].max_val);
		uint8_t val2 = RAND(_ev_profile.fields[1].min_val,
				    _ev_profile.fields[1].max_val);
		uint16_t packed = ((val1 << 8) | val2);
		uint16_t *p = (uint16_t *)&_ev_profile.evs[i];
		p[0] = packed;
	}

	CHECK_AND_DUMP();

	EV_PROF_FREE();
}

void srv6_exp(void)
{
	int i;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("-- SRv6 Explicit ---------------------------------------\n");
	printf("--------------------------------------------------------\n");

	EV_PROF_INIT(5, 5, MAX_EVS);

	/* EV Format Profile */

	_ev_fmt_profile.type = EV_FMT_TYPE_SRV6;

	/* SRv6 32b locator, 4x 16b uSIDs */
	_ev_fmt_profile.fmt_fields[0].width = 32;
	for (i = 1; i < 5; i++)
		_ev_fmt_profile.fmt_fields[i].width = 16;

	/* EV Profile */

	_ev_profile.type = EV_TYPE_EXP;

	_ev_profile.fields[0].width    = 32;
	_ev_profile.fields[0].init_val = 0xcafecafe; /* SRv6 locator */
	_ev_profile.fields[0].min_val  = 0;
	_ev_profile.fields[0].max_val  = 0;
	_ev_profile.fields[0].mask     = 0;

	for (i = 1; i < 5; i++) {
		_ev_profile.fields[i].width    = 16;
		_ev_profile.fields[i].init_val = 0;
		_ev_profile.fields[i].min_val  = 0;
		_ev_profile.fields[i].max_val  = 0;
		_ev_profile.fields[i].mask     = 0xffff;
	}

	/* create some explicit EVs */
	for (i = 0; i < MAX_EVS; i++) {
		uint16_t *p = (uint16_t *)&_ev_profile.evs[i];
		p[0] = RAND(1, 0xffff);
		p[1] = RAND(1, 0xffff);
		p[2] = RAND(1, 0xffff);
		p[3] = RAND(1, 0xffff);
	}

	CHECK_AND_DUMP();

	EV_PROF_FREE();
}

void srv6_exp_subset(void)
{
	int i;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("-- SRv6 Explicit (subset) ------------------------------\n");
	printf("--------------------------------------------------------\n");

	EV_PROF_INIT(5, 3, MAX_EVS);

	/* EV Format Profile */

	_ev_fmt_profile.type = EV_FMT_TYPE_SRV6;

	/* SRv6 32b locator, 4x 16b uSIDs */
	_ev_fmt_profile.fmt_fields[0].width = 32;
	for (i = 1; i < 5; i++)
		_ev_fmt_profile.fmt_fields[i].width = 16;

	/* EV Profile */

	_ev_profile.type = EV_TYPE_EXP;

	_ev_profile.fields[0].width    = 32;
	_ev_profile.fields[0].init_val = 0xdeaddead; /* SRv6 locator */
	_ev_profile.fields[0].min_val  = 0;
	_ev_profile.fields[0].max_val  = 0;
	_ev_profile.fields[0].mask     = 0;

	for (i = 1; i < 3; i++) {
		_ev_profile.fields[i].width    = 16;
		_ev_profile.fields[i].init_val = 0;
		_ev_profile.fields[i].min_val  = 0;
		_ev_profile.fields[i].max_val  = 0;
		_ev_profile.fields[i].mask     = 0xffff;
	}

	/* create some explicit EVs */
	for (i = 0; i < MAX_EVS; i++) {
		uint16_t *p = (uint16_t *)&_ev_profile.evs[i];
		p[0] = RAND(1, 0xffff);
		p[1] = RAND(1, 0xffff);
	}

	CHECK_AND_DUMP();

	EV_PROF_FREE();
}

void srv6_gen(void)
{
	int i;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("-- SRv6 Generated --------------------------------------\n");
	printf("--------------------------------------------------------\n");

	EV_PROF_INIT(5, 5, MAX_EVS);

	/* EV Format Profile */

	_ev_fmt_profile.type = EV_FMT_TYPE_SRV6;

	/* SRv6 32b locator, 4x 16b uSIDs */
	_ev_fmt_profile.fmt_fields[0].width = 32;
	for (i = 1; i < 5; i++)
		_ev_fmt_profile.fmt_fields[i].width = 16;

	/* EV Profile */

	_ev_profile.type = EV_TYPE_GEN;

	_ev_profile.fields[0].width    = 32;
	_ev_profile.fields[0].init_val = 0x42424242; /* SRv6 locator */
	_ev_profile.fields[0].min_val  = 0;
	_ev_profile.fields[0].max_val  = 0;
	_ev_profile.fields[0].mask     = 0;

	for (i = 1; i < 5; i++) {
		_ev_profile.fields[i].width    = 4;
		_ev_profile.fields[i].init_val = 0;
		_ev_profile.fields[i].min_val  = 1;
		_ev_profile.fields[i].max_val  = 0xf;
		_ev_profile.fields[i].mask     = 0xf;
	}

	/* generate some EVs (this is what hardware would do) */
	for (i = 0; i < MAX_EVS; i++) {
		/* pack 4x 4-bit values into 2 bytes (16 bits total) */
		uint16_t val1 = RAND(_ev_profile.fields[1].min_val,
				     _ev_profile.fields[1].max_val);
		uint16_t val2 = RAND(_ev_profile.fields[2].min_val,
				     _ev_profile.fields[2].max_val);
		uint16_t val3 = RAND(_ev_profile.fields[3].min_val,
				     _ev_profile.fields[3].max_val);
		uint16_t val4 = RAND(_ev_profile.fields[4].min_val,
				     _ev_profile.fields[4].max_val);
		uint16_t packed = ((val1 << 12) | (val2 << 8) |
				   (val3 << 4) | val4);
		uint16_t *p = (uint16_t *)&_ev_profile.evs[i];
		p[0] = packed;
	}

	CHECK_AND_DUMP();

	EV_PROF_FREE();
}

void srv6_gen_with_fixed(void)
{
	int i;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("-- SRv6 Generated w/ Fixed -----------------------------\n");
	printf("--------------------------------------------------------\n");

	EV_PROF_INIT(5, 5, MAX_EVS);

	/* EV Format Profile */

	_ev_fmt_profile.type = EV_FMT_TYPE_SRV6;

	/* SRv6 32b locator, 4x 16b uSIDs */
	_ev_fmt_profile.fmt_fields[0].width = 32;
	for (i = 1; i < 5; i++)
		_ev_fmt_profile.fmt_fields[i].width = 16;

	/* EV Profile */

	_ev_profile.type = EV_TYPE_GEN;

	_ev_profile.fields[0].width    = 32;
	_ev_profile.fields[0].init_val = 0x13131313; /* SRv6 locator */
	_ev_profile.fields[0].min_val  = 0;
	_ev_profile.fields[0].max_val  = 0;
	_ev_profile.fields[0].mask     = 0;

	for (i = 1; i < 5; i++) {
		_ev_profile.fields[i].width    = 16;
		_ev_profile.fields[i].init_val = (i << 8);
		_ev_profile.fields[i].min_val  = 1;
		_ev_profile.fields[i].max_val  = 0xf;
		_ev_profile.fields[i].mask     = 0x000f;
	}

	/* generate some EVs (this is what hardware would do) */
	for (i = 0; i < MAX_EVS; i++) {
		/* Pack 4x 4-bit values into 2 bytes (16 bits total) */
		uint16_t val1 = RAND(_ev_profile.fields[1].min_val,
				     _ev_profile.fields[1].max_val);
		uint16_t val2 = RAND(_ev_profile.fields[2].min_val,
				     _ev_profile.fields[2].max_val);
		uint16_t val3 = RAND(_ev_profile.fields[3].min_val,
				     _ev_profile.fields[3].max_val);
		uint16_t val4 = RAND(_ev_profile.fields[4].min_val,
				     _ev_profile.fields[4].max_val);
		uint16_t packed = ((val1 << 12) | (val2 << 8) |
				   (val3 << 4) | val4);
		uint16_t *p = (uint16_t *)&_ev_profile.evs[i];
		p[0] = packed;
	}

	CHECK_AND_DUMP();

	EV_PROF_FREE();
}

void srv6_srh_exp(void)
{
	int i;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("-- SRv6+SRH Explicit -----------------------------------\n");
	printf("--------------------------------------------------------\n");

	EV_PROF_INIT(10, 10, MAX_EVS);

	/* EV Format Profile */

	_ev_fmt_profile.type = EV_FMT_TYPE_SRV6;

	/* SRv6 32b locator, 6x 16b uSIDs, SRH 32b locator, 2x 16b uSIDs */
	_ev_fmt_profile.fmt_fields[0].width = 32;
	for (i = 1; i < 7; i++)
		_ev_fmt_profile.fmt_fields[i].width = 16;
	_ev_fmt_profile.fmt_fields[7].width = 32;
	for (i = 8; i < 10; i++)
		_ev_fmt_profile.fmt_fields[i].width = 16;

	/* EV Profile */

	_ev_profile.type = EV_TYPE_EXP;

	_ev_profile.fields[0].width    = 32;
	_ev_profile.fields[0].init_val = 0xcafecafe; /* SRv6 locator */
	_ev_profile.fields[0].min_val  = 0;
	_ev_profile.fields[0].max_val  = 0;
	_ev_profile.fields[0].mask     = 0;

	for (i = 1; i < 7; i++) {
		_ev_profile.fields[i].width    = 16;
		_ev_profile.fields[i].init_val = 0;
		_ev_profile.fields[i].min_val  = 0;
		_ev_profile.fields[i].max_val  = 0;
		_ev_profile.fields[i].mask     = 0xffff;
	}

	_ev_profile.fields[7].width    = 32;
	_ev_profile.fields[7].init_val = 0xdecafbad; /* SRH locator */
	_ev_profile.fields[7].min_val  = 0;
	_ev_profile.fields[7].max_val  = 0;
	_ev_profile.fields[7].mask     = 0;

	for (i = 8; i < 10; i++) {
		_ev_profile.fields[i].width    = 16;
		_ev_profile.fields[i].init_val = 0;
		_ev_profile.fields[i].min_val  = 0;
		_ev_profile.fields[i].max_val  = 0;
		_ev_profile.fields[i].mask     = 0xffff;
	}

	/* create some explicit EVs */
	for (i = 0; i < MAX_EVS; i++) {
		uint16_t *p = (uint16_t *)&_ev_profile.evs[i];
		p[0] = RAND(1, 0xffff);
		p[1] = RAND(1, 0xffff);
		p[2] = RAND(1, 0xffff);
		p[3] = RAND(1, 0xffff);
		p[4] = RAND(1, 0xffff);
		p[5] = RAND(1, 0xffff);
		p[6] = RAND(1, 0xffff);
		p[7] = RAND(1, 0xffff);
	}

	CHECK_AND_DUMP();

	EV_PROF_FREE();
}

/****************************************************************************/
/* Main...                                                                  */
/****************************************************************************/

void (*tests[])() = {
	stev_exp,
	stev_gen,
	stev_gen_with_fixed,
	stev_gen_with_fixed_weird_mask,
	srv6_exp,
	srv6_exp_subset,
	srv6_gen,
	srv6_gen_with_fixed,
	srv6_srh_exp,
};

int main(int argc, char *argv[])
{
	//srandom(time(NULL));
	srandom(0); /* For repeatable results! */

	int num_tests = (sizeof(tests) / sizeof(tests[0]));
	for (int i = 0; i < num_tests; i++)
		(*tests[i])();

	return 0;
}

