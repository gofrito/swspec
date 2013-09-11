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

#include "cell_spectrometer.h"

#ifndef min
    #define min(x,y) ((x>y) ? y : x)
    #define max(x,y) ((x>y) ? x : y)
#endif

// -- CONTROL BLOCK
static control_block_t cb _CLINE_ALIGN;

// -- DATA BUFFERS
static vector unsigned char* raw[2];
static vector float* pow_reim_accu;
static vector float* fft_reim;
static vector float* windowfunc;
static complex_t*    fft_W;

// -- GLOBALS
unsigned int raw_bytelen;
unsigned int fft_bytelen;
unsigned int re_bytelen;
unsigned int fft_veclen;
unsigned int re_veclen;
vector float scaler;


// ------------------------------------------------------------------------------------------------
//    D M A   H E L P E R S
// ------------------------------------------------------------------------------------------------

#define TAG_CB          1 // control block DMA access tag
#define TAG_SPECTR      2 // spectrum data output tag
#define TAG_RAW_A       3 // raw sample data input tag
#define TAG_RAW_B       4 // raw sample data input tag
#define TAG_A           5 // generic tags
#define TAG_B           6
#define tag2mask(x) (1<<x)

inline void store_data(addr64 dst, char* src, size_t remaining, int tag) {
   unsigned long long dstEA = dst.ull;
   int chunklen             = 16384;
   while (remaining > 0) {
      if (remaining < 16384) { chunklen = remaining; }
      mfc_putf(src, dstEA, chunklen, tag, 0, 0);
      dstEA     += chunklen;
      src       += chunklen;
      remaining -= chunklen;
   }
}

inline void wait_for_tag(int _tag) {
    mfc_write_tag_mask(tag2mask(_tag));
    mfc_read_tag_status_all();
    return;
}

inline void wait_for_mask(int _mask) {
    mfc_write_tag_mask(_mask);
    mfc_read_tag_status_all();
    return;
}


// ------------------------------------------------------------------------------------------------
//    O T H E R   H E L P E R S
// ------------------------------------------------------------------------------------------------

inline void print4f(vector float f) {
    printf("%f %f %f %f ", spu_extract(f,0), spu_extract(f,1),spu_extract(f,2),spu_extract(f,3));
}


// ------------------------------------------------------------------------------------------------
//    M A I N
// ------------------------------------------------------------------------------------------------

