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

#include "VSIBSource.h"
#include <string>
#include <iostream>
using std::cerr;
using std::endl;

/**
 * Open the file
 * @return int Returns 0 on success
 * @param  uri File path
 */
int VSIBSource::open(std::string uri)
{
   totalBytesWanted = 16*1024*1024; // TODO prase from 'uri'
   fvsib = ::open(VSIB_DEV_PATH, O_RDONLY);
   if (-1 == fvsib) {
      cerr << "VSIBSource: could not open " << VSIB_DEV_PATH << endl;
      return -1;
   }
   ::ioctl(fvsib, VSIB_SET_MODE, VSIB_MODE_MODE(0x00) | VSIB_MODE_RUN);
   return 0;
}

/**
 * Try to fill the entire bufferspace with new data and return the number of bytes actually read
 * @return int    Returns the amount of bytes read
 * @param  bspace Pointer to Buffer to fill out
 */
int VSIBSource::read(Buffer* buf)
{
   int nread;
   char *dst   = buf->getData();
   int nwanted = buf->getAllocated();
   /* read the data */
   while (nwanted>0) {
      nread = ::read(fvsib, dst, nwanted);
      dst     += nread;
      nwanted -= nread;
   }
   /* set buffer size */
   nwanted = buf->getAllocated();
   buf->setLength(nwanted);
   /* check for "EOF" */
   if ((unsigned long)nwanted > totalBytesWanted) {
      totalBytesWanted = 0;
   } else {
      totalBytesWanted -= nwanted;
   }
   return nwanted;
}

/**
 * Close the resource
 * @return int
 */
int VSIBSource::close()
{
   ::ioctl(fvsib, VSIB_SET_MODE, VSIB_MODE_STOP);
   ::close(fvsib);
   totalBytesWanted = 0;
   return 0;
}

/**
 * Check for end of file
 * @return bool EOF
 */
bool VSIBSource::eof()
{
   return (totalBytesWanted <= 0);
}

#ifdef UNIT_TEST_FSOURCE
int main(int argc, char** argv)
{
   return 0;
}
#endif

