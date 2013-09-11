#ifndef TASKCOREFFTW_H
#define TASKCOREFFTW_H
/************************************************************************
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
 **************************************************************************/

#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cmath>

using namespace std;

#include "Buffer.h"
#include "Settings.h"
#include "TaskCore.h"
#include "Helpers.h"

// Cell FFTW 3.2alpha:
//   http://www.fftw.org/download.html
//   ./configure --enable-cell --enable-single --enable-altivec
//   make ; sudo make install   (into /usr/local/lib/ and /usr/local/include/)
//   
#include <fftw3.h>
typedef float fftw_real; 
typedef struct _fftwu_complex_t {
   fftw_real re, im;
} fftwu_complex;

#include <pthread.h>

#define STAGE_NONE    0
#define STAGE_RAWDATA 1
#define STAGE_FFTDONE 2
#define STAGE_EXIT    3

/**
  * class TaskCore
  * This class does the actual computations for all input data in one
  * Bufferpartition. Output data is placed into another Bufferpartition. For Cell
  * processors, TaskCore will just wrap an SPE process. On other processors,
  * TaskCore contains the actual computation.
  */

class TaskCoreFFTW : public TaskCore
{
public:
   TaskCoreFFTW(int mrank) { rank = mrank; }

   /**
    * Do all necessary initializations
    * @param  settings  Pointer to the global settings
    */
   void prepare(swspect_settings_t* settings);

   /**
    * Perform the spectrum computations.
    * @return int      Returns 0 when task was successfully started
    * @param  inbuf    Pointers to raw input Buffer(s)
    * @param  outbuf   Pointers to spectrum output Buffer(s)
    * @param  xpolbuf  Pointers to cross-spectrum output Buffer(s)
    */
   int run(Buffer** inbuf, Buffer** outbuf, Buffer** xpolbuf);

   /**
    * Actual spectrum calculation. Call only from worker thread.
    */   
   void doMaths();

   /**
    * Wait for computation to complete.
    * @return int     Returns -1 on failure, or the number >=0 of output
    *                 buffer(s) that contain a complete spectrum
    */
   int join();

   /**
    * Clean up all allocations etc
    * @return int     Returns 0 on success
    */
   int finalize();

   /**
    * Get core rank
    * @return int     Rank
    */
   int getRank() { return rank; }

   /**
    * Sum own spectral data to the provided output buffer.
    * @return int     Returns 0
    * @param  outbuf  Pointer to spectrum output Buffer[], the first part of the array are
    *                 direct auto-spectra from each input source, the last part are cross-pol spectra
    */
   int combineResults(Buffer** outbuf);

  /**
    * Reset buffer to 0.0 floats
    * @param buf     Buffer to reset
    */
   void resetBuffer(Buffer* buf);

private:
   swspect_settings_t* cfg;
   int                 rank;
   fftw_real*          windowfct;
   fftw_real*          unpacked_re;
   fftw_real**         fft_result_reim;
   fftw_real*          fft_result_reim_conj;

   fftw_real**         fft_accu_reim;
   int                 num_ffts_accumulated;
   int                 num_spectra_calculated;

   fftw_real*          spectrum_scalevector;
   
   double              total_runtime;
   long                total_ffts;

   fftwf_plan*         fftwPlans;

   pthread_t           wthread;
   bool                terminate_worker;
   Buffer**            buf_in; 
   Buffer**            buf_out;
   Buffer**            bufxpol_out;

public:
   pthread_mutex_t     mmutex;
   int                 processing_stage;

   bool                shouldTerminate() { return terminate_worker; }

private:
   /**
    * Reset the currently integrated spectrum to 0
    */
   void reset_spectrum();
};



/*
 *
 * Poor mans "vector" functions
 *
 */

inline void vecSet(fftw_real val, fftw_real *ptr, int len) {
   for (int i=0; i<len; i++) ptr[i] = val;
}

inline void vecZero(fftw_real *ptr, int len) {
   for (int i=0; i<len; i++) ptr[i] = 0;
}

inline void vecAdd_Complex_I(fftwu_complex *fromB, fftwu_complex *intoA, int len) {
   for (int i=0; i<len; i++) {
      intoA[i].re += fromB[i].re;
      intoA[i].im += fromB[i].im;
   }
}

inline void vecMul_I(fftw_real *withB, fftw_real *intoA, int len) {
   for (int i=0; i<len; i++) {
      intoA[i] *= withB[i];
   }
}

inline void vecConj(fftwu_complex *in, fftwu_complex *out, int len) {
   for (int i=0; i<len; i++) {
      out[i].re = in[i].re;
      out[i].im = -in[i].im;
   } 
}

inline void vecAddProduct_Complex(fftwu_complex *A, fftwu_complex *B, fftwu_complex *accu, int len) {
  for(int i=0;i <len; i++) {
     accu[i].re = (A[i].re * B[i].re) - (A[i].im * B[i].im);
     accu[i].im = (A[i].im * B[i].re) + (A[i].re * B[i].im);
  }
}

#endif // TASKCOREFFTW_H
