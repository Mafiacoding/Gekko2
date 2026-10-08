// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-FileCopyrightText: 2000-2002 Michel Lespinasse
// SPDX-FileCopyrightText: 1999-2000 Aaron Holtzman
// SPDX-License-Identifier: GPL-3.0+
/* Scalar IDCT adaptation from PCSX2 IPU_MultiISA.cpp 3c8df07.
 * Multiplication replaces left shifts of negative values; explicit samples
 * avoid aliasing and unaligned host integer accesses. */
#include <stdint.h>
#define W1 2841 /* 2048*sqrt (2)*cos (1*pi/16) */
#define W2 2676 /* 2048*sqrt (2)*cos (2*pi/16) */
#define W3 2408 /* 2048*sqrt (2)*cos (3*pi/16) */
#define W5 1609 /* 2048*sqrt (2)*cos (5*pi/16) */
#define W6 1108 /* 2048*sqrt (2)*cos (6*pi/16) */
#define W7 565  /* 2048*sqrt (2)*cos (7*pi/16) */

/*
 * In legal streams, the IDCT output should be between -384 and +384.
 * In corrupted streams, it is possible to force the IDCT output to go
 * to +-3826 - this is the worst case for a column IDCT where the
 * column inputs are 16-bit values.
 */

static void butterfly(int *t0, int *t1, int w0, int w1, int d0, int d1)
{
	int tmp = w0 * (d0 + d1);
	*t0 = tmp + (w1 - w0) * d1;
	*t1 = tmp - (w1 + w0) * d0;
}

void ipu_idct(int16_t *block)
{
	for (int i = 0; i < 8; i++)
	{
		int16_t* const rblock = block + 8 * i;
		if (!(rblock[1]|rblock[2]|rblock[3]|rblock[4]|rblock[5]|rblock[6]|rblock[7])) {
            int16_t dc=(int16_t)(rblock[0]*8);
            for(unsigned j=0;j<8;j++)rblock[j]=dc;
            continue;
        }

		int a0, a1, a2, a3;
		{
			const int d0 = (rblock[0] * 2048) + 128;
			const int d1 = rblock[1];
			const int d2 = rblock[2] * 2048;
			const int d3 = rblock[3];
			int t0 = d0 + d2;
			int t1 = d0 - d2;
			int t2, t3;
			butterfly(&t2, &t3, W6, W2, d3, d1);
			a0 = t0 + t2;
			a1 = t1 + t3;
			a2 = t1 - t3;
			a3 = t0 - t2;
		}

		int b0, b1, b2, b3;
		{
			const int d0 = rblock[4];
			const int d1 = rblock[5];
			const int d2 = rblock[6];
			const int d3 = rblock[7];
			int t0, t1, t2, t3;
			butterfly(&t0, &t1, W7, W1, d3, d0);
			butterfly(&t2, &t3, W3, W5, d1, d2);
			b0 = t0 + t2;
			b3 = t1 + t3;
			t0 -= t2;
			t1 -= t3;
			b1 = ((int64_t)(t0 + t1) * 181) >> 8;
			b2 = ((int64_t)(t0 - t1) * 181) >> 8;
		}

		rblock[0] = (a0 + b0) >> 8;
		rblock[1] = (a1 + b1) >> 8;
		rblock[2] = (a2 + b2) >> 8;
		rblock[3] = (a3 + b3) >> 8;
		rblock[4] = (a3 - b3) >> 8;
		rblock[5] = (a2 - b2) >> 8;
		rblock[6] = (a1 - b1) >> 8;
		rblock[7] = (a0 - b0) >> 8;
	}

	for (int i = 0; i < 8; i++)
	{
		int16_t* const cblock = block + i;

		int a0, a1, a2, a3;
		{
			const int d0 = (cblock[8 * 0] * 2048) + 65536;
			const int d1 = cblock[8 * 1];
			const int d2 = cblock[8 * 2] * 2048;
			const int d3 = cblock[8 * 3];
			const int t0 = d0 + d2;
			const int t1 = d0 - d2;
			int t2;
			int t3;
			butterfly(&t2, &t3, W6, W2, d3, d1);
			a0 = t0 + t2;
			a1 = t1 + t3;
			a2 = t1 - t3;
			a3 = t0 - t2;
		}

		int b0, b1, b2, b3;
		{
			const int d0 = cblock[8 * 4];
			const int d1 = cblock[8 * 5];
			const int d2 = cblock[8 * 6];
			const int d3 = cblock[8 * 7];
			int t0, t1, t2, t3;
			butterfly(&t0, &t1, W7, W1, d3, d0);
			butterfly(&t2, &t3, W3, W5, d1, d2);
			b0 = t0 + t2;
			b3 = t1 + t3;
			t0 = (t0 - t2) >> 8;
			t1 = (t1 - t3) >> 8;
			b1 = (t0 + t1) * 181;
			b2 = (t0 - t1) * 181;
		}

		cblock[8 * 0] = (a0 + b0) >> 17;
		cblock[8 * 1] = (a1 + b1) >> 17;
		cblock[8 * 2] = (a2 + b2) >> 17;
		cblock[8 * 3] = (a3 + b3) >> 17;
		cblock[8 * 4] = (a3 - b3) >> 17;
		cblock[8 * 5] = (a2 - b2) >> 17;
		cblock[8 * 6] = (a1 - b1) >> 17;
		cblock[8 * 7] = (a0 - b0) >> 17;
	}
}

