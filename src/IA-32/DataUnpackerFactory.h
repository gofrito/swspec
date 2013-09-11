#ifndef DATAUNPACKERFACTORY_H
#define DATAUNPACKERFACTORY_H
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
#include "Helpers.h"
#include "DataUnpacker.h"
#include "DataUnpackers.h"

#include <ippcore.h>

#include <string>
#include <iostream>

/**
 * DataUnpacker factory class
 */
class DataUnpackerFactory {
public:
    static DataUnpacker* getDataUnpacker(swspect_settings_t const* cfg) {

        if (NULL == cfg) {
            return NULL;
        }

        /* Simple headerless data formats */
        if (cfg->sourceformat == RawSigned) {
            if (TwoBitUnpacker::canHandleConfig(cfg)) {
                return new TwoBitUnpacker(cfg);
            }
            if (TwoBitSinglechannelUnpacker::canHandleConfig(cfg)) {
                return new TwoBitSinglechannelUnpacker(cfg);
            }
            if (SignedUnpacker::canHandleConfig(cfg)) {
                return new SignedUnpacker(cfg);
            }
            // TODO: 4-bit unpack
        }
        if (cfg->sourceformat == RawUnsigned) {
            if (TwoBitUnpacker::canHandleConfig(cfg)) {
                return new TwoBitUnpacker(cfg);
            }
            if (TwoBitSinglechannelUnpacker::canHandleConfig(cfg)) {
                return new TwoBitSinglechannelUnpacker(cfg);
            }
            if (UnsignedUnpacker::canHandleConfig(cfg)) {
                return new UnsignedUnpacker(cfg);
            }
            // TODO: 4-bit unpack
        }
        /* Data from data-replacement formats with perverted unpacking */
        if (cfg->sourceformat == VLBA) {
            if (VLBAUnpacker::canHandleConfig(cfg)) {
                return new VLBAUnpacker(cfg);
            }
        }
        if (cfg->sourceformat == MKIV) {
            if (MarkIVUnpacker::canHandleConfig(cfg)) {
                return new MarkIVUnpacker(cfg);
            }
        }
        if (cfg->sourceformat == Mk5B) {
            if (Mk5BUnpacker::canHandleConfig(cfg)) {
                return new Mk5BUnpacker(cfg);
            }
        }
        /* Data from datasources with headers pre-removed */
        if (cfg->sourceformat == Mark5B) {
            if (Mark5BUnpacker::canHandleConfig(cfg)) {
                return new Mark5BUnpacker(cfg);
            }
        }
        if (cfg->sourceformat == iBOB) {
            if (SignedUnpacker::canHandleConfig(cfg)) {
                return new SignedUnpacker(cfg);
            }
        }
        if (cfg->sourceformat == VDIF) {
            // TODO: cvs add VDIFUnpacker.cpp?
            //if (VDIFUnpacker::canHandleConfig(cfg)) {
            //    return new VDIFUnpacker(cfg);
            //}
        }

        std::cerr << "DataUnpackerFactory: encoding '" << cfg->sourceformat_str << "' and "
                    << cfg->source_channels << "-channel "
                    << cfg->bits_per_sample << "-bit format "
                    << "does not have any unpacking method yet!" << std::endl;
        return NULL;
    }
};

#endif // DATAUNPACKERFACTORY_H
