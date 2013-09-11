/********************************************************************************
 * IBM Cell Software Spectrometer
 * Copyright (C) 2008 Jan Wagner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 ********************************************************************************/
#ifndef _CELL_SPECTROMETER_H
#define _CELL_SPECTROMETER_H

#include <stdlib.h>
#include <math.h>
#ifdef __SPU__
    #include <simdmath.h>
    #include <libgmath.h>
    #include <fft_1d.h>
    #include <fft_1d_r2.h>
    #include <spu_mfcio.h>
    #include <stdio.h>
    #include <sin18_v.h>
    #include <float.h>
    #include <libmisc.h>
    #include <vec_literal.h>
#else
    #include <libspe.h>
    #include <stdio.h>
    #include <sched.h>
    #include <sys/wait.h>
    // #include <malloc_align.h>
    #include <free_align.h>
    #include <sys/time.h>
#endif


// ------------------------------------------------------------------------------------------------
//    D E F I N E S
// ------------------------------------------------------------------------------------------------

// Data buffers
#define FFT_POINTS      1024                 // 512V as complex, 256V as real
#define IF_VECSPERBLOCK 1024                 // 1kV
#define IF_BYTEPERBLOCK (16*IF_VECSPERBLOCK) // 16kB, real input data

// Branch hints
#ifndef unlikely
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#endif

// Executing time profiling
#define __freq__      3200     // CPU frequency in MHz (cat /proc/cpuinfo)
#define __timebase__  79800000 // Cell Blade: 14318000  PS3: 79800000 (cat /proc/cpuinfo)

// Unrolling helpers
#define REPLICATE_2(x,y) { y=0; { x } y=1; { x } }
#define REPLICATE_4(x,y) { y=0; { x } y=1; { x } y=2; { x } y=3; { x } }
#define UNROLL_BY_2(x)  { x }{ x }
#define UNROLL_BY_4(x)  { x }{ x }{ x }{ x }
#define UNROLL_BY_8(x)  UNROLL_BY_4(x) UNROLL_BY_4(x)
#define UNROLL_BY_16(x) UNROLL_BY_8(x) UNROLL_BY_8(x)

// Generic vector helpers
#define VEC_4f(x)          (vector float){x, x, x, x}
#define SHREP_VEC_ELEM(x)  (vector unsigned char) { \
                                0+4*(x),1+4*(x),2+4*(x),3+4*(x), \
                                0+4*(x),1+4*(x),2+4*(x),3+4*(x), \
                                0+4*(x),1+4*(x),2+4*(x),3+4*(x), \
                                0+4*(x),1+4*(x),2+4*(x),3+4*(x) }
#define vec_sld(A,B,bytes)  spu_shuffle(A, B, \
     ( (vector unsigned char){0+(bytes),  1+(bytes),  2+(bytes),  3+(bytes), \
                              4+(bytes),  5+(bytes),  6+(bytes),  7+(bytes), \
                              8+(bytes),  9+(bytes), 10+(bytes), 11+(bytes), \
                             12+(bytes), 13+(bytes), 14+(bytes), 15+(bytes)} ) )

// Alignment helpers
#define _CLINE_ALIGN     __attribute__ ((aligned (128)))
#define _QUAD_ALIGN      __attribute__ ((aligned (16)))
#define _MA_CLINE_ALIGN  7 // for the malloc_align(bytes,N) SPE version that aligns to 2^N=128 <-> N=7

// ------------------------------------------------------------------------------------------------
//    S T R U C T S
// ------------------------------------------------------------------------------------------------

// Address conversion 32/64-bit
typedef union {
    unsigned long long ull;
    unsigned int ui[2];
    void* p;
} addr64;

// Task context
typedef struct control_block_tt {
    unsigned int    rank        _CLINE_ALIGN;
    unsigned int    terminate_flag;      // 0 to keep running, 1 to exit thead after processing
    unsigned int    fft_points;          // = 2^N*1024
    unsigned int    fft_points_log2;
    unsigned int    overlap_factor;      // 2 for 50%
    unsigned int    integrate_ffts;      // how many FFTs to sum up
    unsigned int    samples_per_rawbyte; // <-> bits per sample
    addr64 rawif_src;
    unsigned int    rawif_bytecount;
    addr64 spectrum_dst;
} control_block_t       _CLINE_ALIGN;

// Complex nums
typedef struct _complex_t {
    float re, im;
} complex_t;


// ------------------------------------------------------------------------------------------------
//    S P U   D E C L A R E S
// ------------------------------------------------------------------------------------------------

#ifdef __SPU__

void calc_fft_twiddle();
void calc_cos_window_function();
inline void reset_spectrum();
inline void add_to_spectrum();
inline void unpack_windowed_to_fft(vector unsigned char* in, vector float* out);

#endif // __SPU__

#endif // _CELL_SPECTROMETER_H
