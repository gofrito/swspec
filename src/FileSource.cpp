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

#include "FileSource.h"
#include "Helpers.h"
#include <string>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <stdlib.h>
using std::cerr;
using std::endl;

extern "C" { 
    #include <mark5access.h> 
}

/**
 * Open the file
 * @return int Returns 0 on success
 * @param  uri File path
 */
int FileSource::open(std::string uri) 
{
   /* Determine format: with headers, without headers? */
   std::ostream* log = cfg->tlog;
   this->_uri = std::string(uri);
   this->_format = std::string(cfg->sourceformat_str);
   cfg->sourceformat = Unknown;
   if (Helpers::cicompare(cfg->sourceformat_str, "Mark5B") == Helpers::FullMatch) {
      cfg->sourceformat = Mark5B;
      sourceformat_uses_frames = true;
      frame_header_length = 16;     // specified to be always 16B
      frame_payload_length = 10000; // specified to be always 10.000B
   } else if (Helpers::cicompare(cfg->sourceformat_str, "VDIF") == Helpers::FullMatch) {
      cfg->sourceformat = VDIF;
      sourceformat_uses_frames = true;
      frame_header_length = 16;     // 16B if contains extended user info, 8B without user info
      frame_payload_length = 8000;  // depends on VDIF header, default 8k is used on iBOB
   } else if (Helpers::cicompare(cfg->sourceformat_str, "iBob") == Helpers::FullMatch) {
      cfg->sourceformat = iBOB;
      sourceformat_uses_frames = true;
      frame_header_length = 4;      // 64-bit UDP packet sequence#
      frame_payload_length = 4096;  // depends on ibob settings!!
   } else {
      /* Format has headers that overwrite data */
      if (Helpers::cicompare(cfg->sourceformat_str, "MKIV") == Helpers::BeginningsMatch) {
          mk5fileoffset = 0;
          _mk5s = new_mark5_stream( 
                new_mark5_stream_file(_uri.c_str(), mk5fileoffset),
                new_mark5_format_generic_from_string(_format.c_str()) 
          );
          mark5_stream_print(_mk5s);
          if (!_mk5s) {
              *log << "Mark5access ERROR: apparently did not like format " << _format << std::endl;
              return -1;
          }
          cfg->sourceformat = MKIV;
      } else if (Helpers::cicompare(cfg->sourceformat_str, "VLBA") == Helpers::BeginningsMatch) {
          mk5fileoffset = 0;
          _mk5s = new_mark5_stream(
                new_mark5_stream_file(_uri.c_str(), mk5fileoffset),
                new_mark5_format_generic_from_string(_format.c_str()) );
          mark5_stream_print(_mk5s);
          if (!_mk5s) {
              *log << "Mark5access ERROR: apparently did not like format " << _format << std::endl;
              return -1;
          }
          cfg->sourceformat = VLBA;
      } else if (Helpers::cicompare(cfg->sourceformat_str, "Mk5B") == Helpers::BeginningsMatch) {
          mk5fileoffset = 0;
          _mk5s = new_mark5_stream(
                new_mark5_stream_file(_uri.c_str(), mk5fileoffset),
                new_mark5_format_generic_from_string(_format.c_str()) );
          mark5_stream_print(_mk5s);
          if (!_mk5s) {
              *log << "Mark5access ERROR: apparently did not like format " << _format << std::endl;
              return -1;
          }
          cfg->sourceformat = Mk5B;
      }
      /* Format has no headers at all */
      if (Helpers::cicompare(cfg->sourceformat_str, "Maxim") == Helpers::FullMatch) {
          cfg->sourceformat = Maxim;
      } else if (Helpers::cicompare(cfg->sourceformat_str, "RawSigned") == Helpers::FullMatch) {
          cfg->sourceformat = RawSigned;
      } else if (Helpers::cicompare(cfg->sourceformat_str, "RawUnsigned") == Helpers::FullMatch) {
          cfg->sourceformat = RawUnsigned;
      }
      sourceformat_uses_frames = false;
      frame_header_length = 0;
      frame_payload_length = 1;
   }

   /* Show the result */
   if (sourceformat_uses_frames) {
      *log << "FileSource   : processing framed data with " << frame_header_length << "-byte headers and "
                << frame_payload_length << "-byte payloads" << std::endl;
   } else {
      *log << "FileSource   : processing unframed or data-replacement data format" << std::endl;
   }

   /* Open the file */
   if (ifile.is_open()) {
      ifile.close();
   }
   ifile.open(_uri.c_str(), std::ios::binary | std::ios::in);
   if (!ifile.is_open()) {
      *log << "Could not open input file " << uri << std::endl;
      got_eof = true;
      return -1;
   }

   /* Read the header and possibly update 'frame_payload_length' */
   this->locateFirstHeader();
   ifile.seekg(-frame_header_length, std::ios_base::cur);
   
   /* Skip the first part of data? */
   if (cfg->seconds_to_skip > 0) {
       if (cfg->sourceformat == VLBA || cfg->sourceformat == MKIV || cfg->sourceformat == Mk5B ) {
           this->reopen_mark5_stream(cfg->seconds_to_skip);
       } else {
           std::ifstream::pos_type smp_bytes, seek_pos, frames;
           smp_bytes = (cfg->bits_per_sample * cfg->source_channels * cfg->samplingfreq) / 8;
           smp_bytes = smp_bytes * cfg->seconds_to_skip;
           seek_pos  = first_header_offset;
           if (sourceformat_uses_frames) {
               frames     = std::ceil(smp_bytes / frame_payload_length);
               seek_pos += frames * (frame_header_length + frame_payload_length);
           } else {
               frames    = 0;
               seek_pos += smp_bytes;
           }
           *log << "Skipping " << cfg->seconds_to_skip << " sample-seconds: " 
                << smp_bytes << " sample bytes correspods to " << frames << " frames, "
                << "seeking to offset " << seek_pos << std::endl;
           ifile.seekg(seek_pos, std::ios_base::cur);
           first_header_offset = seek_pos; // this will be used for modulo frame-boundary detection
       }
   }

   /* Show the first header, if any */
   if (sourceformat_uses_frames) {
       this->inspectAndConsumeHeader();
       ifile.seekg(-frame_header_length, std::ios_base::cur);
   }

   got_eof = false;
   return 0;
}

