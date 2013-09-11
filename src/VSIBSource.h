#ifndef VSIBSOURCE_H
#define VSIBSOURCE_H
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

#include "DataSource.h"
#include "Buffer.h"
#include <string>

#define VSIB_DEV_PATH "/dev/vsib"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define VSIB_SET_MODE     0x7801

#define VSIB_MODE_RUN     0x80000000
#define VSIB_MODE_STOP    0x0

#define VSIB_MODE_MODE(mode) (((mode) & 0x0f) << 16)

class VSIBSource : public DataSource
{

private:
   unsigned long totalBytesWanted;
   int           fvsib;

public:
   VSIBSource() { totalBytesWanted=0; return; };
   VSIBSource(std::string uri) { open(uri); }
   ~VSIBSource() { close(); }

public:
   /**
    * Open the file
    * @return int Returns 0 on success
    * @param  uri File path
    */
   int open(std::string uri);

   /**
    * Tries to fill the entire buffer with new data and return the number of bytes actually read
    * @return int    Returns number of bytes read
    * @param  bspace Try to read enough data to fill buffer, return amount of bytes read
    */
   int read(Buffer *buf);

   /**
    * Close the file
    * @return int
    */
   int close();

    /**
     * Check for end of file
     * @return bool EOF
     */
   bool eof();

};

#endif // VSIBSOURCE_H
