// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-2.0+

/* NOTE: While part of this header is originally from libmpeg2, which is GPL - licensed,
 * it's not substantial and does not contain any functions, therefore can be argued
 * not to be a derived work. See http://lkml.iu.edu/hypermail/linux/kernel/0301.1/0362.html
 * The constants themselves can also be argued to be part of the MPEG-2 standard, whose
 * patents expired worldwide in Feb 2020.
 *
 * Original copyright included for completeness:
 *   Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
 *   Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 */

#ifndef GEKKO2_IPU_VLC_TABLES_H
#define GEKKO2_IPU_VLC_TABLES_H
#include <stdint.h>

#ifdef _MSC_VER
#define VLC_ALIGNED16 __declspec(align(16))
#else
#define VLC_ALIGNED16 __attribute__((aligned(16)))
#endif

enum macroblock_modes
{
	MACROBLOCK_INTRA = 1,
	MACROBLOCK_PATTERN = 2,
	MACROBLOCK_MOTION_BACKWARD = 4,
	MACROBLOCK_MOTION_FORWARD = 8,
	MACROBLOCK_QUANT = 16,
	DCT_TYPE_INTERLACED = 32
};

enum motion_type
{
	MOTION_TYPE_SHIFT = 6,
	MOTION_TYPE_MASK = (3 * 64),
	MOTION_TYPE_BASE = 64,
	MC_FIELD = (1 * 64),
	MC_FRAME = (2 * 64),
	MC_16X8 = (2 * 64),
	MC_DMV = (3 * 64)
};

/* picture structure */
enum picture_structure
{
	TOP_FIELD = 1,
	BOTTOM_FIELD = 2,
	FRAME_PICTURE = 3
};

/* picture coding type */
enum picture_coding_type
{
	I_TYPE = 1,
	P_TYPE = 2,
	B_TYPE = 3,
	D_TYPE = 4
};

typedef struct MBtab
{
	uint8_t modes;
	uint8_t len;
} MBtab;

typedef struct MVtab
{
	uint8_t delta;
	uint8_t len;
} MVtab;

typedef struct DMVtab
{
	int8_t dmv;
	uint8_t len;
} DMVtab;

typedef struct CBPtab
{
	uint8_t cbp;
	uint8_t len;
} CBPtab;

typedef struct DCtab
{
	uint8_t size;
	uint8_t len;
} DCtab;

typedef struct DCTtab
{
	uint8_t run;
	uint8_t level;
	uint8_t len;
} DCTtab;

typedef struct MBAtab
{
	uint8_t mba;
	uint8_t len;
} MBAtab;


#define INTRA MACROBLOCK_INTRA
#define QUANT MACROBLOCK_QUANT

static const MBtab MB_I[] = {
	{INTRA | QUANT, 2}, {INTRA, 1}};

#define MC MACROBLOCK_MOTION_FORWARD
#define CODED MACROBLOCK_PATTERN

static const VLC_ALIGNED16 MBtab MB_P[] = {
	{INTRA | QUANT, 6}, {CODED | QUANT, 5}, {MC | CODED | QUANT, 5}, {INTRA, 5},
	{MC, 3}, {MC, 3}, {MC, 3}, {MC, 3},
	{CODED, 2}, {CODED, 2}, {CODED, 2}, {CODED, 2},
	{CODED, 2}, {CODED, 2}, {CODED, 2}, {CODED, 2},
	{MC | CODED, 1}, {MC | CODED, 1}, {MC | CODED, 1}, {MC | CODED, 1},
	{MC | CODED, 1}, {MC | CODED, 1}, {MC | CODED, 1}, {MC | CODED, 1},
	{MC | CODED, 1}, {MC | CODED, 1}, {MC | CODED, 1}, {MC | CODED, 1},
	{MC | CODED, 1}, {MC | CODED, 1}, {MC | CODED, 1}, {MC | CODED, 1}};

#define FWD MACROBLOCK_MOTION_FORWARD
#define BWD MACROBLOCK_MOTION_BACKWARD
#define INTER MACROBLOCK_MOTION_FORWARD | MACROBLOCK_MOTION_BACKWARD

