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

#include "Buffer.h"
#include <malloc.h>
#include <memory.h>
#include <iostream>
#include <cstring>

using std::cerr;
using std::endl;


/**
 * Initialize buffer by allocating memory-aligned bytes.
 * @param bytes Amount of bytes to allocate
 */
Buffer::Buffer(size_t bytes)
{
   len_allocated = 0;
   length        = 0;
   data = (char*)memalign(128, bytes);
   if (NULL == data) {
      cerr << "Failed to allocate " << bytes << " bytes." << endl;
   } else {
      len_allocated = bytes;
   }
}

/**
 * Release the buffer
 */
Buffer::~Buffer() 
{
   if (NULL != data) {
      free(data);
      data = NULL;
   }
}

/**
 * Return pointer to buffer.
 * @return char*
 */
char* Buffer::getData()
{
   return data;
}

/**
 * Return amount of data in the buffer.
 * @return size_t
 */
size_t Buffer::getLength()
{
   return length;
}

/**
 * Returns how many bytes were allocated for the buffer.
 * @return size_t
 */
size_t Buffer::getAllocated()
{
   return len_allocated;
}

/**
 * Externally set the length of contained data.
 * @param len New length
 */
void Buffer::setLength(size_t len)
{
   if (len <= len_allocated) {
      length = len;
   } else {
      cerr << "Warning: Buffer::setLength() cropping " << len << " to allocated length " << len_allocated << endl;
      length = len_allocated;
   }
}

#ifdef UNIT_TEST_BUF
int main(int argc, char** argv)
{
   return 0;
}
#endif

