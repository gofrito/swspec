#ifndef DATAUNPACKERS_H
#define DATAUNPACKERS_H
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
#include "DataUnpacker.h"
#include <mark5access.h>
#include <ippcore.h>

class SignedUnpacker : public DataUnpacker {
  public:
    SignedUnpacker(swspect_settings_t const* settings) { cfg = settings; }
    size_t extract_samples(char const* const, Ipp32f*, const size_t, const int) const;
    static bool canHandleConfig(swspect_settings_t const* settings);
  protected:
    swspect_settings_t const* cfg;
};

class UnsignedUnpacker : public DataUnpacker {
  public:
    UnsignedUnpacker(swspect_settings_t const* settings) { cfg = settings; }
    size_t extract_samples(char const* const, Ipp32f*, const size_t, const int) const;
    static bool canHandleConfig(swspect_settings_t const* settings);
  protected:
    swspect_settings_t const* cfg;
};

class TwoBitUnpacker : public DataUnpacker {
  public:
    TwoBitUnpacker(swspect_settings_t const* settings) { cfg = settings; }
    size_t extract_samples(char const* const, Ipp32f*, const size_t, const int) const;
    static bool canHandleConfig(swspect_settings_t const* settings);
  protected:
    swspect_settings_t const* cfg;
};

class TwoBitSinglechannelUnpacker : public DataUnpacker {
  public:
    TwoBitSinglechannelUnpacker(swspect_settings_t const* settings) { cfg = settings; }
    size_t extract_samples(char const* const, Ipp32f*, const size_t, const int) const;
    static bool canHandleConfig(swspect_settings_t const* settings);
  protected:
    swspect_settings_t const* cfg;
};

class Mark5BUnpacker : public DataUnpacker {
  public:
    Mark5BUnpacker(swspect_settings_t const* settings) { cfg = settings; }
    size_t extract_samples(char const* const, Ipp32f*, const size_t, const int) const;
    static bool canHandleConfig(swspect_settings_t const* settings);
  protected:
    swspect_settings_t const* cfg;
};

class VLBAUnpacker : public DataUnpacker {
  public:
    VLBAUnpacker(swspect_settings_t const*);
    size_t extract_samples(char const* const, Ipp32f*, const size_t, const int) const;
    static bool canHandleConfig(swspect_settings_t const* settings);
  protected:
    swspect_settings_t const* cfg;
    struct mark5_stream *ms;
    Ipp32f** allchannels;
};

class MarkIVUnpacker : public DataUnpacker {
  public:
    MarkIVUnpacker(swspect_settings_t const*);
    size_t extract_samples(char const* const, Ipp32f*, const size_t, const int) const;
    static bool canHandleConfig(swspect_settings_t const* settings);
  protected:
    swspect_settings_t const* cfg;
    int fanout;
    struct mark5_stream *ms;
    Ipp32f** allchannels;
};

class Mk5BUnpacker : public DataUnpacker {
  public:
    Mk5BUnpacker(swspect_settings_t const*);
    size_t extract_samples(char const* const, Ipp32f*, const size_t, const int) const;
    static bool canHandleConfig(swspect_settings_t const* settings);
  protected:
    swspect_settings_t const* cfg;
    int fanout;
    struct mark5_stream *ms;
    Ipp32f** allchannels;
};

#endif // DATAUNPACKERS_H