static const VLC_ALIGNED16 MBtab MB_B[] = {
	{0, 0}, {INTRA | QUANT, 6},
	{BWD | CODED | QUANT, 6}, {FWD | CODED | QUANT, 6},
	{INTER | CODED | QUANT, 5}, {INTER | CODED | QUANT, 5},
	{INTRA, 5}, {INTRA, 5},
	{FWD, 4}, {FWD, 4}, {FWD, 4}, {FWD, 4},
	{FWD | CODED, 4}, {FWD | CODED, 4}, {FWD | CODED, 4}, {FWD | CODED, 4},
	{BWD, 3}, {BWD, 3}, {BWD, 3}, {BWD, 3},
	{BWD, 3}, {BWD, 3}, {BWD, 3}, {BWD, 3},
	{BWD | CODED, 3}, {BWD | CODED, 3}, {BWD | CODED, 3}, {BWD | CODED, 3},
	{BWD | CODED, 3}, {BWD | CODED, 3}, {BWD | CODED, 3}, {BWD | CODED, 3},
	{INTER, 2}, {INTER, 2}, {INTER, 2}, {INTER, 2},
	{INTER, 2}, {INTER, 2}, {INTER, 2}, {INTER, 2},
	{INTER, 2}, {INTER, 2}, {INTER, 2}, {INTER, 2},
	{INTER, 2}, {INTER, 2}, {INTER, 2}, {INTER, 2},
	{INTER | CODED, 2}, {INTER | CODED, 2}, {INTER | CODED, 2}, {INTER | CODED, 2},
	{INTER | CODED, 2}, {INTER | CODED, 2}, {INTER | CODED, 2}, {INTER | CODED, 2},
	{INTER | CODED, 2}, {INTER | CODED, 2}, {INTER | CODED, 2}, {INTER | CODED, 2},
	{INTER | CODED, 2}, {INTER | CODED, 2}, {INTER | CODED, 2}, {INTER | CODED, 2}};

#undef INTRA
#undef QUANT
#undef MC
#undef CODED
#undef FWD
#undef BWD
#undef INTER


static const MVtab MV_4[] = {
	{3, 6}, {2, 4}, {1, 3}, {1, 3}, {0, 2}, {0, 2}, {0, 2}, {0, 2}};

static const VLC_ALIGNED16 MVtab MV_10[] = {
	{0, 10}, {0, 10}, {0, 10}, {0, 10}, {0, 10}, {0, 10}, {0, 10}, {0, 10},
	{0, 10}, {0, 10}, {0, 10}, {0, 10}, {15, 10}, {14, 10}, {13, 10}, {12, 10},
	{11, 10}, {10, 10}, {9, 9}, {9, 9}, {8, 9}, {8, 9}, {7, 9}, {7, 9},
	{6, 7}, {6, 7}, {6, 7}, {6, 7}, {6, 7}, {6, 7}, {6, 7}, {6, 7},
	{5, 7}, {5, 7}, {5, 7}, {5, 7}, {5, 7}, {5, 7}, {5, 7}, {5, 7},
	{4, 7}, {4, 7}, {4, 7}, {4, 7}, {4, 7}, {4, 7}, {4, 7}, {4, 7}};


static const DMVtab DMV_2[] = {
	{0, 1}, {0, 1}, {1, 2}, {(int8_t)-1, 2}};


typedef struct MBAtabSet
{
	MBAtab mba5[30];
	MBAtab mba11[26 * 4];
} MBAtabSet;
static const VLC_ALIGNED16 MBAtabSet MBA = {
	{// mba5
		{6, 5}, {5, 5}, {4, 4}, {4, 4}, {3, 4}, {3, 4},
		{2, 3}, {2, 3}, {2, 3}, {2, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
		{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
		{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}},

	{// mba11
		{32, 11}, {31, 11}, {30, 11}, {29, 11},
		{28, 11}, {27, 11}, {26, 11}, {25, 11},
		{24, 11}, {23, 11}, {22, 11}, {21, 11},
		{20, 10}, {20, 10}, {19, 10}, {19, 10},
		{18, 10}, {18, 10}, {17, 10}, {17, 10},
		{16, 10}, {16, 10}, {15, 10}, {15, 10},
		{14, 8}, {14, 8}, {14, 8}, {14, 8},
		{14, 8}, {14, 8}, {14, 8}, {14, 8},
		{13, 8}, {13, 8}, {13, 8}, {13, 8},
		{13, 8}, {13, 8}, {13, 8}, {13, 8},
		{12, 8}, {12, 8}, {12, 8}, {12, 8},
		{12, 8}, {12, 8}, {12, 8}, {12, 8},
		{11, 8}, {11, 8}, {11, 8}, {11, 8},
		{11, 8}, {11, 8}, {11, 8}, {11, 8},
		{10, 8}, {10, 8}, {10, 8}, {10, 8},
		{10, 8}, {10, 8}, {10, 8}, {10, 8},
		{9, 8}, {9, 8}, {9, 8}, {9, 8},
		{9, 8}, {9, 8}, {9, 8}, {9, 8},
		{8, 7}, {8, 7}, {8, 7}, {8, 7},
		{8, 7}, {8, 7}, {8, 7}, {8, 7},
		{8, 7}, {8, 7}, {8, 7}, {8, 7},
		{8, 7}, {8, 7}, {8, 7}, {8, 7},
		{7, 7}, {7, 7}, {7, 7}, {7, 7},
		{7, 7}, {7, 7}, {7, 7}, {7, 7},
		{7, 7}, {7, 7}, {7, 7}, {7, 7},
		{7, 7}, {7, 7}, {7, 7}, {7, 7}}};


#endif
