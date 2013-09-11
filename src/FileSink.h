#ifndef FILESINK_H
#define FILESINK_H
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

#include "DataSink.h"
#include "Buffer.h"

#include <string>
#include <fstream>

class FileSink : public DataSink
{
public:
   FileSink(swspect_settings_t* settings) { this->settings = settings; return; }
   FileSink(std::string uri) { open(uri); }
   ~FileSink() { close(); }

   /**
    * Open the file
    * @return int Returns 0 on success
    * @param  uri File path
    */
   int open(std::string uri);

   /**
    * Write data into resource
    * @return Returns the number of bytes written
    * @param  buf Pointer to the buffer to write out.
    */
   size_t write(Buffer *buf);

   /**
    * Close the resource
    * @return int Returns 0 on success
    */
   int close();

private:
   std::ofstream ofile;
   swspect_settings_t* settings;

};

#endif // FILESINK_H
