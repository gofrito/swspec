#ifndef BUFFERSPACE_H
#define BUFFERSPACE_H
/************************************************************************
 * IBM Cell Software Spectrometer
 * Copyright (C) 2008 Jan Wagner
 * 
 * Bufferspace.h - Copyright jwagner
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

#include "swspectrometer.h"

#include <string>
#include <vector>

#define MAX_PARTS 64

/**
  * class Bufferspace
  */

class Bufferspace
{
public:

   /**
    * Empty Constructor
    */
   // Bufferspace();

   /**
    * Empty Destructor
    */
   ~Bufferspace();

   /**
    * Initialize buffer(s) to get total given size in bytes. Split the buffer(s) into
    * 'Bufferpartition' partitions of char[] arrays with platform specific optimal
    * memory alignment.
    * @param  totalbytes
    * @param  partitions
    */
    Bufferspace(int totalbytes, int partitions);

   /**
    * Return the number of partitions.
    * @return int
    */
   int getNumPartitions();

   /**
    * Return pointer to buffer partition object.
    * @return Bufferpartition
    * @param  index
    */
   Bufferpartition* getPartition(int index);

private:

   int num_partitions;
   Bufferpartition* parts[MAX_PARTS];

};

#endif // BUFFERSPACE_H
