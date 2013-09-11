#ifndef TASKCOREMKL_H
#define TASKCOREMKL_H
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
#include <cstring>

using namespace std;

#include "Buffer.h"
#include "Settings.h"
#include "TaskCore.h"
#include "Helpers.h"

#include <mkl_dfti.h>
#include <mkl_vml.h>

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

class TaskCoreMKL : public TaskCore
{
public:
   TaskCoreMKL(int mrank) { rank = mrank; }

   /**
    * Do all necessary initializations
    * @param  settings  Pointer to the global settings
    */
   void prepare(swspect_settings_t* settings);

   /**
    * Start the spectrum computations in the background.
    * @return int     Returns 0 when task was successfully started
    * @param  inbuf   Pointer to raw input Buffer
    * @param  outbuf  Pointer to spectrum output Buffer
    */
   int run(Buffer* inbuf, Buffer* outbuf);

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

private:
   swspect_settings_t* cfg;
   int                 rank;
   float*              windowfct;
   float*              unpacked_re;
   float*              fft_result_reim;

   float*              fft_accu_reim;
   int                 num_ffts_accumulated;
   
   double              total_runtime;
   long                total_ffts;

   DFTI_DESCRIPTOR_HANDLE dftDescHandle;

   pthread_t           wthread;
   bool                terminate_worker;
   Buffer*             buf_in; 
   Buffer*             buf_out;

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

#endif // TASKCOREMKL_H
