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

// For debugging: 
// SPU_DEBUG_START=1 ./swspectrometer --cores=1 --fftpts=1024 --fftint=8192 Data1.NoSpin_rep.byte results-data1.txt

#include "TaskCoreCell.h"
#include "cell_spectrometer.h" 

/* The spectrometer_spu.a file (SPE code) that the linker will embed: */
extern spe_program_handle_t spectrometer_spu;

/**
 * Do all necessary initializations
 * @param  settings  Pointer to the global settings
 */
void TaskCoreCell::prepare(swspect_settings_t* settings)
{
   /* setup */
   this->cfg = settings;
   this->total_time = 0;
   this->total_spectra_count = 0;
   this->total_ffts_count = 0;

   /* prepare SPE task context constants */
   cb.rank                = this->rank;
   cb.fft_points          = cfg->fft_points;
   cb.fft_points_log2     = (int)log2(cfg->fft_points);
   cb.overlap_factor      = cfg->fft_overlap_factor;
   cb.integrate_ffts      = cfg->integrated_ffts;
   cb.samples_per_rawbyte = cfg->samples_per_rawbyte;

   /* load and start the SPE program pointing it to the control block */
   this->gid = spe_create_group (SCHED_OTHER, 0, 1);
   if ((this->gid == NULL) || (spe_group_max (this->gid) < 1)) {
      cerr << "TaskCoreCell prepare : spe_create_group() failed!" << endl;
      exit(1);
   }
   this->speid = spe_create_thread(this->gid, &spectrometer_spu, (unsigned long long *)&(this->cb), NULL, -1, 0);
   if (this->speid == NULL) {
      cerr << "TaskCoreCell prepare : spe_create_thread() failed!" << endl;
   }

   cerr << "SPE thread created, rank " << this->rank << " ID " << speid << endl;
   return;
}

/**
 * Perform the spectrum computations.
 * @return int     Returns 0 when task was successfully started
 * @param  inbuf   Pointer to raw input Buffer
 * @param  outbuf  Pointer to spectrum output Buffer
 */
int TaskCoreCell::run(Buffer* inbuf, Buffer* outbuf)
{
   int mbox;

   /* update SPE task context */
   cb.terminate_flag  = 0;
   cb.rawif_src.p     = (void*) inbuf->getData();
   cb.rawif_bytecount = inbuf->getLength();
   cb.spectrum_dst.p  = (void*) outbuf->getData();

   /* task context ready, wake up the SPE to start processing */
   spe_write_in_mbox(this->speid, 0x1234);

   /* remember the current output buffer for setting length on completion */
   prevBufOut = outbuf;
   return 0;
}

/**
 * Wait for computation to complete.
 * @return int     Returns -1 on failure, or the number >=0 of output
 *                 buffer(s) that contain a complete spectrum
 */
int TaskCoreCell::join() 
{
   unsigned int spe_time;
   unsigned int spe_ffts;
   unsigned int spe_spectra;

   /* wait until SPE completes and sends out the performance statistics */
   while (!spe_stat_out_mbox(this->speid));
   spe_time = spe_read_out_mbox(this->speid);
   this->total_time += spe_time;

   while (!spe_stat_out_mbox(this->speid));
   spe_spectra = spe_read_out_mbox(this->speid);
   this->total_spectra_count += spe_spectra;

   while (!spe_stat_out_mbox(this->speid));
   spe_ffts = spe_read_out_mbox(this->speid);
   this->total_ffts_count += spe_ffts;

   #if 0
   cout << "SPE " << rank << " stats: " 
        << (1e6*spe_time/__timebase__) << "us " 
        << spe_ffts << " FFTs " 
        << spe_spectra << " spectra" << endl << flush;
   #endif

   /* set and return how many spectra are complete */
   if (prevBufOut != NULL) {
      prevBufOut->setLength(spe_spectra * (cfg->fft_points * sizeof(float)*2));
   }   
   return spe_spectra;
}

/**
 * Clean up all allocations etc
 * @return int     Returns 0 on success
 */
int TaskCoreCell::finalize() 
{ 
   int status;

   /* tell SPE to exit */
   cb.terminate_flag = 1;
   spe_write_in_mbox(this->speid, 0x1234);

   /* wait for the SPE program to exit */
   spe_wait(this->speid, &status, 0);

   #if 1
   cout << "SPE of rank " << rank << " completed, total time " << (double(total_time)/__timebase__) << "s, "
        << " total FFTs " << total_ffts_count << ", total spectra " << total_spectra_count << endl;
   #endif

   return 0; 
}

/**
 * Sum own spectral data to the provided output buffer.
 * @return int     Returns 0
 * @param  outbuf  Pointer to spectrum output Buffer
 */
int TaskCoreCell::combineResults(Buffer* outbuf)
{
   if (outbuf == NULL || prevBufOut == NULL) {
      cerr << "TaskCoreCell combine : null buffer(s)!" << endl;
      return 0;
   }

   vector float* vfNew = (vector float*)outbuf->getData();
   vector float* vfOld = (vector float*)prevBufOut->getData();

   for (int i=0; i<cfg->fft_points/2; ) { // 2 {re,im} points per vector
      const char unroll_factor = 4;
      for (char j=0; j<unroll_factor; j++) {
         vfNew[i+j] = vec_add(vfNew[i+j], vfOld[i+j]);
      }
      vfNew += unroll_factor; 
      vfOld += unroll_factor;
      i += unroll_factor;
   }
}
