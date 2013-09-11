/************************************************************************
 * IBM Cell / Intel Software Spectrometer
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
 * Spectrum calculations for the Intel Performance Primitives v5.3 series
 * Set your enviromnent variable 'IPP_PATH' to /opt/intel/ipp/...etc/
 *
 * Computation is performed asynchronously in a single worker thread.
 * To use multiple CPU cores, create multiple TaskCoreIPP objects.
 *
 **************************************************************************/

#include "TaskCoreIPP.h"
#include <cmath>
#include <iostream>
#include <sstream>
using std::cerr;
using std::endl;
using std::flush;
#include <memory.h>
#include <malloc.h>

void* taskcoreipp_worker(void* p);

// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
//   H E L P E R S
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------

/**
 * Print out a complex vector. Mainly for debugging.
 */
void print_vec32fc(Ipp32fc* v, int pts)
{
   for (int i=0; i<pts; i++) {
      cerr << v[i].re << "+i" << v[i].im << " \t";
   }
   cerr << endl << flush;
}


// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
//   T A S K   M A N A G I N G
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------

/**
 * Do all necessary initializations
 * @param  settings  Pointer to the global settings
 */
void TaskCoreIPP::prepare(swspect_settings_t* settings)
{
   IppStatus status;

   /* IPP init if libs are statically linked (shouldn't harm to call it several times...) */
   ippStaticInit();
   std::ostream* log = settings->tlog;

   /* get a new raw data unpacker object */
   this->unpacker = DataUnpackerFactory::getDataUnpacker(settings);
   if (this->unpacker == NULL) {
       *log << "No suitable data unpacker found!" << endl;
       return;
   }

   /* get settings and prepare buffers */
   this->cfg                  = settings;
   this->windowfct            = (Ipp32f*)memalign(128, sizeof(Ipp32f)*cfg->fft_points);
   this->windowgct            = (Ipp32fc*)memalign(128, sizeof(Ipp32f)*cfg->fft_points*2);
   this->unpacked_re          = (Ipp32f*)memalign(128, sizeof(Ipp32f)*cfg->fft_points);
   this->fft_conj_reim        = (Ipp32fc*)memalign(128, sizeof(Ipp32fc)*cfg->fft_ssb_points);
   this->pcal_rotatevec       = (Ipp32fc*)memalign(128, sizeof(Ipp32fc)*cfg->pcal_rotatorlen);
   this->pcal_rotated         = (Ipp32fc*)memalign(128, sizeof(Ipp32fc)*cfg->pcal_rotatorlen);
   this->fft_result_reim      = new Ipp32fc*[cfg->num_sources];
   this->fft_powspec          = new Ipp32f*[cfg->num_sources];
   for (int s=0; s<cfg->num_sources; s++) {
      this->fft_result_reim[s]  = (Ipp32fc*)memalign(128, sizeof(Ipp32fc)*cfg->fft_ssb_points);
      this->fft_powspec[s]      = (Ipp32f*) memalign(128, sizeof(Ipp32f) *cfg->fft_ssb_points);
   }

   this->num_ffts_accumulated   = 0;
   this->num_spectra_calculated = 0;
   this->processing_stage       = STAGE_NONE;
   this->total_runtime          = 0.0;
   this->total_ffts             = 0;
   this->buf_out                = NULL;
   this->bufxpol_out            = NULL;

   /* precompute the windowing function */
   generate_windowfunction(windowfct, cfg->wf_type, cfg->fft_points);

   /* precompute the windowing function for the Costas loop */
   generate_costaswindow(windowgct, cfg->samplingfreq, cfg->fft_points);

   /* FFT setup */
   int fftWorkbufferSize = 0;
   status = ippsDFTInitAlloc_R_32f(&fftSpecHandle, (int)cfg->fft_points, IPP_FFT_DIV_INV_BY_N, ippAlgHintFast);
   if (status != ippStsNoErr) {
      *log << "ippsDFT init failed: " << status << " " << ippGetStatusString(status) << endl;
   }
   ippsDFTGetBufSize_R_32f(fftSpecHandle, &fftWorkbufferSize);
   fftWorkbuffer = (Ipp8u*)memalign(128, fftWorkbufferSize);

   /* prepare the fixed scale/normalization factor for the integrated spectrum */
   spectrum_scale_Re = Ipp32f(1.0/cfg->core_overlapped_ffts);
   spectrum_scale_ReIm.re = spectrum_scale_Re;
   spectrum_scale_ReIm.im = 0.0;

   /* prepare the detection of phase calibration tones */
   if (cfg->extract_PCal) {
      double dphi = 2*M_PI * (-cfg->pcaloffsethz/cfg->samplingfreq);
      for (int i=0; i<(cfg->pcal_rotatorlen); i++) {
         double arg = dphi * double(i);
         this->pcal_rotatevec[i].re = Ipp32f(cos(arg));
         this->pcal_rotatevec[i].im = Ipp32f(sin(arg));
      }
   }

   /* init mutexeds and start the worker thread */
   terminate_worker = false;
   pthread_mutex_init(&mmutex, NULL);
   pthread_mutex_lock(&mmutex);
   pthread_create(&wthread, NULL, taskcoreipp_worker, (void*)this);

   return;
}