/**
 * Try to fill the entire bufferspace with new data and return the number of bytes actually read
 * @return int    Returns the amount of bytes read
 * @param  bspace Pointer to Buffer to fill out
 */
int FileSource::read(Buffer* buf) 
{
   int nread;
   std::ostream* log = cfg->tlog;

   /* Sanity */
   if (!ifile.is_open() || NULL==buf || this->cfg==NULL) {
      nread = 0;
      buf->setLength(nread);
      std::cerr << "FileSource::read() attempt to read in illegal state" << std::endl;
      return nread;
   } 

   /* Formats with headers that do not overwrite data */
   if (sourceformat_uses_frames) {

       int nreq  = buf->getAllocated();
       char* dst = buf->getData();
       const int framesize = frame_header_length + frame_payload_length;

       while ((nreq > 0) && !got_eof) {

          /* At frame start boundary: decode & skip header */
          std::streampos curr = ifile.tellg();
          if ((curr % framesize) == 0) {
             this->inspectAndConsumeHeader();
             continue;
          }

          /* Within frame: grab all data before start of next frame */
          int available = framesize - (curr % framesize);
          ifile.read(dst, available);
          if (ifile.fail() && !ifile.eof()) {
             *log << "Read I/O error!" << std::endl << std::flush;
          }
          if (!ifile || !ifile.good()) {
             got_eof = true;
          }
          nread = ifile.gcount();

          /* Advance */
          dst  += nread;
          nreq -= nread;
       }
       nread = buf->getAllocated();
   }

   /* Formats without headers or with headers that overwrite data */
   if (!sourceformat_uses_frames) {

       if (false && (cfg->sourceformat == VLBA || cfg->sourceformat == MKIV)) {
           /* Removed for the time being! mark5_stream_copy is buggy... 
            * We use normal ifile.read() instead. See reopen_mark5_stream() for ifile.seekg().
            */
           int res = mark5_stream_copy(_mk5s, buf->getAllocated(), buf->getData());
           nread = buf->getAllocated();
           if (res != 0) {
              int bytegranularity = _mk5s->samplegranularity * _mk5s->nbit * _mk5s->nchan / 8;
              *log << "mark5_stream_copy returned error (or EOF!) - check that bufsize " << buf->getAllocated() 
                   << " matches granularity " << bytegranularity << std::endl;
              nread = 0;
              got_eof = true;
           }
       } else {
           ifile.read(buf->getData(), buf->getAllocated());
           if (ifile.fail() && !ifile.eof()) {
              *log << "Read I/O error!" << std::endl << std::flush;
           }
           if (!ifile || !ifile.good()) {
              got_eof = true;
           }
           nread = ifile.gcount();
           if ((size_t)nread != buf->getAllocated() && !got_eof) {
              *log << "Read " << nread << " bytes instead of " << buf->getAllocated() << std::endl;
           }
       }
   }

   buf->setLength(nread);
   return nread;
}

/**
 * Close the resource
 * @return int
 */
int FileSource::close() 
{
   ifile.close();
   return 0;
}

/**
 * Check for end of file
 * @return bool EOF
 */
bool FileSource::eof()
{
   return got_eof;
}


/**
 * On an open stream, scan for the first occurence of a format-dependant
 * data header and set the first_header_offset variable to it.
 * For headerless formats first_header_offset is set to 0.
 */
