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
 *
 **************************************************************************
 *
 * Spectrum calculations for Cell using the FFTW 3.2 Cell version.
 * Compile FFTW for single or double precision and multithreading.
 *
 **************************************************************************/

#include "TaskCoreFFTW.h"
#include <cmath>

void* taskcorefft_worker(void* p);

// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
//   T A S K   M A N A G I N G
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------

/**
 * Do all necessary initializations
 * @param  settings  Pointer to the global settings
 */
void TaskCoreFFTW::prepare(swspect_settings_t* settings)
{
   /* get settings and prepare buffers */
   this->cfg                  = settings;
   this->windowfct            = (fftw_real*)memalign(128, sizeof(fftw_real)*cfg->fft_points);
   this->unpacked_re          = (fftw_real*)memalign(128, sizeof(fftw_real)*cfg->fft_points);
   this->fft_result_reim      = new fftw_real*[cfg->num_sources];
   this->fft_accu_reim        = new fftw_real*[cfg->num_sources];
   this->num_ffts_accumulated   = 0;
   this->num_spectra_calculated = 0;
   for (int s=0; s<cfg->num_sources; s++) {
      this->fft_result_reim[s] = (fftw_real*)memalign(128, sizeof(fftw_real)*cfg->fft_points); // 4*fft_points*2 if c2c dft
      this->fft_accu_reim[s]   = (fftw_real*)memalign(128, sizeof(fftw_real)*cfg->fft_points);
   }
   this->fft_result_reim_conj = (fftw_real*)memalign(128, sizeof(fftw_real)*cfg->fft_points); // 4*fft_points*2 if c2c dft
   this->spectrum_scalevector = (fftw_real*)memalign(128, sizeof(fftw_real)*cfg->fft_points);
   this->processing_stage     = STAGE_NONE;
   this->total_runtime        = 0.0;
   this->total_ffts           = 0;
   reset_spectrum();

   /* windowing function */
   for (int i=0; i<(cfg->fft_points); i++) {
      fftw_real w = sin(fftw_real(i) * M_PI/(cfg->fft_points - 1.0));
      windowfct[i] = w*w;
   }
 
   /* FFT setup */
   // fftwPlan = fftw_create_plan(cfg->fft_points, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE); // IPP_FFT_DIV_INV_BY_N, ippAlgHintFast
   fftwPlans = new fftwf_plan[cfg->num_sources];
   for (int s=0; s<cfg->num_sources; s++) {
      fftwPlans[s] = fftwf_plan_r2r_1d(cfg->fft_points, this->unpacked_re, this->fft_result_reim[s], FFTW_R2HC, FFTW_ESTIMATE);
   }

   /* prepare fixed scale/normalization factor */
   vecSet(fftw_real(1.0/cfg->integrated_ffts), spectrum_scalevector, cfg->fft_points);

   /* init mutexeds and start the worker thread */
   terminate_worker = false;
   pthread_mutex_init(&mmutex, NULL);
   pthread_mutex_lock(&mmutex);
   pthread_create(&wthread, NULL, taskcorefft_worker, (void*)this);

   return;
}

/**
 * Perform the spectrum computations.
 * @return int      Returns 0 when task was successfully started
 * @param  inbuf    Pointers to raw input Buffer(s)
 * @param  outbuf   Pointers to spectrum output Buffer(s)
 * @param  xpolbuf  Pointers to cross-spectrum output Buffer(s)
 */
int TaskCoreFFTW::run(Buffer** inbuf, Buffer** outbuf, Buffer** xpolbuf)
{
   terminate_worker             = false;
   this->buf_in                 = inbuf;
   this->buf_out                = outbuf;
   this->bufxpol_out            = xpolbuf;
   this->processing_stage       = STAGE_RAWDATA;
   this->num_spectra_calculated = 0;
   pthread_mutex_unlock(&mmutex);
   return 0;
}

/**
 * Wait for computation to complete.
 * @return int     Returns -1 on failure, or the number >=0 of output
 *                 buffer(s) that contain a complete spectrum
 */
int TaskCoreFFTW::join()
{
   pthread_mutex_lock(&mmutex);
   while (processing_stage != STAGE_FFTDONE) {
      pthread_mutex_unlock(&mmutex);
      pthread_yield();
      pthread_mutex_lock(&mmutex);
   }
   processing_stage = STAGE_NONE;
   return this->num_spectra_calculated;
}

/**
 * Clean up all allocations etc
 * @return int     Returns 0 on success
 */
