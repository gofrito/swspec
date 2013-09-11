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

#include "FileSink.h"
#include <iostream>
#include <fstream>
#include <string>

/**
 * Open the file
 * @return int Returns 0 on success
 * @param  uri File path
 */
int FileSink::open(std::string uri)
{
   if (ofile.is_open()) {
      ofile.close();
   }
   ofile.open(uri.c_str(), /*std::ios::binary |*/ std::ofstream::trunc | std::ios::out);
   if (ofile.is_open()) {
      ofile.precision(12);
      return 0;
   } else {
      std::cerr << "Could not open output file " << uri << std::endl;
      return -1;
   }
}


/**
 * Write data into resource
 * @return int Returns the number of bytes written
 * @param  buf Pointer to the buffer to write out.
 */
size_t FileSink::write(Buffer *buf)
{
   if (NULL == buf || buf->getLength() <= 0 || NULL == this->settings) {
      return 0;
   }
   if (!ofile.is_open()) {
      std::cerr << "FileSink write: file not open" << std::endl << std::flush;
      return 0;
   }
   if (!ofile.good()) {
      std::cerr << "FileSink write: file in bad state" << std::endl << std::flush;
      return 0;
   }

   float* src = (float*) buf->getData();
   size_t len = buf->getLength() / (2*sizeof(float));

   /* Write according to the output format specified in the INI/Settings */
   if (this->settings->sinkformat == Binary) {
      ofile.write(buf->getData(), buf->getLength());
   } else {
      ofile << "// --------------------- DATA SET TIMESTAMP xxxx ---------------------" << std::endl;
      #if 0
      ofile << "// complex points = " << len << std::endl;
      for (int i=0; i<len; i++) {
         ofile << *(src+0) << "\t" << *(src+1) << std::endl; // Re,Im
         src += 2;
      }
      #else
      ofile << "// FFT bins 0.." << (len-1) << " + Nyquist" << std::endl;
      float mrNyquist = *(src+1);
      for (size_t i=0; i<len; i++) {
         // ofile << i << "\t\t" << *(src+0) << std::endl; // Re only
         ofile << *(src+0) << std::endl; // Re only
         src += 2;
      }
      // ofile << len << "\t\t" << mrNyquist << std::endl;
      ofile << mrNyquist << std::endl;
      #endif
   }
   ofile.flush();

   if (!ofile.good()) {
      std::cerr << "Write I/O error!" << std::endl << std::flush;
   }

   return buf->getLength();
}


/**
 * Close the resource
 * @return int Returns 0 on success
 */
int FileSink::close() 
{
   ofile.close();
   return 0;
}


#ifdef UNIT_TEST_FSINK
int main(int argc, char** argv)
{
   return 0;
}
#endif

