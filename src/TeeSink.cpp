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

#include "TeeSink.h"
#include <string>


TeeSink::TeeSink(swspect_settings_t* settings)
{
    this->settings = settings;
}

TeeSink::~TeeSink()
{
    this->close();
    sinks.clear();
}


/**
 * Add a further output sink to distribute data to.
 */
int TeeSink::addSink(DataSink* ds)
{
    sinks.push_back(ds);
    return sinks.size();
}

/**
 * Open all sinks, passing the same URI argument
 * @return int Returns 0 on success (no sink failed)
 * @param  uri Argument to sinks
 */
int TeeSink::open(std::string uri)
{
    int fails = 0;
    std::vector<DataSink*>::iterator it = sinks.begin();
    while (it != sinks.end()) {
        if ((*it)->open(uri) != 0) { fails++; }
        ++it;
    }
    return fails;
}

/**
 * Write data into resource
 * @return Returns the number of bytes written
 * @param  buf Pointer to the buffer to write out.
 */
size_t TeeSink::write(Buffer *buf)
{
    size_t max_written = 0;
    std::vector<DataSink*>::iterator it = sinks.begin();
    while (it != sinks.end()) {
        size_t written = (*it)->write(buf);
        if (written > max_written) { max_written = written; }
        ++it;
    }
    return max_written;
}

/**
* Close the resource
* @return int Returns 0 on success
*/
int TeeSink::close()
{
    std::vector<DataSink*>::iterator it = sinks.begin();
    while (it != sinks.end()) {
        (*it)->close();
        ++it;
    }
    return 0;
}

#ifdef UNIT_TEST_TSINK
int main(int argc, char** argv)
{
   return 0;
}
#endif
