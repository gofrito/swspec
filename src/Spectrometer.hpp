/********************************************************************************
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
 ********************************************************************************/
#ifndef _SPECTROMETER_HPP
#define _SPECTROMETER_HPP

#include <iostream>

// Real nums
typedef float real_t;

// Complex nums
typedef struct _complex_t {
    real_t re, im;
} complex_t;

// Spectrometer class
class Spectrometer {
   public:
     Spectrometer();
     ~Spectrometer();

   private:
     complex_t* spectrum;

   public:
     int resetSpectrum();
     int accumulateSpectrumC(complex_t* complexdata);
     int accumulateSpectrumR(real_t* realdata);
};

#endif // _SPECTROMETER_HPP
