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

#include "Bufferpartition.h"

/**
 * Allocate 'size' bytes with proper memory alignment
 * @param  size
 */
Bufferpartition::Bufferpartition(int size) {

   max_data_len = 0; 
   data_len = 0;

   if (size <= 0) {
      return;
   }

   int rc = posix_memalign((void**)&data, 128, size);
   if (rc != 0) {
      cout << "Bufferpartition posix_memalign(" << size << ") returned error " << rc << endl;
   } else {
      max_data_len = 0; 
   }

}


Bufferpartition::~Bufferpartition() { 
   if (max_data_len > 0) {
      free((void*)data);
   }
}


/**
 * Return pointer to contained char array.
 * @return char*
 */
char* Bufferpartition::getDataPtr() {
   return data;
}


/**
 * Return number of valid bytes currently in the char array.
 * @return int
 */
int Bufferpartition::getDataLen() {
   return data_len;
}


/**
 * Set number of valid bytes currently in the char array.
 * @param  datalen
 */
void Bufferpartition::setDataLen(int datalen) {
   data_len = datalen;
}


