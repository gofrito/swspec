#ifndef DATASINK_H
#define DATASINK_H
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

#include "Settings.h"
#include <string>
#include <iostream>

class Buffer;

class DataSink
{

public:
   virtual ~DataSink() { }

   /**
    * Open and truncate the resource
    * @return int Returns 0 on success
    * @param  uri Location of the sink e.g. file path.
    */
   virtual int open(std::string uri) = 0;

   /**
    * Write data into resource
    * @return Returns the number of bytes written
    * @param  buf Pointer to the buffer to write out.
    */
   virtual size_t write(Buffer *buf) = 0;

   /**
    * Close the resource
    * @return int Returns 0 on success
    */
   virtual int close() = 0;

   /**
    * Return a new data sink object corresponding to the URI.
    * @return DataSink*   A data sink that can be FileSink, etc
    * @param  uri         The URL or path or other identifier for the resource location
    */
   static DataSink* getDataSink(std::string const&, struct swspect_settings_tt*);

};

#endif // DATASINK_H