/**
 * Perform the spectrum computations.
 * @return int      Returns 0 when task was successfully started
 * @param  inbuf    Pointers to raw input Buffer(s)
 * @param  outbuf   Pointers to spectrum output Buffer(s)
 * @param  xpolbuf  Pointers to cross-spectrum output Buffer(s)
 * @param  pcalbuf  Pointers to phasecal output BUffer(s)
 */
int TaskCoreIPP::run(Buffer** inbuf, Buffer** outbuf, Buffer** xpolbuf, Buffer** pcalbuf)
{
   terminate_worker             = false;
   this->buf_in                 = inbuf;
   this->buf_out                = outbuf;
   this->bufxpol_out            = xpolbuf;
   this->bufpcal_out            = pcalbuf;
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
int TaskCoreIPP::join()
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
int TaskCoreIPP::finalize() 
{ 
   std::ostream* log = cfg->tlog;
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

   *log << "IPP core " << rank << " completed: " 
        << total_runtime << "s total internal calculation time, " 
        << total_ffts << " FFTs" << endl << flush; 

   ippsDFTFree_R_32f(fftSpecHandle);
   free(fftWorkbuffer);

   free(windowfct);
   free(windowgct);
   free(unpacked_re);
   free(fft_conj_reim);

   for (int s=0; s<cfg->num_sources; s++) {
      free(fft_result_reim[s]);
      free(fft_powspec[s]);
   }
   delete fft_result_reim;
   delete fft_powspec;

   free(pcal_rotatevec);
   free(pcal_rotated);

   return 0; 
}


/**
 * Worker thread. When woken up by a TaskCore object (TaskCore
 * releases a mutex), calls back to the TaskCore spectrum calc
 * function.
 * @param  p   Pointer to the TaskCore that created the thread
 */

void* taskcoreipp_worker(void* p)
{
   TaskCoreIPP* host = (TaskCoreIPP*)p; 

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
 * Reset the buffers with integrated spectra to 0
 */
void TaskCoreIPP::reset_spectrum()
{
   if ((this->buf_out != NULL) && (this->bufxpol_out != NULL)) {
      for (int s=0; s<cfg->num_sources; s++) {
         resetBuffer(this->buf_out[s]);
      }
      for (int x=0; x<cfg->num_xpols; x++) {
         resetBuffer(this->bufxpol_out[x]);
      }
   }
   if (cfg->extract_PCal && (this->bufpcal_out != NULL )) {
      for (int s=0; s<cfg->num_sources; s++) {
         resetBuffer(this->bufpcal_out[s]);
      }
   }
   this->num_ffts_accumulated = 0;
}

/**
 * Reset buffer to 0.0 floats
 * @param buf     Buffer to reset
 */
void TaskCoreIPP::resetBuffer(Buffer* buf) 
{
   ippsZero_32f((Ipp32f*) buf->getData(), buf->getAllocated() / sizeof(Ipp32f));
}


/**
 * Sum own spectral data to the provided output buffer.
 * @return int     Returns 0
 * @param  outbuf  Pointer to spectrum output Buffer[], the first part of the array are
 *                 direct auto-spectra from each input source, the last part are cross-pol spectra
 * @param  pcalbuf Pointer to phasecal output Buffer[], the array has direct phasecal spectra
 *                 from each input source
 */
int TaskCoreIPP::combineResults(Buffer** outbuf, Buffer** pcalbuf)
{
   /* checks */
   if ((outbuf == NULL) || (this->buf_out == NULL) || (pcalbuf == NULL) || (this->bufpcal_out == NULL)) {
      cerr << "TaskCoreIPP combine : null buffer(s)!" << endl; // TODO: check individual buffers?
      return 0;
   }

   /* add per-source autocorrelation data to the common result set */
   for (int s=0; s<cfg->num_sources; s++) {
      ippsAdd_32f_I( (Ipp32f*)buf_out[s]->getData(),
                     (Ipp32f*)outbuf[s]->getData(),
                      cfg->fft_ssb_points );
      outbuf[s]->setLength(sizeof(Ipp32f)*cfg->fft_ssb_points);
   }

   /* add cross-correlation data to the common result set */
   for (int x=0; x<cfg->num_xpols; x++) {
      int xo = x + cfg->num_sources;
      ippsAdd_32fc_I( (Ipp32fc*)bufxpol_out[x]->getData(), 
                      (Ipp32fc*)outbuf[xo]->getData(), 
                      cfg->fft_ssb_points );
      outbuf[xo]->setLength(sizeof(Ipp32f)*cfg->fft_ssb_points);
   }

   /* add phasecal results to the common result set */
   if (cfg->extract_PCal) {
      for (int s=0; s<cfg->num_sources; s++) {
          ippsAdd_32fc_I( (Ipp32fc*)bufpcal_out[s]->getData(),
                          (Ipp32fc*)pcalbuf[s]->getData(),
                          cfg->pcal_tonebins );
          pcalbuf[s]->setLength(sizeof(Ipp32fc)*cfg->pcal_tonebins);
      }
   }

   /* clear all data in the output work buffers */
   reset_spectrum();
   return 0;
}


/**
 * Perform the spectrum computations.
 */
void TaskCoreIPP::doMaths()
{
   double times[4];
   int curr_ffts = 0;
   std::ostream* log = cfg->tlog;

   Ipp32f** channels; 
   channels = new Ipp32f*[1]; 
   channels[0] = unpacked_re;

   /* get tidier input and output buffer pointers for "ptr++" advancing */
   char**    src      = new char*   [cfg->num_sources];
   Ipp32f**  out_auto = new Ipp32f* [cfg->num_sources];
   Ipp32fc** out_xpol = new Ipp32fc*[cfg->num_xpols];
   Ipp32fc** out_pcal = new Ipp32fc*[cfg->num_sources];

   size_t*   raw_remaining = new size_t[cfg->num_sources];
   for (int s=0; s<cfg->num_sources; s++) {
      src[s]           = buf_in[s]->getData();
      out_auto[s]      = (Ipp32f*) (buf_out[s]->getData());
      out_pcal[s]      = (Ipp32fc*)(bufpcal_out[s]->getData());
      raw_remaining[s] = buf_in[s]->getLength();
   }
   for (int x=0; x<cfg->num_xpols; x++) {
      out_xpol[x]      = (Ipp32fc*)(bufxpol_out[x]->getData());
   }
   size_t min_raw_remaining = raw_remaining[0];
   for (int s=0; s<cfg->num_sources; s++) {
      min_raw_remaining = std::min(min_raw_remaining, raw_remaining[s]);
   }

   /* clear our old results */
   reset_spectrum();

   /* start performance timing */
   times[1] = 0.0; times[2] = 0.0; times[3] = 0.0;
   times[0] = Helpers::getSysSeconds();

   /* calculate full or partial spectrum */
   while (min_raw_remaining >= cfg->raw_fullfft_bytes) {

      IppStatus status;

      times[2] = Helpers::getSysSeconds();

      /* windowed FFT for every source */
      for (int rs=0; rs<(cfg->num_sources); rs++) {

         /* unpack the samples */
         if (rs == 0) {
             unpacker->extract_samples(src[rs], unpacked_re, cfg->fft_points, cfg->use_channel_file1);
         } else {
             unpacker->extract_samples(src[rs], unpacked_re, cfg->fft_points, cfg->use_channel_file2);
         }

         /* advance the data but keep some overlap */
         src[rs] += cfg->raw_overlap_bytes;

         /* detect phase calibration tones on non-overlapped input data sets */
         if (cfg->extract_PCal) {
            if ((curr_ffts % cfg->fft_overlap_factor) == 0) {
                 extract_PCal(unpacked_re, out_pcal[rs], cfg->fft_points);
            }
         }
          /* Costas loop */
          if (cfg->costas_loop) {
          //    status = ippsMul_32fc(windowgct, unpacked_re, cfg->fft_points);
          }
         // ippsReal_32fc(unpacked_re, output, points);
         // ippsSqr_32f(temput,output,points);

         /* window the data */
         status = ippsMul_32f_I(windowfct, unpacked_re, cfg->fft_points);

         /* FFT */
         status = ippsDFTFwd_RToPerm_32f(unpacked_re, (Ipp32f*)fft_result_reim[rs], fftSpecHandle, fftWorkbuffer);
         if(status != ippStsNoErr) {
            *log << "ippsDFT compute error: " << status << " " << ippGetStatusString(status) << endl;
         }
         total_ffts++;
         curr_ffts++;

         /* autocorrelate */
         Ipp32f re0  = fft_result_reim[rs][0].re; // DC
         Ipp32f reN2 = fft_result_reim[rs][0].im; // Packed Nyquist
         status = ippsPowerSpectr_32fc((Ipp32fc*)fft_result_reim[rs], fft_powspec[rs], cfg->fft_ssb_points-1);

         /* accumulate, take DC and Nyquist into account separately */
         Ipp32f accRe0  = out_auto[rs][0]                     + re0*re0;
         Ipp32f accReN2 = out_auto[rs][cfg->fft_ssb_points-1] + reN2*reN2;
         status = ippsAdd_32f_I(fft_powspec[rs], out_auto[rs], cfg->fft_ssb_points-1);
         out_auto[rs][0] = accRe0;
         out_auto[rs][cfg->fft_ssb_points-1] = accReN2;

      }// all sources

      times[3] = (Helpers::getSysSeconds() - times[2]) + times[3];

      /* raw_overlap_bytes taken from all sources */
      min_raw_remaining -= cfg->raw_overlap_bytes;

      /* accumulate cross-spectrum between all sources */
      int xp_i=0;
      for (int xp=0; xp<cfg->num_xpols; xp++) {

         /* get the next source-source pair, ignore permutations e.g. {0,1}=={1,0} */
         int xp_j = xp_i + 1;
         if (xp_j > cfg->num_sources) {
            xp_j = (++xp_i) + 1;
         }
         // cerr << " xpol pair {"<<xp_i<<","<<xp_j<<"} " << endl << flush;

         /* get DC and Nyquist */
         Ipp32f re0_srcI  = fft_result_reim[xp_i][0].re; // DC
         Ipp32f reN2_srcI = fft_result_reim[xp_i][0].im; // Packed Nyquist
         Ipp32f re0_srcJ  = fft_result_reim[xp_j][0].re; // DC
         Ipp32f reN2_srcJ = fft_result_reim[xp_j][0].im; // Packed Nyquist
         Ipp32f old_re0acc  = out_xpol[xp][0].re;
         Ipp32f old_reN2acc = out_xpol[xp][cfg->fft_ssb_points-1].re;

         /* accumulate the product of source I and the conjugate of source J */
         Ipp32fc *dsrcI = (Ipp32fc*)fft_result_reim[xp_i];
         Ipp32fc *dsrcJ = (Ipp32fc*)fft_result_reim[xp_j];
         status = ippsConj_32fc(dsrcJ, fft_conj_reim, cfg->fft_ssb_points-1);
         status = ippsAddProduct_32fc(dsrcI, fft_conj_reim, out_xpol[xp], cfg->fft_ssb_points-1);

         /* accumulate DC and Nyquist */
         out_xpol[xp][0].re = old_re0acc + re0_srcI*re0_srcJ;
         out_xpol[xp][0].im = 0;
         out_xpol[xp][cfg->fft_ssb_points-1].re = old_reN2acc + reN2_srcI*reN2_srcJ;
         out_xpol[xp][cfg->fft_ssb_points-1].im = 0;

      }

      /* when enough overlapped FFTs have been integrated, store the results */
      num_ffts_accumulated++; // per source
      if (num_ffts_accumulated >= cfg->core_overlapped_ffts) {
         for (int rs=0; rs<cfg->num_sources; rs++) {
            status = ippsMulC_32f_I(spectrum_scale_Re, out_auto[rs], cfg->fft_ssb_points);
            out_auto[rs] += cfg->fft_ssb_points;
         }
         for (int xp=0; xp<cfg->num_xpols; xp++) {
            status = ippsMulC_32fc_I(spectrum_scale_ReIm, out_xpol[xp], cfg->fft_ssb_points);
            out_xpol[xp] += cfg->fft_ssb_points;
         }
         for (int rs=0; rs<cfg->num_sources; rs++) {
            out_pcal[rs] += cfg->pcal_tonebins;
         }
         num_spectra_calculated++;
         num_ffts_accumulated = 0;

         /* hop over the overlapping remainder between integrated spectra boundaries */
         for (int rs=0; rs<cfg->num_sources; rs++) {
            src[rs] += (cfg->raw_fullfft_bytes - cfg->raw_overlap_bytes);
         }
         min_raw_remaining -= (cfg->raw_fullfft_bytes - cfg->raw_overlap_bytes);
      }

   }// source buffer(s) have raw bytes left

   /* end performance timing */
   times[1] = Helpers::getSysSeconds();
   total_runtime += (times[1]-times[0]);

   if (num_ffts_accumulated > 0) {
      *log << "IPP core " << rank << " write partial: " << num_ffts_accumulated << " unused FFTs, "
           << "this should not happen except at early EOF!" << endl <<  flush;
      /* Write partial spectra as well (potential "bug" for multicore combining though...) */
      num_spectra_calculated++;
   }

   /* Set length of output buffers to match nr of written spectra */
   for (int rs=0; rs<cfg->num_sources; rs++) {
      buf_out[rs]->setLength(cfg->fft_bytes_ssb * num_spectra_calculated);
   }
   for (int xp=0; xp<cfg->num_xpols; xp++) {
      bufxpol_out[xp]->setLength(cfg->fft_bytes_xpol * num_spectra_calculated);
   }

   /* Output the PCal results */
   if (cfg->extract_PCal) {
      for (int pc=0; pc<cfg->num_sources; pc++) {
         bufpcal_out[pc]->setLength(cfg->pcal_result_bytes * num_spectra_calculated);
         // cerr << "PCal of source " << pc << " " << cfg->pcal_tonebins << " bins: ";
         // print_vec32fc(out_pcal[pc], cfg->pcal_tonebins);
      }
   }

   #if 0
   std::ostringstream stats;
   double dT   = (times[1]-times[0]);
   double Msps = 1e-6 * (num_spectra_calculated * cfg->core_averaged_ffts * cfg->fft_points) / dT;
   double Mbps = Msps * 8.0 * cfg->rawbytes_per_channelsample;
   stats << "[core" << rank
                    << " spectra=" << num_spectra_calculated
                    << " leftovers=" << num_ffts_accumulated
                    << " ffts=" << curr_ffts
                    << " time=" << dT << "s" 
                    << " goodput=" << Msps << "Ms/s"
                    << " " << Mbps << "Mbit/s"
                    << " subtime=" << times[3] << "s]" 
                    << endl;
   *log << stats.str() << flush;
   #endif
   return;
}


/**
 * Fill out a vector with the specified window function.
 * @param output where to place the values
 * @param type   type of window function
 * @param points how many points
 */
void TaskCoreIPP::generate_windowfunction(Ipp32f* output, WindowFunctionType type, int points)
{
   switch (type) {
        case None:
            // no windowing: currently implemented as *1.0 rectangular windowing...
            ippsSet_32f(1, output, points);
            break;
        case Cosine:
            for (int i=0; i<points; i++) {
                output[i] = cos( M_PI * (Ipp32f(i) - 0.5*Ipp32f(points) + 0.5) / Ipp32f(points));
            }
            break;
        case Cosine2:
            for (int i=0; i<points; i++) {
                Ipp64f w = cos( M_PI * (Ipp32f(i) - 0.5*Ipp32f(points) + 0.5) / Ipp32f(points));
                output[i] = w*w;
            }
            break;
        case Hamming:
            ippsSet_32f(1, output, points);
            ippsWinHamming_32f_I(output, points);
            break;
        case Blackman:
            ippsSet_32f(1, output, points);
            ippsWinBlackman_32f_I(output, points, 0.5 /* alpha, hardcoded for now... */);
            break;
        case Hann:
            ippsSet_32f(1, output, points);
            ippsWinHann_32f_I(output, points);
            break;
        default:
            ippsSet_32f(1, output, points);
            cerr << "TaskCore: unsupported window function " << type << ", reverting to None." << endl;
            break;
   }
   return;
}

/**
 * Fill out a vector with the specified window function for the Costas loop.
 * @param output where to place the values
 * @param sfreq   sampling frequency
 * @param points how many points
 */
void TaskCoreIPP::generate_costaswindow(Ipp32fc* output, int sfreq, int points)
{
    Ipp32fc casa = {0,1};
   // Ipp32fc* soc = &casa;
  //  Ipp32f test;

    for (int i=0; i<points; i++) {
        // ippsMulC_32fc(const Ipp32fc* pSrc, Ipp32fc val, Ipp32fc* pDst, int len);
     //   test = ipp32f(points);
//        ippsMulC_32fc(soc, Ipp32fc(sfreq * i / 4), output[i],2); // (sqrt(-1)* Ipp32f(sfreq) * Ipp32f(i)) / 4;
    }
    return;
}

/**
 * Process samples and accumulate the detected phase calibration tone vector.
 * @param data    input samples
 * @param pcal    output pcal vector to which data will be accumulated
 * @param points  number of samples
 */
void TaskCoreIPP::extract_PCal(Ipp32f const* data, Ipp32fc* pcal, int points)
{

   /*
    * Process an amount #fft_points of input samples. 
    * The phase cal comb signal "n*1MHz+offsetHz" has a period of #pcal_rotatorlen
    * input samples. This period must be less than #fft_points.
    *
    * An additional condition is that an integer multiple (#pcal_pulses_per_fft) 
    * of #pcal_rotatorlen samples must fit evenly into the #fft_points input samples.
    *
    * Further, the #pcal_rotatorlen must be evenly divisible by twice the
    * number of pcal tones present in the input signal bandwidth.
    *
    * The final output will contain a number #pcal_pulses of bins. 
    * As an example, the output could be:
    *
    * ------------               8 MHz input bandwidth => 8 tones => 16 bins
    *             \              value of output pcal tone bins, ideal case
    *             ------------
    * 0           7         15 
    *
    * If the upper half of bins is non-zero and the lower half is not
    * a complex constant, then there has been some phase shift in the
    * input signal -- presumably the amount of phase shift is different 
    * at different tone frequencies.
    * The amount of phase shift can be detected by comparing/dividing
    * the actual output by the expected output of the ideal case.
    * It is possible to detect changes in the phase shift, too, by 
    * comparing the integrated output at say 20s intervals.
    *
    * The principle: 
    * - the phase calibration multi-tone signal is detected by multiplying 
    *   every segment of #pcal_rotatorlen input samples with an a-priori 
    *   complex-valued multitone vector 
    * - the product is further folded down into one segment that is 
    *   #pcal_pulses complex samples long
    * - each of these segments is added to an accumulator
    * - the accumulator is output and reset to zero at every boundary 
    *   of a new dynamic spectrum (say, in 20 data-second intervals)
    */

   Ipp32f const* src = data;
   for (int n=0; n<points; n+=cfg->pcal_rotatorlen, src+=cfg->pcal_rotatorlen) {
       ippsMul_32f32fc(/*A(32f)*/src, /*B(32fc)*/this->pcal_rotatevec, 
                       /*dst(32fc)*/this->pcal_rotated, 
                       /*len*/cfg->pcal_rotatorlen
       );
       Ipp32fc* pulse = this->pcal_rotated;
       for (int p=0; p<(cfg->pcal_rotatorlen/cfg->pcal_tonebins); p++) {
           ippsAdd_32fc_I(/*src*/pulse, /*srcdst*/pcal, cfg->pcal_tonebins);
           pulse += cfg->pcal_tonebins;
       }
   }

   // cerr << "PCal done: " << cfg->pcal_pulses_per_fft << " sample pieces of size " << cfg->pcal_rotatorlen
   //     << ", folded into " << cfg->pcal_tonebins << " in " << cfg->pcal_pulses << " inner-loop steps." << endl;
   // print_vec32fc(pcal, cfg->pcal_tonebins);

   return;
}



#ifdef UNIT_TEST_TCIPP
int main(int argc, char** argv)
{
   return 0;
}
#endif
