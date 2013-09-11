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

#include "DataSink.h"
#include "FileSink.h"
#include <string>

/**
 * Return a new data source object corresponding
 * to the URI.
 * @return DataSource* A data source that can be FileSource, VSIBSource etc
 * @param  uri         The URL or path or other identifier for the resource location
 */
DataSink* DataSink::getDataSink(std::string const& uri, swspect_settings_t* settings)
{
   DataSink* ds = NULL;

   /* select correct datasink derivate -- lots of unsupported ones for future TODO... */
   if (uri.compare(0, 6, "mpi://") == 0) {
      std::cerr << "MPI output not yet supported" << std::endl;
   } else {
      ds = new FileSink(settings);
   }

   return ds;
}


#ifdef UNIT_TEST_DSINK
int main(int argc, char** argv)
{
   return 0;
}
#endif

