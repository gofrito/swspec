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

#include "DataSource.h"
#include "FileSource.h"
#include "VSIBSource.h"

#include <iostream>
using std::cerr;
using std::endl;

/**
 * Return a new data source object corresponding
 * to the URI.
 * @return DataSource* A data source that can be FileSource, VSIBSource etc
 * @param  uri         The URL or path or other identifier for the resource location
 */
DataSource* DataSource::getDataSource(std::string const& uri, struct swspect_settings_tt* set)
{
   DataSource* ds = NULL;

   /* select correct datasource derivate -- lots of unsupported ones for future TODO... */
   if (uri.compare(0, 7, "vsib://") == 0) {
      ds = new VSIBSource();
   } else if (uri.compare(0, 8, "tsunami://") == 0) {
      cerr << "Tsunami protocol not yet supported" << endl;
   } else if (uri.compare(0, 6, "mpi://") == 0) {
      cerr << "MPI not yet supported" << endl;
   } else if (uri.compare(0, 6, "udp://") == 0) {
      cerr << "UDP Socket not yet supported" << endl;
   } else if (uri.compare(0, 6, "udt://") == 0) {
      cerr << "UDP protocol not yet supported" << endl; 
   } else {
      ds = new FileSource();
   }
   ds->cfg = set;

   return ds;
}


#ifdef UNIT_TEST_DSOURCE
int main(int argc, char** argv)
{
   return 0;
}
#endif

