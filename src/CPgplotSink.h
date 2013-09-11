#ifndef CPGPLOTSINK_H
#define CPGPLOTSINK_H
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
#include <fstream>
#include <cstring>

using namespace std;

#include "DataSink.h"
#include "Buffer.h"

#include <cpgplot.h>

#define PG_SCALE_INDEPENDENT 0
#define PG_SCALE_FIXED       1

#define PG_AXIS_LINEAR       0
#define PG_AXIS_XLOG         10
#define PG_AXIS_YLOG         20
#define PG_AXIS_XYLOG        30
#define PLOTSTYLE_SCATTER    0
#define PLOTSTYLE_LINES      1

class CPgplotSink : public DataSink
{
public:
   CPgplotSink() { return; }
   CPgplotSink(string uri) { open(uri); }
   ~CPgplotSink() { close(); }

   /**
    * Open the file
    * @return int Returns 0 on success
    * @param  uri File path
    */
   int open(string uri);

   /**
    * Write data into resource
    * @return int Returns the number of bytes written
    * @param  buf Pointer to the buffer to write out.
    */
   int write(Buffer *buf);

   /**
    * Close the resource
    * @return int Returns 0 on success
    */
   int close();

protected:
private:

};

#endif // CPGPLOTSINK_H
