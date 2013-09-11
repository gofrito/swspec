#ifndef TASKCORE_H
#define TASKCORE_H
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

#include "Settings.h"
#include <string>

class Buffer;

/**
  * class TaskCore
  * This class does the actual computations for all input data in one
  * Bufferpartition. Output data is placed into another Bufferpartition. For Cell
  * processors, TaskCore will just wrap an SPE process. On other processors,
  * TaskCore contains the actual computation.
  */

class TaskCore
{
public:
   TaskCore(int mrank) { return; }
   TaskCore() { return; }
   virtual ~TaskCore() { }

   /**
    * Do all necessary initializations
    * @param  settings  Pointer to the global settings
    */
   virtual void prepare(swspect_settings_t* settings) = 0;
//{ cerr << "TaskCore virtual" << endl; cerr.flush(); }

   /**
    * Perform the spectrum computations.
    * @return int      Returns 0 when task was successfully started
    * @param  inbuf    Pointers to raw input Buffer(s)
    * @param  outbuf   Pointers to spectrum output Buffer(s)
    * @param  xpolbuf  Pointers to cross-spectrum output Buffer(s)
    * @param  pcalbuf  Pointers to phasecal output Buffer(s)
    */
   virtual int run(Buffer** inbuf, Buffer** outbuf, Buffer** xpolbuf, Buffer** pcalbuf) = 0;

   /**
    * Wait for computation to complete.
    * @return int     Returns -1 on failure, or the number >=0 of output 
    *                 buffer(s) that contain a complete spectrum
    */
   virtual int join() { return 1; }

   /**
    * Clean up all allocations etc
    * @return int     Returns 0 on success
    */
   virtual int finalize() { return 0; }

   /**
    * Sum own spectral data to the provided output buffer.
    * @return int     Returns 0
    * @param  outbuf  Pointer to spectrum output Buffer[], the first part of the array are
    *                 direct auto-spectra from each input source, the last part are cross-pol spectra
    * @param  pcalbuf Pointer to phasecal output Buffer[], the array has direct phasecal spectra
    *                 from each input source
    */
   virtual int combineResults(Buffer** outbuf, Buffer** pcalbuf) { return 0; }

  /**
    * Reset buffer to 0.0 floats
    * @param buf     Buffer to reset
    */
   virtual void resetBuffer(Buffer* buf) { return; }

};

#endif // TASKCORE_H
