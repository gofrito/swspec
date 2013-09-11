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

//
// Output spectrum data into a graph using PGPLOT 5
// In Debian Etch the package 'pgplot5' is in non-free.
//

#include <cmath>
#include "CPgplotSink.h"

/**
 * Open the file
 * @return int Returns 0 on success
 * @param  uri File path
 */
int CPgplotSink::open(string uri)
{
   /* open X11/xserver Pgplot window */
   cpgopen("/XSERVE");
   /* disable "Type <RETURN> for next page:" prompting */
   cpgask(0);
   return 0;
}

/**
 * Write data into resource
 * @return int Returns the number of bytes written
 * @param  buf Pointer to the buffer to write out.
 */
int CPgplotSink::write(Buffer *buf)
{
   if (NULL == buf || buf->getLength() <= 0)
      return 0;

   float* src = (float*) buf->getData();
   float  ppeak = 0.0, npeak = 0.0;
   int    len = buf->getLength() / (2*sizeof(float));

   float xdata[len];
   float ydata[len];

   cpgbbuf();
   cpgsave();
   for (int i=0; i<len; i++) {
      xdata[i] = float(i);
      ydata[i] = log10(abs(*(src+0))); // Re, skip Im
      if (ydata[i] > ppeak) ppeak = ydata[i];
      if (ydata[i] < npeak) npeak = ydata[i];
      src += 2;
   }
   cpgenv(0, len, npeak, ppeak, 0, 1); //PG_SCALE_INDEPENDENT, PG_AXIS_LINEAR);
   cpglab("FFT point", "Power", "Integrated power spectrum");
   // cpgpt(len, xdata, ydata, 1);
   cpgline(len, xdata, ydata);
   cpgunsa();
   cpgebuf();

   return buf->getLength();
}

/**
 * Close the resource
 * @return int Returns 0 on success
 */
int CPgplotSink::close()
{
   cpgclos();
   cpgend();
   return 0;
}

#ifdef UNIT_TEST_CPGSINK
int main(int argc, char** argv)
{
   return 0;
}
#endif

