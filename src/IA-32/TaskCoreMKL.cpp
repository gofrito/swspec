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
 * Spectrum calculations for the Intel Math Kernel Library 10.0 series
 * Set your enviromnent variable 'MKL_PATH' to /opt/intel/mkl/...etc/
 *
 **************************************************************************/

#include "TaskCoreMKL.h"
#include <cmath>

void* taskcoremkl_worker(void* p);


/**
 * Do all necessary initializations
 * @param  settings  Pointer to the global settings
 */
void TaskCoreMKL::prepare(swspect_settings_t* settings)
{
   /* get settings and prepare buffers */
   this->cfg                  = settings;
   this->windowfct            = (float*)memalign(128, 4*cfg->fft_points);
   this->unpacked_re          = (float*)memalign(128, 4*cfg->fft_points);
   this->fft_result_reim      = (float*)memalign(128, 4*cfg->fft_points); // fft_points*2 if c2c dft
   this->fft_accu_reim        = (float*)memalign(128, 4*cfg->fft_points);
   this->processing_stage     = STAGE_NONE;
   this->total_runtime        = 0.0;
   this->total_ffts           = 0;
   this->num_ffts_accumulated = 0;
   reset_spectrum();

   /* windowing function */
   for (int i=0; i<(cfg->fft_points); i++) {
      float w = sin(float(i) * M_PI/(cfg->fft_points - 1.0));
      windowfct[i] = w*w;
   }
 
   /* MKL setup */
   vmlSetMode(VML_FLOAT_CONSISTENT | /*LA=low,HA=high Accuracy*/ VML_LA);

   /* FFT setup */
   MKL_LONG status = DftiCreateDescriptor(&dftDescHandle, DFTI_SINGLE, DFTI_REAL, 1, cfg->fft_points);
   if(!DftiErrorClass(status, DFTI_NO_ERROR)) {
      cerr << "Dfti create failed!" << endl;
   }
   status |= DftiSetValue(dftDescHandle, DFTI_PLACEMENT, DFTI_NOT_INPLACE);
   status |= DftiSetValue(dftDescHandle, DFTI_PACKED_FORMAT, DFTI_PERM_FORMAT);
   status |= DftiCommitDescriptor(dftDescHandle);
   if(!DftiErrorClass(status, DFTI_NO_ERROR)) {
      cerr << "Dfti setup set failed!" << endl;
   }

   /* init mutexeds and start the worker thread */
   terminate_worker = false;
   pthread_mutex_init(&mmutex, NULL);
   pthread_mutex_lock(&mmutex);
   pthread_create(&wthread, NULL, taskcoremkl_worker, (void*)this);

   return;
}

/**
 * Begin the spectrum computations in the background.
 * @return int     Returns 0 when task was successfully started
 * @param  inbuf   Pointer to raw input Buffer
 * @param  outbuf  Pointer to spectrum output Buffer
 */
int TaskCoreMKL::run(Buffer* inbuf, Buffer* outbuf)
{
   terminate_worker = false;
   this->buf_in = inbuf;
   this->buf_out = outbuf;
   this->processing_stage = STAGE_RAWDATA;
   pthread_mutex_unlock(&mmutex);
   return 0;
}

/**
 * Perform the spectrum computations.
 */
