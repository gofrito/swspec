#ifndef TASKCORECELL_H
#define TASKCORECELL_H
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

#include "cell_spectrometer.h"

#include <altivec.h>

class TaskCoreCell : public TaskCore
{
public:
   TaskCoreCell(int mrank) { rank = mrank; prevBufOut = NULL; }

   /**
    * Do all necessary initializations
    * @param  settings  Pointer to the global settings
    */
   void prepare(swspect_settings_t* settings);

   /**
    * Perform the spectrum computations.
    * @return int     Returns 0 when task was successfully started
    * @param  inbuf   Pointer to raw input Buffer
    * @param  outbuf  Pointer to spectrum output Buffer
    */
   int run(Buffer* inbuf, Buffer* outbuf);

   /**
    * Wait for computation to complete.
    * @return int     Returns -1 on failure, or the number >=0 of output
    *                 buffer(s) that contain a complete spectrum
    */
   int join();

   /**
    * Tell the SPEs to exit.
    * @return int     Returns 0 on success
    */
   int finalize();

   /**
    * Sum own spectral data to the provided output buffer.
    * @return int     Returns 0
    * @param  outbuf  Pointer to spectrum output Buffer
    */
   int combineResults(Buffer* outbuf);

private:
   /* SPE thread */
   spe_gid_t        gid;
   speid_t          speid;
   volatile control_block_t  cb  _CLINE_ALIGN;
   Buffer*          prevBufOut;

   /* settings */
   swspect_settings_t* cfg;
   int                 rank;

   /* accumulative SPE statistics */
   unsigned long total_time;
   unsigned long total_spectra_count;
   unsigned long total_ffts_count;
};

#endif // TASKCORECELL_H
