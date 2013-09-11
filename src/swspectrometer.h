#ifndef SWSPECTROMETER_H
#define SWSPECTROMETER_H
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

#define VERSION_STR  "IBM Cell/Intel Software Spectrometer v1.03"
extern char          __BUILD_DATE;
extern char          __BUILD_NUMBER;

#include "Settings.h"
#include <vector>
#include <string>

class DataSource;
class DataSink;

// helpers
bool addOpenSource(std::string const&, std::vector<DataSource*>&, swspect_settings_t&);
bool addOpenSink  (std::string const&, std::vector<DataSink*>&, swspect_settings_t&);
bool addOpenPlotSink(std::string const&, std::vector<DataSink*>&, swspect_settings_t&, std::string const&);
std::string cfg_to_filename(std::string, swspect_settings_t const&, int);

#endif // SWSPECTROMETER_H
