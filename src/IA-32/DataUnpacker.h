#ifndef DATAUNPACKER_H
#define DATAUNPACKER_H
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

#include "Settings.h"

#include <cstring>
#include <ippcore.h>
#include <string>

/**
 * DataUnpacker base class
 */
class DataUnpacker {
  public:
    DataUnpacker() { return; }
    DataUnpacker(swspect_settings_t const* settings) { return; }
    virtual size_t extract_samples(char const* const src, Ipp32f* dst, const size_t count, const int channel) const = 0;
    static bool canHandleConfig(swspect_settings_t const* settings) { return false; }
};

#endif // DATAUNPACKER_H
