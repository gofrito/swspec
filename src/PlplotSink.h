#ifndef PLPLOTSINK_H
#define PLPLOTSINK_H
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

#include "DataSink.h"
#include "Buffer.h"

#include <plplot/plstream.h> // PLPlot

#include <string>
#include <fstream>

class PlpotSink : public DataSink
{
public:
   PlpotSink(swspect_settings_t* settings) {
      this->pls = NULL;
      this->settings = settings;
      this->title = std::string("Integrated power spectrum");
   }
   PlpotSink(swspect_settings_t* settings, std::string title) {
      this->pls = NULL;
      this->settings = settings;
      this->title = title;
   }
   PlpotSink(std::string uri) {
      this->pls = NULL;
      this->settings = NULL;
      this->title = std::string("Integrated power spectrum");
      open(uri);
   }
   ~PlpotSink() { close(); }

   /**
    * Open the plot
    * @return int Returns 0 on success
    * @param  uri Ignored
    */
   int open(std::string uri);

   /**
    * Write data onto the screen as some graph
    * @return Returns the number of bytes written
    * @param  buf Pointer to the buffer to write out.
    */
   size_t write(Buffer *buf);

   /**
    * Close the plot
    * @return int Returns 0 on success
    */
   int close();

private:
   plstream *pls;
   std::string title;
   swspect_settings_t* settings;

   PLFLT* xdata;
   PLFLT* ydata;
};

#endif // PLPLOTSINK_H