int main(unsigned long long speid, addr64 argp, addr64 envp) {

    static int preinit_done = 0;

    unsigned int i;
    unsigned int t_start = 0, t_spu = 0;

    int buf, next_buf;                     // raw-data double buffering
    unsigned int num_ffts_done;            // how many FFTs have been done after end of previous spectrum
    unsigned int total_ffts_done;          // how many FFTs have been done in total on the current raw buffer
    unsigned int num_spectra_returned;     // how many complete integrated spectra have been DMA'ed back during current raw-data task

    unsigned raw_delta_bytelen;            // how many raw bytes to advance after FFT (overlap!)

    speid = 0; envp.ull = 0; num_ffts_done = 0; total_ffts_done = 0; num_spectra_returned = 0;

repeat:
    /* sync start with PPU */
    i = spu_read_in_mbox();

    /* fetch control block */
    mfc_get(&cb, argp.ui[1], sizeof(cb), TAG_CB, 0, 0);
    mfc_write_tag_mask(1<<TAG_CB);
    mfc_read_tag_status_all();

    /* terminate SPU program if told */
    if (0 != cb.terminate_flag) {
       return 0;
    }

    /* internal init */
    if (0 == preinit_done) {

       raw_bytelen   = cb.fft_points / cb.samples_per_rawbyte;       // M bytes to get N samples
       re_bytelen    = cb.fft_points * sizeof(float);                // 1 {re} x N data
       fft_bytelen   = 2 * re_bytelen;                               // 1 {re,im} x N bytes

       re_veclen     = cb.fft_points / sizeof(float);                // 4 {re} floats per vector
       fft_veclen    = 2 * re_veclen;                                // 2 {re,im} per vector

       scaler        = VEC_4f(1.0/cb.integrate_ffts);

       raw[0]        = malloc_align(raw_bytelen, _MA_CLINE_ALIGN);
       raw[1]        = malloc_align(raw_bytelen, _MA_CLINE_ALIGN);
       windowfunc    = malloc_align(re_bytelen,  _MA_CLINE_ALIGN); // for {re} real data

       fft_W         = malloc_align(fft_bytelen, _MA_CLINE_ALIGN);   // {re,im} x N
       fft_reim      = malloc_align(fft_bytelen, _MA_CLINE_ALIGN);   // {re,im} x N
       pow_reim_accu = malloc_align(fft_bytelen, _MA_CLINE_ALIGN);   // {re,im} x N

       num_ffts_done = 0;
       total_ffts_done = 0;

       calc_fft_twiddle();
       calc_cos_window_function();

       reset_spectrum();

       preinit_done = 1;
    }

    /* prepare overlap */
    raw_delta_bytelen = raw_bytelen/cb.overlap_factor;

    /* prefetch first data */
    buf = 0;
    mfc_get(raw[buf], cb.rawif_src.ull, raw_bytelen, TAG_RAW_A + buf, 0, 0);
    cb.rawif_src.ull   += raw_delta_bytelen;
    cb.rawif_bytecount -= raw_delta_bytelen;

    /* start to measure execution time */
    spu_write_decrementer(0x7fffffff);
    t_start = spu_read_decrementer();

    /* do whatever maths should be done */
    num_spectra_returned = 0;
    total_ffts_done = 0;
    while (cb.rawif_bytecount >= raw_delta_bytelen)
    {
        /* already prefetch next raw buffer */
        next_buf = (buf + 1) % 2;
        if (cb.rawif_bytecount >= raw_delta_bytelen) {
           mfc_get(raw[buf], cb.rawif_src.ull, raw_bytelen, TAG_RAW_A + next_buf, 0, 0);
           cb.rawif_src.ull   += raw_delta_bytelen;
           cb.rawif_bytecount -= raw_delta_bytelen;
        }

        /* process current buffer */
        wait_for_tag(TAG_RAW_A + buf);

        unpack_windowed_to_fft(raw[buf], fft_reim);
        _fft_1d_r2(fft_reim, fft_reim, (vector float*)fft_W, cb.fft_points_log2);
        add_to_spectrum();

        num_ffts_done++; 
        total_ffts_done++;

        /* integrated enough? */
        if (num_ffts_done == cb.integrate_ffts) {

           /* write to main RAM, advance in RAM to next target buf area */
           store_data(cb.spectrum_dst, pow_reim_accu, fft_bytelen, TAG_SPECTR);
           cb.spectrum_dst.ull += fft_bytelen;
           num_spectra_returned++;
           wait_for_tag(TAG_SPECTR);

           /* clear old */
           reset_spectrum();
           num_ffts_done = 0;
        }

        /* advance, flip buffers */
        buf = next_buf;
    }

    /* write result */
    // store_data(cb.spectrum_dst, (void*)pow_reim_accu, 0, fft_bytelen, TAG_A);
    // wait_for_tag(TAG_A);

    /* stop execution time measurement, return statistics to PPU */
    t_spu = t_start - spu_read_decrementer();
    spu_write_out_mbox(t_spu);
    spu_write_out_mbox(num_spectra_returned);
    spu_write_out_mbox(total_ffts_done);

    goto repeat;
    return -1;
}


// ------------------------------------------------------------------------------------------------
//    C A L C U L A T I O N   H E L P E R S
// ------------------------------------------------------------------------------------------------

// Single 4-tap FIR block
inline vector float FIRtap4block(vector float oldx, vector float newx, vector float b) {
    // b: [b0 b1 b2 b3]
    vector float acc;
    vector float xshifted[4];
    xshifted[0] = oldx;
    xshifted[1] = vec_sld(oldx, newx, 4);
    xshifted[2] = vec_sld(oldx, newx, 8);
    xshifted[3] = vec_sld(oldx, newx, 12);
            // instead of extract+splat perhaps "spu_shuffle(b, b, SHREP_VEC_ELEM(0..3))"
    acc = spu_madd(spu_splats(spu_extract(b,0)), xshifted[0], VEC_4f(0.0));
    acc = spu_madd(spu_splats(spu_extract(b,1)), xshifted[1], acc);
    acc = spu_madd(spu_splats(spu_extract(b,2)), xshifted[2], acc);
    acc = spu_madd(spu_splats(spu_extract(b,3)), xshifted[3], acc);
    return acc;
}

// ------------------------------------------------------------------------------------------------
//    S P E C T R U M   T A M P E R I N G
// ------------------------------------------------------------------------------------------------

// Set integrated power spectrum to zero
inline void reset_spectrum() {
   unsigned int i;
   for (i=0; i<fft_veclen; i++) {
      pow_reim_accu[i] = VEC_4f(0.0);
   }
}

