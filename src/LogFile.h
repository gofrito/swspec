#ifndef LOGFILE_H
#define LOGFILE_H
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

#include <streambuf>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

class TeeBuf;
class TeeStream;

class LogFile
{
  public:
   LogFile();
   LogFile(std::string name);
   ~LogFile();

  private:
   virtual LogFile& operator=(const LogFile&) { return *this; }

  public:
   void log(std::string);
   std::fstream& stream() { return m_file; }
   void open(std::string);
   void setverbose(bool);

  private:
    std::fstream m_file;
    std::string  m_fname;
    bool m_verbose;
};


/* Following code was taken from http://wordaligned.org/articles/cpp-streambufs */
class TeeBuf : public std::streambuf
{
public:
    // Construct a streambuf which tees output to both input
    // streambufs.
    TeeBuf(std::streambuf * sb1, std::streambuf * sb2) : sb1(sb1), sb2(sb2) { }
private:
    // This tee buffer has no buffer. So every character "overflows"
    // and can be put directly into the teed buffers.
    virtual int overflow(int c) {
        if (c == EOF) { return !EOF; }
        else {
            int const r1 = sb1->sputc(c);
            int const r2 = sb2->sputc(c);
            return (r1 == EOF || r2 == EOF) ? EOF : c;
        }
    }
    
    // Sync both teed buffers.
    virtual int sync() {
        int const r1 = sb1->pubsync();
        int const r2 = sb2->pubsync();
        return r1 == 0 && r2 == 0 ? 0 : -1;
    }   
private:
    std::streambuf * sb1;
    std::streambuf * sb2;
};

class TeeStream : public std::ostream
{
public:
    // Construct an ostream which tees output to the supplied
    // ostreams.
    TeeStream(std::ostream & o1, std::ostream & o2);
private:
    TeeBuf tbuf;
};


#endif // LOGFILE_H
