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

#include "TaskDispatcher.h"
#include "Helpers.h"

#include <cstdlib>
#include <iostream>
#include <algorithm>

using std::cerr;
using std::endl;

/**
 * New TaskDispatcher instance based on the
 * passed settings. This C'stor prepares all
 * cores/threads for executing the spectrum
 * processing started later with run().
 * @param settings the settings, files and buffers to use
 */
TaskDispatcher::TaskDispatcher(swspect_settings_t* settings)
{
   /* Silly checks */
   this->set = settings;
   if (this->set == NULL) {
      cerr << "TaskDispatcher NULL task" << endl;
      exit(-1);
   }
   std::ostream* log = set->tlog;

   /* Pre-fill the first buffers */
   *log << "TaskDispatcher buffer prefill..." << endl;
   for (int c=0; c<(set->num_cores); c++) {
      for (int s=0; s<(set->num_sources); s++) {
         int dblbuf_nr = 0;
         (set->sources[s])->read (set->rawbuffers[c][dblbuf_nr][s]);
      }
   }

   /* Initialize the cores specific to the platform */
   *log << "TaskDispatcher core init... ";
   this->cores = new TaskCore*[set->num_cores];
   for (int c=0; c<(set->num_cores); c++) {
      *log << c << " ";
      cores[c] = new Platform_TaskCore(c);
      cores[c]->prepare(set);
   }
   *log << endl;

   /* Prepare the buffers we might need for assembling sub-spectra together */
   this->corecombined_spectra = new Buffer*[set->num_sinks];
   size_t peak_size           = std::max(set->fft_bytes_xpol, set->fft_bytes_ssb);
   for (int cc=0; cc < (set->num_sinks); cc++) {
      corecombined_spectra[cc] = new Buffer(peak_size);
      corecombined_spectra[cc]->setLength(peak_size);
      cores[0]->resetBuffer(corecombined_spectra[cc]);
   }

   /* Prepare the buffers we might need for assemblin Phase Cal subresults together */
   this->corecombined_pcals   = new Buffer*[set->num_sources];
   for (int cp=0; cp<(set->num_sources); cp++) {
      corecombined_pcals[cp] = new Buffer(set->pcal_result_bytes);
      corecombined_pcals[cp]->setLength(set->pcal_result_bytes);
      cores[0]->resetBuffer(corecombined_pcals[cp]);
   }

   return;
}

/**
 * Starts processing data from the input sources
 * until they run out of data. Results are written
 * to output sinks continuosly.
 */
