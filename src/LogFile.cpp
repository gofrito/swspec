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

#include "LogFile.h"

LogFile::LogFile() : m_fname(std::string("test")), m_verbose(false)
{
}

LogFile::LogFile(std::string fname) : m_fname(fname), m_verbose(false)
{
   m_file.open(fname.c_str(), std::ios::out);
}

LogFile::~LogFile()
{
    if (m_file.is_open()) {
       m_file << " [END]";
       m_file.flush();
       m_file.close();
    }
}

void LogFile::log(std::string msg)
{
    m_file << msg;
}

void LogFile::open(std::string fname)
{
    if (m_file.is_open()) { m_file.close(); }
    m_file.open(fname.c_str(), std::ios::out);
    m_fname = fname;
}

void LogFile::setverbose(bool v)
{
    m_verbose = v;
    // TODO: http://wordaligned.org/articles/cpp-streambufs
}

// LogFile& LogFile::operator=(const LogFile& lf)
// {
//    if (this != &lf) {
//       if (lf.m_file.is_open()) {
//           std::cerr << "Can't overwrite open file " << lf.m_fname << std::endl;
//       }
//       this->m_fname = lf.m_fname;
//       this->m_verbose = lf.m_verbose;
//    }
//    return *this;
// }


/* Following code was taken from http://wordaligned.org/articles/cpp-streambufs */
TeeStream::TeeStream(std::ostream & o1, std::ostream & o2) : std::ostream(&tbuf) , tbuf(o1.rdbuf(), o2.rdbuf())
{
}


