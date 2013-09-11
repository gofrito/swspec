#ifndef DATASOURCE_H
#define DATASOURCE_H
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

#include "Buffer.h"
#include "Settings.h"

#include <string>

class DataSource
{
   friend class FileSource;
   friend class VSIBSource;

public:
   virtual ~DataSource() { }

   /**
    * Open the resource
    * @return int Returns 0 on success
    * @param  uri The URL or path or other identifier for the resource location
    */
   virtual int open(std::string uri) = 0;

   /**
    * Try to fill the entire buffer with new data and return the number of bytes actually read
    * @return int    Returns the amount of bytes read
    * @param  buf    Pointer to Buffer to fill out
    */
   virtual int read(Buffer *buf) = 0;

   /**
    * Close the resource
    * @return int Returns 0 on success
    */
   virtual int close() = 0;

   /**
    * Check for end of file
    * @return bool EOF 
    */
   virtual bool eof() = 0;

   /**
    * Return a new data source object corresponding to the URI.
    * @return DataSource* A data source that can be FileSource, VSIBSource etc
    * @param  uri         The URL or path or other identifier for the resource location
    * @param  set         Settings that also contain the underlying 'sourceformat_str' used
    *                     by the specific data source later to remove headers.
    */
   static DataSource* getDataSource(std::string const& uri, struct swspect_settings_tt* set);

private:
    struct swspect_settings_tt* cfg;

};

#endif // DATASOURCE_H
