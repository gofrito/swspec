#ifndef TASKDISPATCHER_H
#define TASKDISPATCHER_H
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

#include "Settings.h"
#include "TaskCore.h"

#define VERBOSE 0

#if defined(INTEL_IPP)
   #include "TaskCoreIPP.h"
   #define Platform_TaskCore TaskCoreIPP
#elif defined(IBM_CELL)
   #if defined(CELL_USER_SPE)
   #include "TaskCoreCell.h"
   #define Platform_TaskCore TaskCoreCell
   #else
   #include "TaskCoreFFTW.h"
   #define Platform_TaskCore TaskCoreFFTW
   #endif
#else
   #define Platform_TaskCore TaskCore
#endif


/**
  * class TaskDispatcher
  * Creates new TaskCore instances and supplies them with data. Result data from all
  * TaskCores may need to be gathered together and combined into one result data
  * set...
  */

class TaskDispatcher
{
public:

   /**
    * New TaskDispatcher instance based on the
    * passed settings. This C'stor prepares all
    * cores/threads for executing the spectrum
    * processing started later with run().
    * @param settings the settings, files and buffers to use
    */
   TaskDispatcher(swspect_settings_t* settings);

  /**
   * Cleanup
   */
  ~TaskDispatcher();

   /**
    * Starts processing data from the input sources
    * until they run out of data. Results are written
    * to output sinks continuosly.
    */
   void run();

private:

   swspect_settings_t *set;           // settings to run with, incl. preallocated buffers, open files, ...

   TaskCore **cores;                  // platform-specific computing "cores"
   Buffer   **corecombined_spectra;   // help buffers used for assembling together several core sub-spectrum results
   Buffer   **corecombined_pcals;     // help buffers used for assembling together several core Phase Cal detection results

};

#endif // TASKDISPATCHER_H