void FileSource::locateFirstHeader()
{
   std::ostream* log = cfg->tlog;

   if (!sourceformat_uses_frames) {

      /* Headerless in the sense of data-replacement headers */
      if (cfg->sourceformat == VLBA || cfg->sourceformat == MKIV || cfg->sourceformat == Mk5B ) {
           first_header_offset = 0;  // unused
           frame_payload_length = 0; // unused

           int mjd = 0, sec = 0;
           double ns = 0;
           mark5_stream_get_sample_time(_mk5s, &mjd, &sec, &ns);
           if (ns > 0.0f) {
               *log << "First frame not at an integer second. "
                    << "Re-opening first integer second and skipping additional " 
                    << cfg->seconds_to_skip << "s." << std::endl;
               int old_sec = sec;
               // doesn't work: mark5_stream_seek(_mk5s, mjd, sec + 1 + cfg->seconds_to_skip, 0.0f);
               this->reopen_mark5_stream(cfg->seconds_to_skip);
               mark5_stream_get_sample_time(_mk5s, &mjd, &sec, &ns);
               if ((old_sec+1) != sec || (ns != 0.0f)) {
                   *log << "Warning: mark5_stream_get_sample_time gave wrong second" << std::endl;
               }
           }

           *log << "VLBA/MkIV    : starting at " << mjd << " MJD " << sec << " sec + " << ns << " ns" << std::endl;
           cfg->logIO->stream() << "// VLBA/MkIV start time <MJD sec ns>" << std::endl;
           cfg->logIO->stream() << std::fixed << mjd << " " << sec << " " << ns << std::endl;

      /* Truly raw data or we just don't care about headers */          
      } else {
          cfg->logIO->stream() << "// No timestamps extracted from input data" << std::endl;
          first_header_offset = 0;

      }

   } else {

      /* With headers */
      if (cfg->sourceformat == Mark5B) {
          // we keep life simple and assume a smart recording tool
          // has been used, such that the first header always
          // begins at byte 0!
          // MJD_0 is the 0 position of the truncated MJD
          first_header_offset = 0;
          int mjd = 0, sec = 0;
          double ns = 0;
          int MJD_0 = 56000;
          unsigned char header[frame_header_length+1];

          if (!ifile.is_open()) { return; }
          if (frame_header_length<=0 || !sourceformat_uses_frames) { return; }

          ifile.read((char*)&header[0], frame_header_length);
          if (!ifile || !ifile.good()) {
              got_eof = true;
              return;
          }

          mjd = MJD_0 + int(header[2*4+3]>>4)*100 + int(header[2*4+3]&0x0F)*10 + int(header[2*4+2]>>4);
          sec = int(header[2*4+2]&0x0F)*10000 + int(header[2*4+1]>>4)*1000 + int(header[2*4+1]&0x0F)*100 + int(header[2*4+0]>>4)*10 + int(header[2*4+0]&0x0F);

          *log << "Mark5B       : starting at " << mjd << " MJD " << sec << " sec + " << ns << " ns" << std::endl;
          cfg->logIO->stream() << "// Mark5B start time <MJD sec ns>" << std::endl;
          cfg->logIO->stream() << std::fixed << mjd << " " << sec << " " << ns << std::endl;

      } else if(cfg->sourceformat == VDIF) {
         // we keep life simple and assume a smart recording tool
         // has been used, such that the first header always
         // begins at byte 0!
   	     first_header_offset = 0;
         frame_payload_length = 8000; // ?? -- depends on VDIF header
         cfg->logIO->stream() << "// No timestamps extracted from input data" << std::endl;
      } else if (cfg->sourceformat == iBOB) {
   	     first_header_offset = 0;
          cfg->logIO->stream() << "// No timestamps extracted from input data" << std::endl;
      } else {
         *log << "FileSource::locateFirstHeader: don't know how to handle '"
              << cfg->sourceformat << "'!" << std::endl;
         cfg->logIO->stream() << "// No timestamps extracted from input data" << std::endl;
      }

   }
}


/**
 * Assumes a header starts at the current read offset.
 * Reads the header and parses it. The read offset
 * after this function returns contains data.
 */