void TaskCoreMKL::doMaths()
{
   double times[4];

   char *src        = buf_in->getData();
   char *prevsrc    = src;
   int  len         = buf_in->getLength();
   char *src_end    = src + len;
   int full_spectra = 0;

   int fft_bytes = sizeof(float)*cfg->fft_points; // *2 for c2c fft
   char *out     = buf_out->getData();

   times[3] = 0.0;
   times[0] = Helpers::getSysSeconds();
   while ( (src+cfg->fft_points/cfg->samples_per_rawbyte) <= src_end) {

      // times[2] = Helpers::getSysSeconds();
      // times[3] = Helpers::getSysSeconds() - times[2] + times[3];

      /* windowed unpack (8-bit samples) */
      prevsrc = src;
      for (int s=0; s<(cfg->fft_points); ) {
         const int unroll_factor = 8;
         for (char off=0; off<unroll_factor; off++) {
            unpacked_re[s+off] = (float(*(src+off)));
         }
         src += unroll_factor; s += unroll_factor;
      }
      vsMul(cfg->fft_points, windowfct, unpacked_re, unpacked_re);
      src = prevsrc;

      /* advance in the data but keep some overlap */
      src += cfg->fft_points/(cfg->fft_overlap_factor * cfg->samples_per_rawbyte);

      /* FFT */
      MKL_LONG status = DftiComputeForward(this->dftDescHandle, 
                            (void*)&unpacked_re[0], (void*)&this->fft_result_reim[0]);
      if(!DftiErrorClass(status, DFTI_NO_ERROR)) {
         cerr << "Dfti compute error: " << status << " " << DftiErrorMessage(status) << endl;
      }

      total_ffts++;

      /* autocorrelation */
      float re0 = this->fft_result_reim[0];  re0 *= re0;
      float reN2 = this->fft_result_reim[1]; reN2 *= reN2;
      vcMulByConj(cfg->fft_points / 2, 
                  (MKL_Complex8*)fft_result_reim, (MKL_Complex8*)fft_result_reim, 
                  (MKL_Complex8*)fft_result_reim);
      fft_result_reim[0] = re0;
      fft_result_reim[1] = reN2;

      /* accumulate */
      vcAdd(cfg->fft_points / 2,
                  (MKL_Complex8*)fft_result_reim, (MKL_Complex8*)fft_accu_reim,
                  (MKL_Complex8*)fft_accu_reim);

      num_ffts_accumulated++;

      if (num_ffts_accumulated >= cfg->integrated_ffts) {
         memcpy((void*)out, (void*)fft_accu_reim, fft_bytes);
         out += fft_bytes;
         full_spectra++;
         num_ffts_accumulated -= cfg->integrated_ffts;
         reset_spectrum();
      }
   }
   buf_out->setLength(full_spectra * fft_bytes);
   times[1] = Helpers::getSysSeconds();

   total_runtime += (times[1]-times[0]);

   #if 0
   cerr << "core" << rank << " full_spectra=" << full_spectra 
                          << " leftovers=" << num_ffts_accumulated 
                          << " time=" << (times[1]-times[0]) << "s" 
                      //  << " subtime=" << times[3] << "s" 
                          << endl << flush;
   #endif
   return;
}

/**
 * Wait for computation to complete.
 * @return int     Returns -1 on failure, or the number >=0 of output
 *                 buffer(s) that contain a complete spectrum
 */
int TaskCoreMKL::join()
{
   pthread_mutex_lock(&mmutex);
   while (processing_stage != STAGE_FFTDONE) {
      pthread_mutex_unlock(&mmutex);
      pthread_yield();
      pthread_mutex_lock(&mmutex);
   }
   processing_stage = STAGE_NONE;
   return 0;
}

/**
 * Clean up all allocations etc
 * @return int     Returns 0 on success
 */
int TaskCoreMKL::finalize() 
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

   cerr << "MKL core " << rank << " completed: " << total_runtime << "s total internal calculation time, " << total_ffts << " FFTs" << endl << flush; 

   DftiFreeDescriptor(&dftDescHandle);
   free(windowfct);
   free(unpacked_re);
   free(fft_result_reim);
   free(fft_accu_reim);
   return 0; 
}

/**
 * Reset the currently integrated spectrum to 0
 */
void TaskCoreMKL::reset_spectrum()
{
   int fft_bytes = sizeof(float)*cfg->fft_points;
   memset((void*)fft_accu_reim, 0, fft_bytes);
}


/**
 * Worker thread. When woken up by a TaskCore object (TaskCore
 * releases a mutex), calls back to the TaskCore spectrum calc
 * function.
 * @param  p   Pointer to the TaskCore that created the thread
 */

void* taskcoremkl_worker(void* p)
{
   TaskCoreMKL* host = (TaskCoreMKL*)p; 

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


#ifdef UNIT_TEST_TCIPP
int main(int argc, char** argv)
{
   return 0;
}
#endif