void TaskDispatcher::run()
{
   int  ncores    = set->num_cores;
   int  chunk_nr  = 0;
   int  dblbuf_nr = 0;
   bool gotEOF    = false;
   bool wroteSome = false;

   int num_combined  = 0;
   int total_spectra = 0;
   int total_spectra_prev  = 0;
   int total_corecompleted = 0;
   double times[4];

   std::ostream* log = set->tlog;

   /* Process all available sample data -- allocate [raw]=>[core0][core1]...[coreN] */
   *log << "TaskDispatcher processing all input chunks... ";
   times[0] = Helpers::getSysSeconds();
   times[2] = times[0];
   do {

#ifdef VERBOSE
//	cerr << (chunk_nr++) << " ";
#endif

      /* start background processing of the current buffers */
      for (int c=0; c<ncores; c++) {
         cores[c]->run ( set->rawbuffers[c][dblbuf_nr],
                         set->outbuffers[c],
                         set->outbuffersXpol[c],
                         set->outbuffersPCal[c]
                       );
      }

      /* pre-fill the next raw input buffers */
      for (int c=0; c<ncores; c++) {
         for (int s=0; s<set->num_sources; s++) {
            (set->sources[s])->read (set->rawbuffers[c][(dblbuf_nr+1)%2][s]);
         }
      }

      /* wait for calculations to complete and write out results on the go */
      wroteSome = false;
      for (int c=0; c<ncores; c++) {

         /* wait for one new result */
         int numcompleted = cores[c]->join();
         total_corecompleted += numcompleted;

         /* write it directly to sinks or combine into common results? */
         if (set->max_buffers_per_spectrum > 1) {

            /* subspectra from cores, assemble into common specrum */
            cores[c]->combineResults(corecombined_spectra, corecombined_pcals);

            /* write completed assembled spectrum */
            num_combined++;
            if (num_combined == set->max_buffers_per_spectrum) {

                for (int sk=0; sk<set->num_sinks; sk++) {
                   set->sinks[sk]->write(corecombined_spectra[sk]);
                   cores[0]->resetBuffer(corecombined_spectra[sk]);
                }

                if (set->extract_PCal) {
                   for (int pc=0; pc<set->num_sources; pc++) {
                       #if 0
                       cerr << endl << "Source " << pc << " pcal: ";
                       Helpers::print_vecCplx((float*)corecombined_pcals[pc]->getData(), set->pcal_tonebins);
                       #endif
                       set->pcalsinks[pc]->write(corecombined_pcals[pc]);
                       cores[0]->resetBuffer(corecombined_pcals[pc]);
                   }
                }

                total_spectra++;
                wroteSome    = true;
                num_combined = 0;
            }

         } else {

            /* one or more full spectra from cores, write out */
            for (int sk=0; sk<set->num_sinks && sk<set->num_sources; sk++) {
               set->sinks[sk]->write(set->outbuffers[c][sk]);
            }
            for (int xp=0; xp<set->num_xpols; xp++) {
               int xpolsink = set->num_sources + xp;
               set->sinks[xpolsink]->write(set->outbuffersXpol[c][xp]);
            }
            if (set->extract_PCal) {
               for (int pc=0; pc<set->num_sources; pc++) {
                   #if 0
                   cerr << endl << "Source " << pc << " pcal: ";
                   Helpers::print_vecCplx((float*)set->outbuffersPCal[c][pc]->getData(), set->pcal_tonebins);
                   #endif
                   set->pcalsinks[pc]->write(set->outbuffersPCal[c][pc]);
                   cores[0]->resetBuffer(set->outbuffersPCal[c][pc]);
               }
            }
            total_spectra += set->max_spectra_per_buffer;
            wroteSome = true;
         }
      }

      /* throughput statistics */
      if (wroteSome) {
         times[3]   = times[2];
         times[2]   = Helpers::getSysSeconds();
         double dT  = times[2] - times[3];
         int newspc = total_spectra - total_spectra_prev;
         if (dT == 0) { dT = 1.0; }
#ifdef VERBOSE 
         *log << "TaskDispatcher: wrote " << newspc << " new spectra, "
              << total_spectra << " in total, "
              << " time " << dT << "s"
              << " rate " << (newspc * set->fft_integ_seconds / dT) << "x realtime" 
              << endl;
#endif
         total_spectra_prev = total_spectra;
      }

      /* continue with the next buffered data */
      dblbuf_nr = (dblbuf_nr+1)%2;

      /* combined check for EOF on all of the input sources */
      for (int s=0; s<set->num_sources; s++) {
         int core = 0;
         if ((set->sources[s])->eof()) {
             gotEOF = true;
         }
         if (set->rawbuffers[core][dblbuf_nr][s]->getLength() > 0) {
             break;
         }
      }

   } while (!gotEOF);
   times[1] = Helpers::getSysSeconds();
   cerr << endl;

   /* clean up */
   for (int i=0; i<ncores; i++) {
      cores[i]->finalize();
   }
   for (int i=0; i<set->num_sinks; i++) {
      set->sinks[i]->close();
   }
   if (set->extract_PCal) {
      for (int pc=0; pc<set->num_sources; pc++) {
         set->pcalsinks[pc]->close();
      }
   }
   *log << "TaskDispatcher completed, time delta " << (times[1] - times[0]) << "s." << endl;
   return;
}


#ifdef UNIT_TEST_TD
int main(int argc, char** argv)
{
   return 0;
}
#endif