void FileSource::inspectAndConsumeHeader()
{
   if (!ifile.is_open()) { return; }
   if (frame_header_length<=0 || !sourceformat_uses_frames) { return; }

   unsigned char header[frame_header_length+1];
   ifile.read((char*)&header[0], frame_header_length);
   if (!ifile || !ifile.good()) {
       got_eof = true;
       return;
   }

   if (cfg->sourceformat == Mark5B) {
      // http://www.atnf.csiro.au/vlbi/wiki/index.php?n=EXPReS.Mark5BFormat
      // Data on disk is divided into equal-length disk frames (DF). Each DF has
      // a header of 4 32-bit words (4*4=16 bytes) followed by 2500 32-bit words of data
      // (10000 bytes). The DF boundary is aligned with the UT second tick.
      // Word 0   Synchronization word (0xABADDEED)
      // Word 1   Bits 31-16: User specifed,  Bit 15: T - tvg data (test data if set),
      //          Bits 14-0: DF # within second
      // Word 2-3 VLBA BCD Time code and 16 bit CRC
      //
      // Words are stored MSB first, LSB last. So the byte stream is [w0MSB w0xSB w0xSB w0LSB | ... ]
      //
      // Samples are supposedly stored like:
      // "Raw VLBI data is packed within a 32 bit word, with the earliest time sample corresponding to the least significant bits."
      // => [oldest newest] [oldest newest] ...
      //
      // The above does not match official(?) ftp://web.haystack.edu/pub/mark5/005.2.pdf
      // where the format is totally different, the sync word is '111111..
      bool show = false;
      if (header[0*4+3]!=0xAB && header[0*4+2]!=0xAD && header[0*4+1]!=0xDE && header[0*4+0]!=0xED) {
           std::cerr << "Mark5B: incorrect header at file offset 0x" << ifile.tellg() << std::endl;
           show = true;
      }
      if (show) {
      std::cerr << "Mark5B header: 0xABADDEED ?= { "
           << std::hex << std::setw(2) << std::setfill('0') << int(header[0*4+3])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[0*4+2])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[0*4+1])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[0*4+0])
           << " }, user = { "
           << std::hex << std::setw(2) << std::setfill('0') << int(header[1*4+3])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[1*4+2])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[1*4+1])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[1*4+0])
           << " }, timecode = { "
           << std::hex << std::setw(2) << std::setfill('0') << int(header[2*4+3])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[2*4+2])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[2*4+1])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[2*4+0])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[3*4+3])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[3*4+2])
           << " }, crc = { "
           << std::hex << std::setw(2) << std::setfill('0') << int(header[3*4+1])
           << std::hex << std::setw(2) << std::setfill('0') << int(header[3*4+0])
           << " }; " << std::endl;
      }
   } else if (cfg->sourceformat == iBOB) {
         // Sergei format: 4 bytes Unix timestamp, 4 bytes packet seq nr starting from 1
         long second = (((long)header[0])<<24) + (((long)header[1])<<16) + (((long)header[2])<<8) + header[3];
         long frame = (((long)header[4])<<24) + (((long)header[5])<<16) + (((long)header[6])<<8) + header[7];
         if (frame==1) {
             std::cerr << "iBOB sec=" << second << " frame=" << frame << std::endl;
         }
   }
}


/**
 * Workaround for mark5acces mark5_stream_seek() bugs.
 * Call this after seeking to the desired timestamp. 
 * Determines the current file offset and re-opens
 * the stream at this offset.
 */
void FileSource::reopen_mark5_stream(int secondstoskip)
{
    if (_mk5s == NULL) return;

    /* Figure out the offset mainly by guessing:
     * The earlier seek causes mark5access to refill its internal buffer, hence file offset
     * is at the end of that read buffer. Further we remove almost half a frame, so that
     * on reopening the (hopefully) correct first integer-second frame is detected.
     */
    int64_t nsToGo   = (_mk5s->ns == 0)   ? 0 : 1000000000ULL - _mk5s->ns;
    int64_t nsToSkip = (secondstoskip<=0) ? 0 : 1000000000ULL*secondstoskip;
    int64_t nFrameOffset = (nsToGo + nsToSkip) / _mk5s->framens;
    mk5fileoffset = mk5fileoffset + _mk5s->framebytes * nFrameOffset + _mk5s->frameoffset;
    if (_mk5s->payloadoffset < 0) {
        /* we have to seek back a little to avoid a mark5access decode bug */
        mk5fileoffset += _mk5s->payloadoffset;
    }
    *(cfg->tlog) << "FileSource::reopen_mark5_stream: new offset after skipping additional " 
                 << secondstoskip << "s is " << mk5fileoffset << std::endl;

    /* Create new stream */
    delete_mark5_stream(_mk5s);
    _mk5s = new_mark5_stream(
            new_mark5_stream_file(_uri.c_str(), mk5fileoffset),
            new_mark5_format_generic_from_string(_format.c_str())
    );
    if (!_mk5s) {
        *(cfg->tlog) << "new_mark5_stream failed!" << std::endl;
    }
    mark5_stream_print(_mk5s);

    /* Seek in the ifstream as well, to what _should_ be the start of a frame */
    ifile.seekg(mk5fileoffset, std::ios_base::beg);

    return;
}

#ifdef UNIT_TEST_FSOURCE
int main(int argc, char** argv)
{
   return 0;
}
#endif
