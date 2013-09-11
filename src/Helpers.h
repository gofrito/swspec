#ifndef HELPERS_H
#define HELPERS_H
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
#include <sys/time.h>
#include <string>

class Helpers
{
  public:
    enum CicomparisonResult { NoMatch=0, FullMatch=1, BeginningsMatch=2 };

  public:
    static double getSysSeconds(void);
    static std::string itoa(int n);

    /**
     * Reinterpret a "yes" or "no" string as a boolean.
     * @return String "yes"/"no" translated into true/false
     */
    static bool parse_YesNo(const char* str);

    /**
     * Reinterpret string as Window Function
     * @return WindowFunctionType corresponding to the
     *         string or Cosine2 as default.
     */
    static WindowFunctionType parse_Windowing(const char* str);

    /**
     * Print out a complex vector. Mainly for debugging.
     */
    static void print_vecCplx(float* v, int pts);

   /**
    * Case-insensitive string equality comparison.
    * Works for ASCII strings but not unicode.
    */
    static CicomparisonResult cicompare(const std::string& str1, const std::string& str2);

   /**
    * Calculate and return the greatest common divisor of two integers.
    */
    static long long gcd(long long, long long);
};

#endif // HELPERS_H
