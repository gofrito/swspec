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

#include "Bufferspace.h"

// Constructors/Destructors

/**
 * Initialize buffer(s) to get total given size in bytes. Split the buffer(s) into
 * 'Bufferpartition' partitions of char[] arrays with platform specific optimal
 * memory alignment.
 * @param  totalbytes
 * @param  partitions
 */
Bufferspace::Bufferspace(int totalbytes, int partitions ) {

   if (partitions <= 0 || totalbytes <= 0) {
      num_partitions = 0;
      return;
   }
   num_partitions = partitions;

   int bytesperpart = totalbytes / partitions;
   for (int i=0; i<partitions; i++) {
      if (totalbytes >= bytesperpart) {
         parts[i] = new Bufferpartition(bytesperpart);
      } else {
         parts[i] = new Bufferpartition(totalbytes);
      }
      totalbytes -= bytesperpart;
   }
}

Bufferspace::~Bufferspace() { 
   for (int i=0; i<num_partitions; i++) {
      delete parts[i];
   }
}


/**
 * Return the number of partitions.
 * @return int
 */
int Bufferspace::getNumPartitions() {
   return num_partitions;
}


/**
 * Return pointer to buffer partition object.
 * @return Bufferpartition*
 * @param  index
 */
Bufferpartition* Bufferspace::getPartition(int index) {
   if (index<num_partitions) {
      return parts[index];
   } else {
      return NULL;
   }
}
