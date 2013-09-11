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

#include "Helpers.h"
#include <string>
#include <iostream>
using std::cerr;
using std::endl;

/**
 * Get system seconds
 * @return double Current count of system seconds
 */
double Helpers::getSysSeconds(void) {
   struct timeval time;
   gettimeofday(&time, NULL);
   return (double)(time.tv_sec) + (double)time.tv_usec * 1.0e-6;
}


/**
 * Convert a number to std::string.
 * Copied from http://cpp.pastebin.com/f154baf81
 */
std::string Helpers::itoa(int n)
{
    char* s = new char[17];
    std::string u;

    if (n < 0) { //turns n positive
        n = (-1 * n);
        u = "-"; //adds '-' on result string
    }

    int i=0; //s counter
    do {
        s[i++]= n%10 + '0'; //conversion of each digit of n to char
        n -= n%10; //update n value
    } while ((n /= 10) > 0);
    for (int j = i-1; j >= 0; j--) {
        u += s[j]; //building our string number
    }
    delete[] s; //free-up the memory!
    return u;
}


/**
 * Reinterpret a "yes" or "no" string as a boolean.
 * @return String "yes"/"no" translated into true/false
 */
bool Helpers::parse_YesNo(const char* str)
{
    if (strcasecmp(str, "yes") == 0 || strcasecmp(str, "true") == 0) {
        return true;
    } else if (strcasecmp(str, "no") == 0 || strcasecmp(str, "false") == 0) {
        return false;
    } else {
        cerr << "Warning: Setting should be yes/no or true/false. Instead we got '" << str << endl;
    }
    return false;
}


/**
 * Reinterpret string as Window Function
 * @return WindowFunctionType corresponding to
 *         the string or Cosine2 as default.
 */
WindowFunctionType Helpers::parse_Windowing(const char* str)
{
   if (strcasecmp(str, "None") == 0) {
        return None;
   } else if (strcasecmp(str, "Cosine") == 0) {
        return Cosine;
   } else if (strcasecmp(str, "Cosine2") == 0) {
        return Cosine2;
   } else if (strcasecmp(str, "Hamming") == 0) {
        return Hamming;
   } else if (strcasecmp(str, "Hann") == 0) {
        return Hann;
   } else if (strcasecmp(str, "Blackman") == 0) {
        return Blackman;
   } else {
        cerr << "Warning: window func  '" << str << "' not supported, will use Cosine2 instead." << endl;
        return Cosine2;
   }
}


/**
 * Print out a complex vector. Mainly for debugging.
 */
void Helpers::print_vecCplx(float* v, int pts)
{
   for (int i=0; i<pts; i++) {
      cerr << v[2*i] << "+i" << v[2*i+1] << " \t";
   }
   cerr << endl << std::flush;
}


/**
 * Case-insensitive string equality comparison.
 * Works for ASCII strings but not unicode.
 */
Helpers::CicomparisonResult Helpers::cicompare(const std::string& str1, const std::string& str2) {
    std::string::const_iterator c1 = str1.begin();
    std::string::const_iterator c2 = str2.begin();
    if ((str1.size() < 1) || (str2.size() < 1)) {
        return NoMatch;
    }
    while (true) {
        if ((c1 == str1.end()) || (c2 == str2.end())) {
           break;
        }
        unsigned char cc1 = *c1;
        unsigned char cc2 = *c2;
        if (tolower(cc1) != tolower(cc2)) {
           return NoMatch;
        }
        ++c1, ++c2;
    }
    if ((c1 == str1.end()) && (c2 == str2.end())) {
        return FullMatch;
    }
    return BeginningsMatch;
}

/**
 * Calculate and return the greatest common divisor of two integers.
 */
long long Helpers::gcd(long long a, long long b)
{
    while (true) {
        a = a%b;
        if (a == 0) {
           return b;
        }
		b = b%a;
        if (b == 0) {
           return a;
        }
    }
}

