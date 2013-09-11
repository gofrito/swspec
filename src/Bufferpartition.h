/************************************************************************
 * IBM Cell Software Spectrometer
 * Copyright (C) 2008 Jan Wagner
 * 
 * Bufferpartition.h - Copyright jwagner
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


#ifndef BUFFERPARTITION_H
#define BUFFERPARTITION_H

#include "swspectrometer.h"


/**
  * class Bufferpartition
  * Contains a properly aligned chunk of memory.
  */

class Bufferpartition
{
public:

   // Constructors/Destructors

   /**
    * Allocate 'size' bytes with proper memory alignment
    * @param  size
    */
   Bufferpartition(int size);

   /**
    * Empty Destructor
    */
   ~Bufferpartition();


   /**
    * Return pointer to contained char array.
    * @return char*
    */
   char* getDataPtr();


   /**
    * Return length of the contained data in bytes.
    * @return int
    */
   int getDataLen();


   /**
    * @param  datalen
    */
   void setDataLen(int datalen);

private:

   char *data;          // actual aligned data
   int   max_data_len;  // allocated length of the buffer
   int   data_len;      // length of data in the buffer

};

#endif // BUFFERPARTITION_H
