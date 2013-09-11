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

#include "PlplotSink.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>

#ifdef PLFLT
#warn "PLPlot settings are for 'double', should be 'float'!"
#endif

/**
 * Open the plot
 * @return int Returns 0 on success
 * @param  uri Ignored
 */
int PlpotSink::open(std::string uri)
{
   if (pls != NULL) {
      // pls->end();
      // delete pls; // -- Plplot does not like this
   }
   pls = new plstream();
   pls->start("xwin",1,1); // WindowsType, #cols, #rows
   pls->font(3);		// Italian font 
   pls->adv(0);
   xdata = (PLFLT*)malloc(sizeof(PLFLT)*(size_t)settings->fft_ssb_points);
   ydata = (PLFLT*)malloc(sizeof(PLFLT)*(size_t)settings->fft_ssb_points);
   return 0;
}


/**
 * Write data onto the screen as some graph
 * @return Returns the number of bytes written
 * @param  buf Pointer to the buffer to write out.
 */
size_t PlpotSink::write(Buffer *buf)
{
   if (pls == NULL || settings == NULL) { return 0; }

   float* src = (float*) buf->getData();
   float  ppeak = -1e9, npeak = 1e9;
   size_t len = settings->fft_ssb_points; // len = buf->getLength() / (sizeof(float));

   float xscale;
   std::string xlabel("FFT point");
   std::string ylabel("Logarithmic Spectral Power");

   xscale = 1e-6 * settings->samplingfreq / float(settings->fft_points);
   xlabel = std::string("Video BW in MHz");

   for (size_t i=0; i<len; i++) {
      xdata[i] = xscale*PLFLT(i);
      // ydata[i] = log10(abs(*(src+0))); // Re, skip Im if data is {Re,Im}
      // src += 2;
      if (*src == 0.0f) {
          ydata[i] = 1;
      } else {
          ydata[i] = log10(abs(*(src+0)));
      } 
      src++;
      if (i > 16) {
         if (ydata[i] > ppeak) ppeak = ydata[i];
         if (ydata[i] < npeak) npeak = ydata[i];
      }
   }
   #if 0
   std::cerr << "len=" << len
             << " npeak=" << npeak
             << " ppeak=" << ppeak
             << std::endl << std::flush;
   #endif

   pls->col0(2);
   pls->schr(0,0.5);
   pls->vpor(0.1, 0.9, 0.1, 0.9);
   pls->wind(xdata[0], xdata[len-1], npeak, ppeak);
   pls->clear();
   pls->box("bcnst", 0.0, 0, "bnstv", 0.0, 0);
   pls->line(len, xdata, ydata);
   pls->lab(xlabel.c_str(), ylabel.c_str(), this->title.c_str());
   return buf->getLength();
}


/**
 * Close the resource
 * @return int Returns 0 on success
 */
int PlpotSink::close() 
{
   //delete pls;
   //pls = NULL;
   return 0;
}


#ifdef UNIT_TEST_PLPLOTSINK
int main(int argc, char** argv)
{
   return 0;
}
#endif