int TaskCoreFFTW::finalize() 
{ 
   terminate_worker = true;
   this->processing_stage = STAGE_NONE;
   while (processing_stage != STAGE_EXIT) {
      pthread_mutex_unlock(&mmutex);
      pthread_yield();
      pthread_mutex_lock(&mmutex);
   }
   pthread_mutex_unlock(&mmutex);

   pthread_join(wthread, NULL);
   pthread_mutex_destroy(&mmutex);

   cerr << "FFTW core " << rank << " completed: " << total_runtime << "s total internal calculation time, " << total_ffts << " FFTs" << endl << flush; 

   free(windowfct);
   free(unpacked_re);

   for (int s=0; s<cfg->num_sources; s++) {
      fftwf_destroy_plan(fftwPlans[s]);
      free(fft_result_reim[s]);
      free(fft_accu_reim[s]);
   }
   delete fftwPlans;
   delete fft_accu_reim;
   delete fft_result_reim;

   free(fft_result_reim_conj);
   free(spectrum_scalevector);
   return 0; 
}


/**
 * Worker thread. When woken up by a TaskCore object (TaskCore
 * releases a mutex), calls back to the TaskCore spectrum calc
 * function.
 * @param  p   Pointer to the TaskCore that created the thread
 */

void* taskcorefft_worker(void* p)
{
   TaskCoreFFTW* host = (TaskCoreFFTW*)p; 

   while (1) {
     
      /* grab mutex i.e. wait until there is data */
      pthread_mutex_lock(&host->mmutex); 

      /* check for exiting */
      if (host->shouldTerminate()) {
          host->processing_stage = STAGE_EXIT;
          pthread_mutex_unlock(&host->mmutex); 
          break;
      }

      /* do the maths */
      if (host->processing_stage == STAGE_RAWDATA) {
          host->doMaths(); 
          host->processing_stage = STAGE_FFTDONE;
      }

      /* let main program continue */
      pthread_mutex_unlock(&host->mmutex); 
   }
   pthread_exit((void*) 0);
}


// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
//   T H E   R E A L   M A T H S
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------

/**
 * Reset the currently integrated spectrum to 0
 */
void TaskCoreFFTW::reset_spectrum()
{
   for (int s=0; s<cfg->num_sources; s++) {
      vecZero(fft_accu_reim[s], cfg->fft_points);
   }
   for (int x=0; x<cfg->num_xpols; x++) {
      // vecZero(fft_accu_reim[s], cfg->fft_points);
   }
   this->num_ffts_accumulated = 0;
}

/**
 * Reset buffer to 0.0 floats
 * @param buf     Buffer to reset
 */
void TaskCoreFFTW::resetBuffer(Buffer* buf) 
{ 
   vecZero((fftw_real*) buf->getData(), buf->getLength() / sizeof(fftw_real));
}


/**
 * Sum own spectral data to the provided output buffer.
 * @return int     Returns 0
 * @param  outbuf  Pointer to spectrum output Buffer[], the first part of the array are
 *                 direct auto-spectra from each input source, the last part are cross-pol spectra
 */
int TaskCoreFFTW::combineResults(Buffer** outbuf)
{
   if (outbuf == NULL || this->buf_out == NULL) {
      cerr << "TaskCoreFFTW combine : null buffer(s)!" << endl; // TODO: check individual buffers?
      return 0;
   }
   for (int s=0; s<cfg->num_sources; s++) {
      vecAdd_Complex_I( (fftwu_complex*)buf_out[s]->getData(), 
                        (fftwu_complex*)outbuf[s]->getData(), 
                        cfg->fft_points / 2 );
      outbuf[s]->setLength(sizeof(fftw_real)*cfg->fft_points);
   }
   for (int x=0; x<cfg->num_xpols; x++) {
      int xo = x + cfg->num_sources;
      vecAdd_Complex_I( (fftwu_complex*)bufxpol_out[x]->getData(), 
                        (fftwu_complex*)outbuf[xo]->getData(), 
                        cfg->fft_points / 2 );
      outbuf[xo]->setLength(sizeof(fftw_real)*cfg->fft_points);
   }
   reset_spectrum();
   return 0;
}


/**
 * Perform the spectrum computations.
 */
