#ifndef FILESOURCE_H
#define FILESOURCE_H
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
#include <iostream>
#include <fstream>

extern "C" { 
    #include <mark5access.h> 
}

class FileSource : public DataSource
{
public:
   FileSource() : first_header_offset(0),got_eof(true)  { return; };
   FileSource(std::string uri) : first_header_offset(0) { open(uri); }
   ~FileSource() { close(); }

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

private:
   /**
    * On an open stream, scan for the first occurence of a format-dependant
    * data header and set the first_header_offset variable to it.
    * For headerless formats first_header_offset is set to 0.
    */
   void locateFirstHeader();

   /**
    * Assumes a header starts at the current read offset.
    * Reads the header and parses it. The read offset
    * after this function returns contains data.
    */
   void inspectAndConsumeHeader();

private:
   std::string _uri;
   std::string _format;
   std::ifstream ifile;
   std::ifstream::pos_type first_header_offset;

   bool got_eof;

   bool   sourceformat_uses_frames;
   size_t frame_header_length;
   size_t frame_payload_length;

   /* mark5access library */
   struct mark5_stream* _mk5s;
   off64_t mk5fileoffset;
   void reopen_mark5_stream(int secondstoskip); // workaround for one subset of mark5access seek() bugs

};

#endif // FILESOURCE_H
