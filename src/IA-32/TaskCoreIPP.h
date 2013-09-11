#ifndef TASKCOREIPP_H
#define TASKCOREIPP_H
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
 **************************************************************************/

#include "Buffer.h"
#include "Settings.h"
#include "TaskCore.h"
#include "Helpers.h"
#include "DataUnpackerFactory.h"

#include "PhaseCal/PCal.h"

#include <ipps.h>
#include <ippcore.h>

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

class TaskCoreIPP : public TaskCore
{
public:
   TaskCoreIPP(int mrank) { rank = mrank; }

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
    * @param  pcalbuf  Pointers to phasecal output Buffer(s)
    */
   int run(Buffer** inbuf, Buffer** outbuf, Buffer** xpolbuf, Buffer** pcalbuf);

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
    * @param  pcalbuf Pointer to phasecal output Buffer[], the array has direct phasecal spectra
    *                 from each input source
    */
   int combineResults(Buffer** outbuf, Buffer** pcalbuf);

  /**
    * Reset buffer to 0.0 floats
    * @param buf     Buffer to reset
    */
   void resetBuffer(Buffer* buf);

private:

   swspect_settings_t* cfg;                           // referenced run settings
   int                 rank;                          // core ID

   DataUnpacker*       unpacker;                      // depends on the input data format

   Ipp32f*             unpacked_re;                   // input samples unpacked from raw data
   Ipp32f*             windowfct;                     // input window function
   Ipp32fc*            windowgct;                     // Costas-loop window function
   Ipp32fc**           fft_result_reim;               // full-length FFT/DFT output
   Ipp32fc*            fft_conj_reim;                 // single-sideband FFT/DFT output, complex conjugate

   Ipp32fc*            pcal_rotatevec;                // complex vector that input samples are rotated with before accumulation
   Ipp32fc*            pcal_rotated;                  // temporary processing vector

   Ipp32f**            fft_powspec;                   // fft power spectrum, temporary
   int                 num_ffts_accumulated;
   int                 num_spectra_calculated;

   Ipp32f              spectrum_scale_Re;             // normalization factor for the accumulated spectrum
   Ipp32fc             spectrum_scale_ReIm;

   double              total_runtime;
   long                total_ffts;

   IppsDFTSpec_R_32f*  fftSpecHandle;                 // Intel IPP DFT handles
   Ipp8u*              fftWorkbuffer;

   pthread_t           wthread;                       // worker thread ID
   bool                terminate_worker;              // signal to worker thread

   Buffer**            buf_in;                        // set of pointers used as a
   Buffer**            buf_out;                       // call argument for doMaths() etc
   Buffer**            bufxpol_out;
   Buffer**            bufpcal_out;

public:
   pthread_mutex_t     mmutex;
   long                processing_stage;

   bool                shouldTerminate() { return terminate_worker; }

private:

   /**
    * Reset the buffers with integrated spectra to 0
    */
   void reset_spectrum();

   /**
    * Fill out a vector with the specified window function.
    * @param output where to place the values
    * @param type   type of window function
    * @param points how many points
    */
   void generate_windowfunction(Ipp32f* output, WindowFunctionType type, int points);
   void generate_costaswindow(Ipp32fc* output, int samplingfreq, int points);
    
   /**
    * Process samples and accumulate the detected phase calibration tone vector.
    * @param data    input samples
    * @param pcal    output pcal vector to which data will be accumulated
    * @param points  number of samples
    */
   void extract_PCal(Ipp32f const* data, Ipp32fc* pcal, int points);

};

#endif // TASKCOREIPP_H
