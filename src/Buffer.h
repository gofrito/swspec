#ifndef BUFFER_H
#define BUFFER_H
/************************************************************************
 * IBM Cell / Intel Software Spectrometer
 * Copyright (C) 2008 Jan Wagner
 *
 * Buffer.h - Copyright jwagner
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

#include <cstring>

class Buffer
{
private:
   char* data;
   size_t len_allocated;
   size_t length;

public:
   /**
    * Initialize buffer by allocating memory-aligned bytes.
    * @param bytes Amount of bytes to allocate
    */
   Buffer(size_t bytes);

   /**
    * Release the buffer
    */
   ~Buffer();

   /**
    * Return pointer to buffer.
    * @return char*
    */
   char* getData();

   /**
    * Return amount of data in the buffer.
    * @return size_t
    */
   size_t getLength();

   /**
    * Returns how many bytes were allocated for the buffer.
    * @return size_t
    */
   size_t getAllocated();

    /**
     * Externally set the length of contained data.
     * @param len New length
     */
   void setLength(size_t len);

};

#endif // BUFFER_H