void TaskCoreFFTW::doMaths()
{
   double times[4];
   int curr_ffts = 0;

   fftw_real** channels; 
   channels = new fftw_real*[1]; 
   channels[0] = unpacked_re;

   double  rawbytes_per_channelsample = (cfg->bits_per_sample * cfg->source_channels) / 8.0;
	
   int fft_bytes     = sizeof(fftw_real) * cfg->fft_points; // *2 for c2c fft
   int raw_bytelen   = cfg->fft_points * rawbytes_per_channelsample;
   int raw_delta     = raw_bytelen / cfg->fft_overlap_factor;

   /* get tidier input and output buffer pointers for "ptr++" advancing */
   char** src = new char*[cfg->num_sources];
   char** out = new char*[cfg->num_sources];
   char** outx= new char*[cfg->num_xpols];
   int* raw_remaining = new int[cfg->num_sources];
   for (int s=0; s<cfg->num_sources; s++) {
      src[s] = buf_in[s]->getData();
      out[s] = buf_out[s]->getData();
      raw_remaining[s] = buf_in[s]->getLength();
   }
   for (int x=0; x<cfg->num_xpols; x++) {
      outx[x] = bufxpol_out[x]->getData();
   }

   /* start performance timing */
   times[1] = 0.0; times[2] = 0.0; times[3] = 0.0;
   times[0] = Helpers::getSysSeconds();

   /* calculate full or partial spectrum */
   while (raw_remaining[0] >= raw_bytelen) {

      times[2] = Helpers::getSysSeconds();

      /* windowed FFT for every source */
      for (int rs=0; rs<cfg->num_sources; rs++) {

         if (cfg->bits_per_sample == 2 && cfg->source_channels == 4) {

            /* 2-bit data, some channel */
            unsigned char *src8 = (unsigned char*)src[rs];
            for (int smp=0; smp<(cfg->fft_points); ) {
               const int unroll_factor = 8;
               unsigned char shift = (2 * cfg->use_channel);
               unsigned char mask  = (unsigned char)3 << shift;
               for (char off=0; off<unroll_factor; off++) {
                  float map[4]    = { -1.0, -3.5, +1.0, +3.5 }; // {s,m} : 00,01,10,11 : direct sign/mag bits
                  float mapRev[4] = { -1.0, +1.0, -3.5, +3.5 }; // {m,s} : 00,01,10,11 : reversed sign/mag bits
                  unsigned char c = *((unsigned char*)src8+off);
                  c = (c & mask) >> shift;
                  if (c > 3) {
                     cerr << "ups, decode error, c=" << (int)c << " shift=" << (unsigned int)shift << " mask=" << (unsigned int)mask << endl << flush;
                  }
                  unpacked_re[smp+off] = mapRev[c];
               }
               src8 += unroll_factor; smp += unroll_factor;
            }

         } else if (cfg->bits_per_sample == 8) {

            /* unpack 8-bit samples */
            char *src8 = src[rs];
            src8 += cfg->use_channel;
            for (int smp=0; smp<(cfg->fft_points); ) {
               const int unroll_factor = 8;
               for (char off=0; off<unroll_factor; off++) {
                  // unpacked_re[smp+off] = (fftw_real(*(src8+off)));
                  unpacked_re[smp+off] = -128.0 + (fftw_real) *((unsigned char*)src8 + off*cfg->source_channels);
               }
               src8 += unroll_factor*cfg->source_channels; smp += unroll_factor;
            }

         } else if (cfg->bits_per_sample == 16) {

            /* unpack 16-bit samples */
            signed short* src16 = (signed short*)src[rs];
            src16 += cfg->use_channel;
            for (int smp=0; smp<(cfg->fft_points); ) {
               const int unroll_factor = 8;
               for (char off=0; off<unroll_factor; off++) {
                  #if 1
                  unsigned char* reorder = (unsigned char*) (src16 + off*cfg->source_channels);
                  unsigned char reTmp;
                  reTmp = reorder[0]; reorder[0] = reorder[1]; reorder[1] = reTmp;
                  unpacked_re[smp+off] = (fftw_real) *((signed short*)reorder);
                  #else
                  unpacked_re[smp+off] = (fftw_real) *(src16 + off*cfg->source_channels);
                  #endif
               }
               src16 += unroll_factor*cfg->source_channels;
               smp += unroll_factor;
            }            

         } else {
            cerr << "Can't unpack, " << cfg->bits_per_sample << "-bit " << cfg->source_channels << "-channel unpack not programmed yet!" << endl;
         }

         /* advance the data but keep some overlap */
         src[rs]           += raw_delta;
         raw_remaining[rs] -= raw_delta;

         /* window the data */
         vecMul_I(windowfct, unpacked_re, cfg->fft_points);

         /* FFT */
         fftwf_execute(fftwPlans[rs]); // fftw_exec(plan, unpacked_re, fft_result_reim[rs]);

         total_ffts++;
         curr_ffts++;

         /* autocorrelate, scale before accumulation to prevent saturation (?), then accumulate */
         fftw_real re0     = fft_result_reim[rs][0];
         fftw_real reN2    = fft_result_reim[rs][1];
         vecConj((fftwu_complex*)fft_result_reim[rs], (fftwu_complex*)fft_result_reim_conj, cfg->fft_points / 2);

         fftw_real accRe0  = fft_accu_reim[rs][0] + re0*re0;
         fftw_real accReN2 = fft_accu_reim[rs][1] + reN2*reN2;

         vecAddProduct_Complex((fftwu_complex*)fft_result_reim[rs], (fftwu_complex*)fft_result_reim_conj,
                               (fftwu_complex*)fft_accu_reim[rs], cfg->fft_points / 2);
         fft_accu_reim[rs][0] = accRe0;
         fft_accu_reim[rs][1] = accReN2;

         times[3] = Helpers::getSysSeconds() - times[2] + times[3];

      }// all sources

      /* accumulate cross-spectrum between all sources */
      int xp_i=0;
      for (int xp=0; xp<cfg->num_xpols; xp++) {
         int xp_j = xp_i + 1;
         if (xp_j > cfg->num_sources) {
            xp_j = (++xp_i) + 1;
         }
         // cerr << " xpol pair {"<<xp_i<<","<<xp_j<<"} " << endl << flush;
         char* dst = bufxpol_out[xp]->getData();
         vecConj((fftwu_complex*)fft_result_reim[xp_j], (fftwu_complex*)fft_result_reim_conj, cfg->fft_points / 2);
         vecAddProduct_Complex((fftwu_complex*)fft_result_reim[xp_i], 
                               (fftwu_complex*)fft_result_reim_conj,
                               (fftwu_complex*)dst, cfg->fft_points / 2);
      }

      num_ffts_accumulated++; // per source

      if (num_ffts_accumulated >= cfg->integrated_ffts) {
         for (int rs=0; rs<cfg->num_sources; rs++) {
            vecMul_I((fftw_real*)spectrum_scalevector, (fftw_real*)fft_accu_reim[rs], cfg->fft_points);
            memcpy((void*)out[rs], (void*)fft_accu_reim[rs], fft_bytes);
            out[rs] += fft_bytes;
         }
         for (int xp=0; xp<cfg->num_xpols; xp++) {
            vecMul_I((fftw_real*)spectrum_scalevector, (fftw_real*)outx[xp], cfg->fft_points);
            outx[xp] += fft_bytes;
         }
         num_spectra_calculated++;
         reset_spectrum();
      }


   }// source buffer(s) have raw bytes left

   /* end performance timing */
   times[1] = Helpers::getSysSeconds();
   total_runtime += (times[1]-times[0]);
 
   /* write out partial same-polarization results in addition to the complete spectra */
   for (int rs=0; rs<cfg->num_sources; rs++) {
      int total_output_len = num_spectra_calculated * fft_bytes;
      // partials:
      if (num_ffts_accumulated > 0) {
         vecMul_I((fftw_real*)spectrum_scalevector, (fftw_real*)fft_accu_reim[rs], cfg->fft_points);
         memcpy((void*)out[rs], (void*)fft_accu_reim[rs], fft_bytes);
         total_output_len += fft_bytes;
      }
      // set length entires and partials:
      buf_out[rs]->setLength(total_output_len);
   }

   /* partial cross-pol results: scale */
   if (num_ffts_accumulated > 0) {
      for (int xp=0; xp<cfg->num_xpols; xp++) {
         vecMul_I((fftw_real*)spectrum_scalevector, (fftw_real*)outx[xp], cfg->fft_points);
      }
   }

   #if 0
   std::ostringstream stats;
   stats << "[core" << rank
                    << " spectra=" << num_spectra_calculated
                    << " leftovers=" << num_ffts_accumulated
                    << " ffts=" << curr_ffts
                    << " time=" << (times[1]-times[0]) << "s" 
                    << " subtime=" << times[3] << "s]" 
                    << endl;
   cerr << stats.str() << flush;
   #endif
   return;
}


#ifdef UNIT_TEST_TCFFTW
int main(int argc, char** argv)
{
   return 0;
}
#endif