// Add autocorrelation of new data to integrated power spectrum
inline void add_to_spectrum() {
   unsigned int i;
   vector float* src = fft_reim;
   vector float* dst = pow_reim_accu;
   vector float  re, im, w;
   for (i=0; i<fft_veclen; i++) {
      w = *src;
      // (1/K) * {Re,Im,Re,Im}
      w = spu_mul(w, scaler);
      // {Re,Im,Re,Im} -> {Re,0,Re,0}
      re = spu_shuffle(w, (VEC_4f(0.0)), ((vector unsigned char){0x00,0x01,0x02,0x03, 0x10,0x10,0x10,0x10, 0x08,0x09,0x0A,0x0B, 0x10,0x10,0x10,0x10}));
      // {Re,Im,Re,Im} -> {Im,0,Im,0}
      im = spu_shuffle(w, (VEC_4f(0.0)), ((vector unsigned char){0x04,0x05,0x06,0x07, 0x10,0x10,0x10,0x10, 0x0C,0x0D,0x0E,0x0F, 0x10,0x10,0x10,0x10}));
      // {pwr,0,pwr,0}
      *dst = spu_madd(re, re, spu_madd(im, im, *dst));
      src++; dst++;
   }
}

// ------------------------------------------------------------------------------------------------
//    F F T   T W I D D L E   F A C T O R S
// ------------------------------------------------------------------------------------------------

void calc_fft_twiddle() {
   unsigned int i;
   unsigned int N = cb.fft_points;
   for (i=0; i<N/4; i++) {
      fft_W[i].re = cos(i * 2*M_PI/N);
      fft_W[N/4 - i].im = -fft_W[i].re;
   }
}

// ------------------------------------------------------------------------------------------------
//    W I N D O W   F U N C T I O N S
// ------------------------------------------------------------------------------------------------

void calc_cos_window_function() {
   unsigned int i;
   vector float arg   = (vector float){0, 1.0, 2.0, 3.0}; 
   vector float inc   = VEC_4f(4.0);
   vector float scale = VEC_4f( M_PI/((float)cb.fft_points-1) );

   vector float* wf = windowfunc;
   arg = spu_mul(arg, scale);
   inc = spu_mul(inc, scale);

   for (i=0; i<re_veclen; i++) {
      *wf = sinf4(arg);
      *wf = spu_mul(*wf, *wf); // for cos^2() win function
      arg = spu_add(arg, inc);
      wf++;
   }

   return;
}

// ------------------------------------------------------------------------------------------------
//    R A W   T O   F L O A T
// ------------------------------------------------------------------------------------------------

inline void unpack_windowed_to_fft(vector unsigned char* in, vector float* out) {
   vector float* reim = out;
   vector float* wf   = windowfunc;
   vector float  reals;
   vector unsigned int integers;
   vector unsigned int int_zero = (vector unsigned int){0,0,0,0};
   unsigned int i, j;

   for (i=0; i<cb.fft_points/16; i++) {
      // 16 bytes -> 4x4 samples
      for (j=0; j<4; j++) {
          // four samples from LSB
          integers = (vector unsigned int)  
                      spu_shuffle(*in, (vector unsigned char)int_zero,
                                 ((vector unsigned char) { 0x10,0x10,0x10,0x00, 0x10,0x10,0x10,0x01, 0x10,0x10,0x10,0x02, 0x10,0x10,0x10,0x03 } ));
          reals = spu_convtf(integers, /* *1:2^scale */ 0);
          reals = spu_sub(reals, (VEC_4f(127.5)));
          reals = spu_mul(reals, *wf);
          wf++;
          *reim = spu_shuffle(reals, VEC_4f(0.0), ((vector unsigned char){0,1,2,3, 0x10,0x10,0x10,0x10, 4,5,6,7, 0x10,0x10,0x10,0x10 }));
          reim++;
          *reim = spu_shuffle(reals, VEC_4f(0.0), ((vector unsigned char){8,9,0xA,0xB, 0x10,0x10,0x10,0x10, 0xC,0xD,0xE,0xF, 0x10,0x10,0x10,0x10 }));
          reim++;

          // shift down more to LSB
          *in = spu_shuffle(*in, *in, ((vector unsigned char){4,5,6,7, 8,9,0xA,0xB, 0xC,0xD,0xE,0xF, 0,0,0,0 }));
      }
      in++;
   }

}
