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
#include "DataUnpackers.h"

#include <cstdlib>
#include <iostream>
using std::cerr;
using std::endl;

#include <ippcore.h>
#include <ipps.h>
#include <mark5access.h>
#include <malloc.h>

//#define WRITE_DUMP x
#ifdef WRITE_DUMP
static std::ofstream fout("dump.bin");
#endif

///////////////////////////////////////////////////////////////////////////
// Simple 8-bit and 16-bit data unpacking
///////////////////////////////////////////////////////////////////////////

/**
 * Signed data to float unpacking.
 * @param src     raw input data
 * @param dst     destination of unpacked floatingpoint data
 * @param count   how many samples to unpack
 * @param channel the channel to use, 0..nchannels-1
 * @return how many samples were unpacked
 */
struct HexCharStruct
{
  unsigned char c;
  HexCharStruct(unsigned char _c) : c(_c) { }
};

inline std::ostream& operator<<(std::ostream& o, const HexCharStruct& hs)
{
  return (o << std::hex << (int)hs.c);
}

inline HexCharStruct hex(unsigned char _c)
{
  return HexCharStruct(_c);
}


size_t SignedUnpacker::extract_samples(char const* const src, Ipp32f* dst, const size_t count, const int channel) const
{
   size_t rc = 0, smp = 0;

   if (cfg->bits_per_sample == 8) {
       /* 8-bit data */
       signed char const* src8 = (signed char const*)src;
       src8 += channel;
       for (smp=0; smp<count; ) {
           const int unroll_factor = 8;
           for (char off=0; off<unroll_factor; off++) {
               dst[smp+off] = (Ipp32f) (*(src8 + off * cfg->source_channels));
           }
           src8 += unroll_factor * cfg->source_channels;
           smp  += unroll_factor;
       }
   } else {
       /* 16-bit data */
       signed short const* src16 = (signed short const*)src;
       src16 += channel;
       for (smp=0; smp<count; ) {
           const int unroll_factor = 8;
           for (char off=0; off<unroll_factor; off++) {
               #if 0
               signed short ptr = src16 + off * cfg->source_channels;
               char* s8ins = (char*)ptr;
               char tmp = s8ins[0]; s8ins[0] = s8ins[1]; s8ins[0] = tmp; // swap endianness
               dst = (Ipp32f)(*ptr);
               #else
               dst[smp+off] = (Ipp32f) (*(src16 + off * cfg->source_channels));
               #endif
           }
           src16 += unroll_factor * cfg->source_channels;
           smp   += unroll_factor;
       }
   }
   rc = smp;
   if (rc != count) {
       cerr << "Unpacked " << rc << " samples instead of " << count << " because of better unroll factor. " << endl;
   }
   return rc;
}

bool SignedUnpacker::canHandleConfig(swspect_settings_t const* settings)
{
    return (settings->bits_per_sample == 8 || settings->bits_per_sample == 16);
}


/**
 * Unsigned data to float unpacking. Data is shifted so that 128 becomes 0.
 * @param src     raw input data
 * @param dst     destination of unpacked floatingpoint data
 * @param count   how many samples to unpack
 * @param channel the channel to use, 0..nchannels-1
 * @return how many samples were unpacked
 */
size_t UnsignedUnpacker::extract_samples(char const* const src, Ipp32f* dst, const size_t count, const int channel) const
{
   size_t rc = 0, smp = 0;

   if (cfg->bits_per_sample == 8) {
       /* 8-bit data */
       unsigned char const* src8 = (unsigned char const*)src;
       src8 += channel;
       for (smp=0; smp<count; ) {
           const int unroll_factor = 8;
           for (char off=0; off<unroll_factor; off++) {
               dst[smp+off] = (Ipp32f) (*(src8 + off * cfg->source_channels)) - 128.0f;
           }
           src8 += unroll_factor * cfg->source_channels;
           smp  += unroll_factor;
       }
   } else {
       /* 16-bit data */
       unsigned short const* src16 = (unsigned short const*)src;
       src16 += channel;
       for (smp=0; smp<count; ) {
           const int unroll_factor = 8;
           for (char off=0; off<unroll_factor; off++) {
               #if 0
               unsigned short ptr = src16 + off * cfg->source_channels;
               char* s8ins = (char*)ptr;
               char tmp = s8ins[0]; s8ins[0] = s8ins[1]; s8ins[0] = tmp; // swap endianness
               dst = (Ipp32f)(*ptr) - 32768.0f;
               #else
               dst[smp+off] = (Ipp32f) (*(src16 + off * cfg->source_channels)) - 32768.0f;
               #endif
           }
           src16 += unroll_factor * cfg->source_channels;
           smp   += unroll_factor;
       }
   }
   rc = smp;
   if (rc != count) {
       cerr << "Unpacked " << rc << " samples instead of " << count << " because of better unroll factor. " << endl;
   }
   return rc;
}

bool UnsignedUnpacker::canHandleConfig(swspect_settings_t const* settings)
{
    return (settings->bits_per_sample == 8 || settings->bits_per_sample == 16);
}




///////////////////////////////////////////////////////////////////////////
// 2-bit formats
///////////////////////////////////////////////////////////////////////////

/**
 * Raw 2-bit data to float unpacker. Handles data layouts where samples from
 * every channel are packed together and are samples always in the same order.
 * Assumes samples are packed in network byte order.
 *
 * @param src     raw input data
 * @param dst     destination of unpacked floatingpoint data
 * @param count   how many samples to unpack
 * @param channel the channel to use, 0..channels-1
 * @return how many samples were unpacked
 */
size_t TwoBitUnpacker::extract_samples(char const* const src, Ipp32f* dst, const size_t count, const int channel) const
{
    static Ipp32f precooked_LUT[256];
    static int    precook_done = 0;
    static int    prev_channel = -1;
    size_t rc;

    // Data format:
    //   4 channels :  8-bit : [MSB .. LSB] : [ch3msb ch3lsb ch2msb ch2lsb ch1msb ch1lsb ch0msb ch0lsb]
    //   8 channels : 16-bit : [MSB .. LSB] : [byte0] [byte1] : big endian : [ch7msb ch7lsb .. ch0msb ch0lsb ]

    /* build lookup */
    if (!precook_done || (channel != prev_channel)) {
        unsigned char shift;
        if (cfg->channelorder_increasing) {
            shift = (2 * (channel%4)); // 0,2,4,6
        } else {
            shift = 6 - (2 * (channel%4)); // 6,4,2,0
        }
        unsigned char mask   = (unsigned char)3 << shift;
        // const Ipp32f  map[4] = { 1.0, 3.3359, -1.0, -3.3359 }; // {s,m} : 00,01,10,11 : direct sign/mag bits
        const Ipp32f  map_rev[4] = { 1.0, -1.0, 3.3359, -3.3359 }; // {m,s} : 00,01,10,11 : direct sign/mag bits
        for (int i=0; i<256; i++) {
            unsigned char s = (i & mask) >> shift;
            precooked_LUT[i] = map_rev[s];
        }
        precook_done = 1;
        prev_channel = channel;
        // TODO: for cross-pol or two input files, each file should be assigned its own unpacker! otherwise have to rebuild the LUT for every unpack!
    }

    /* unpack using the lookup table */
    int step = cfg->source_channels / 4; // 4ch=>1byte, 8ch=>2byte, 12ch=>3byte etc
    unsigned char const* src8;
    if (cfg->channelorder_increasing) {
        src8 = (unsigned char const*)src + channel/4;
    } else {
        src8 = (unsigned char const*)src + (step-1) - channel/4; // src + 0..1 - 0..1
    }
    size_t smp;
    for (smp=0; smp<count; ) {
       const int unroll_factor = 8;
       for (char off=0; off<unroll_factor; off++) {
           dst[smp+off] = precooked_LUT[ *(src8+off*step) ];
       }
       src8 += unroll_factor*step;
       smp  += unroll_factor;
    }
    rc = smp;
    if (rc != count) {
        cerr << "Unpacked " << rc << " samples instead of " << count << " because of better unroll factor. " << endl;
    }
    return rc;
}

bool TwoBitUnpacker::canHandleConfig(swspect_settings_t const* settings)
{
    return ((settings->bits_per_sample == 2) && ((settings->source_channels % 4) == 0));
}

/**
 * Raw 2-bit data to float unpacker. Handles the data layout where we have only
 * a single channel. The oldest sample is in the MSB, the newest in the LSB.
 *
 * @param src     raw input data
 * @param dst     destination of unpacked floatingpoint data
 * @param count   how many samples to unpack
 * @param channel the channel to use, always 0 (argument kept for API compatibility)
 * @return how many samples were unpacked
 */
size_t TwoBitSinglechannelUnpacker::extract_samples(char const* const src, Ipp32f* dst, const size_t count, const int channel) const
{
    static Ipp32f precooked_LUT[256][4];
    static int    precook_done = 0;
    size_t rc;

    // Data format:
    // 1 channel  :  2-bit : [MSB]        : [ch1s1 ch1s2 ch1s3 ch1s4]
	// assert ( (count % 4) == 0); // number of samples must divide by 4

    /* build lookup */
    if (!precook_done) {
        // const Ipp32f  map[4] = { 1.0, 3.3359, -1.0, -3.3359 }; // {s,m} : 00,01,10,11 : direct sign/mag bits
        const Ipp32f  map_rev[4] = { 1.0, -1.0, 3.3359, -3.3359 }; // {m,s} : 00,01,10,11 : direct sign/mag bits
        for (int byte=0; byte<256; byte++) {
            for (unsigned char samplenr=0; samplenr<4; samplenr++) {
                unsigned char s = (byte & (128+64)) >> ((3-samplenr)*2);
                precooked_LUT[byte][samplenr] = map_rev[s];
            }
        }
        precook_done = 1;
        // TODO: for cross-pol or two input files, each file should be assigned its own unpacker! otherwise have to rebuild the LUT for every unpack!
    }

    /* unpack using the lookup table */
    unsigned char const* src8 =(unsigned char const*)src;
    size_t smp;
    for(smp=0; smp<count; ) {
        const int unroll_factor = 4;
        for (char off=0; off<unroll_factor; off++) {
           unsigned char val = *(src8+off);
           dst[smp+4*off+0] = precooked_LUT[val][0];
           dst[smp+4*off+1] = precooked_LUT[val][1];
           dst[smp+4*off+2] = precooked_LUT[val][2];
           dst[smp+4*off+3] = precooked_LUT[val][3];
        }
        src8 += unroll_factor;
        smp  += 4*unroll_factor;
     }
     rc = smp;
     if (rc != count) {
         cerr << "Unpacked " << rc << " samples instead of " << count << " because of better unroll factor. " << endl;
     }
     return rc;
}

bool TwoBitSinglechannelUnpacker::canHandleConfig(swspect_settings_t const* settings)
{
    return ((settings->bits_per_sample == 2) && (settings->source_channels == 1));
}


/**
 * Mark5B four or eight channel 2-bit data raw data to float unpacker.
 * It uses own made lookup table
 * @param src     raw input data
 * @param dst     destination of unpacked floatingpoint data
 * @param count   how many samples to unpack
 * @param channel the channel to use, 0..nchannels-1
 * @return how many samples were unpacked
 */
size_t Mk5BUnpacker::extract_samples(char const* const src, Ipp32f* dst, const size_t count, const int channel) const
{
    static Ipp32f precooked_LUT[256];
    static int    precook_done = 0;
    size_t rc;
    size_t smp;

    if (cfg->source_channels == 2) {
     if (!precook_done) {
        unsigned char shift = 2 * (channel%2); // 0,2
	    unsigned char shift2 =  2 * ((channel+2)%2);
        unsigned char mask  = (unsigned char)3 << shift;
        const float map_rev[4] = { +1.0, -1.0, +3.3359, -3.3359 }; // {m,s} : 00,01,10,11 : reversed sign/mag bits
        for (int i=0; i<128; i++) {
            unsigned char s1 = (i & mask) >> shift;
            unsigned char s2 = (i & mask) >> shift2;
            precooked_LUT[2*i]   = map_rev[s1];
	    precooked_LUT[2*i+1] = map_rev[s2];
        }
        precook_done = 1;
     }

     /* unpack using the lookup table */
     unsigned short const* src16 = (unsigned short const*)src + channel/8; // byte offset
     int step = cfg->source_channels / 8; // 1 or 2
     for (smp=0; smp<count; ) {
        const int unroll_factor = 8;
        for (short off=0; off<unroll_factor; off++) {
            dst[smp+off] = precooked_LUT[*(src16+off*step)];
        }
        src16 += unroll_factor*step;
        smp  += unroll_factor;
     }
    }

    /* build lookup */
    if (cfg->source_channels == 4) {
     if (!precook_done) {
        unsigned char shift = 2 * (channel%4); // 0,2,4,6
        unsigned char mask  = (unsigned char)3 << shift;
        const float map_rev[4] = { +1.0, -1.0, +3.3359, -3.3359 }; // {m,s} : 00,01,10,11 : reversed sign/mag bits
        for (int i=0; i<256; i++) {
            unsigned char s = (i & mask) >> shift;
            precooked_LUT[i] = map_rev[s];
        }
        precook_done = 1;
     }

     /* unpack using the lookup table */
     unsigned char const* src8 = (unsigned char const*)src + channel/4; // byte offset
     int step = cfg->source_channels / 4; // 1 or 2
     for (smp=0; smp<count; ) {
         const int unroll_factor = 8;
         for (char off=0; off<unroll_factor; off++) {
            dst[smp+off] = precooked_LUT[*(src8+off*step)];
        }
        src8 += unroll_factor*step;
        smp  += unroll_factor;
     }
    }


    if (cfg->source_channels == 8) {
     if (!precook_done) {
        unsigned short shift = 2 * (channel%4);
        unsigned short mask  = (unsigned short)3 << shift;
        const float map_rev[4] = { +1.0, -1.0, +3.3359, -3.3359 };
        for (int i=0; i<256; i++) {
            unsigned char s = (i & mask) >> shift;
            precooked_LUT[i] = map_rev[s];
        }
        precook_done = 1;
     }

     /* unpack using the lookup table */
     unsigned char const* src8 = (unsigned char const*)src + channel/4;
     int step = cfg->source_channels / 4;
     for (smp=0; smp<count; ) {
        const int unroll_factor = 8;
        for (char off=0; off<unroll_factor; off++) {
            dst[smp+off] = precooked_LUT[*(src8+off*step)];
        }
        src8 += unroll_factor*step;
        smp  += unroll_factor;
     }
    }

    else if (cfg->source_channels == 16) {
     if (!precook_done) {
        unsigned int shift = 2 * (channel%16); // 0,2,4,6,8,10,12,14
        unsigned int mask  = (unsigned int)3 << shift;
        const float map_rev[4] = { +1.0, -1.0, +3.3359, -3.3359 }; // {m,s} : 00,01,10,11 : reversed sign/mag bits
        for (int i=0; i<256; i++) {
            unsigned int s = (i & mask) >> shift;
            precooked_LUT[i] = map_rev[s];
        }
        precook_done = 1;
    }

     /* unpack using the lookup table */
     unsigned short const* src32 = (unsigned short const*)src + channel/16; // byte offset
     int step = cfg->source_channels / 16; // 1 or 2
     cerr << step << endl;
     for (smp=0; smp<count; ) {
        const int unroll_factor = 32;
        for (int off=0; off<unroll_factor; off++) {
            cerr << src32+step*off << endl;
            dst[smp+off] = precooked_LUT[*(src32+off*step)];
	    cerr << smp + off << ":" <<  off + step << endl;
        }
        src32 += unroll_factor*step;
        smp   += unroll_factor;
     }
    }

    rc = smp;
    if (rc != count) {
        cerr << "Unpacked " << rc << " samples instead of " << count << " because of better unroll factor. " << endl;
    }
    return rc;
}

bool Mk5BUnpacker::canHandleConfig(swspect_settings_t const* settings)
{
    return ((settings->bits_per_sample == 2) && (settings->source_channels % 2) == 0));
}



///////////////////////////////////////////////////////////////////////////
// Perverted formats
///////////////////////////////////////////////////////////////////////////

/**
 * VLBA raw data to float unpacker.
 * Initialize things required by the mark5access library.
 */
VLBAUnpacker::VLBAUnpacker(swspect_settings_t const* settings)
{
    /* Preps */
    cfg = settings;
    size_t count = cfg->fft_points;

   /* <mark5acess/docs/UserGuide>
    * 3.2.3.1 struct mark5_stream_generic *new_mark5_stream_unpacker(int noheaders)
    * This function makes a new pseudo-stream.  "noheaders" should be set to 0
    * if the data to be later unpacked is in its original form and should be 1
    * if the data to be later unpacked has its headers stripped.  When used in
    * conjunction with mark5_stream_copy, "noheaders" should be set to 1.
    */
    ms = new_mark5_stream(
         new_mark5_stream_unpacker(/*noheaders:*/1),
         new_mark5_format_generic_from_string(cfg->sourceformat_str.c_str())
    );
    if (!ms) {
        cerr << "Mark5access library did not like the format '" << cfg->sourceformat_str << "'" << endl;
        ms = NULL; //and then crash
    }

    /* mark5access limitation : all channels are unpacked, need lots of memory */
    allchannels = (Ipp32f**)malloc(ms->nchan * sizeof(float*));
    cerr << "VLBAUnpacker allocating " << (ms->nchan*count*sizeof(float))/1024.0 << "kB of mark5access buffers." << endl;
    for (int ch=0; ch < ms->nchan; ch++) {
        allchannels[ch] =  (float *)memalign(128, count * sizeof(float));
        if (allchannels[ch] == NULL) {
            cerr << "Malloc of " <<  (count*sizeof(float)) << " bytes for temp channel " << ch
                 << " out of " << ms->nchan << " channels failed!" << endl;
         }
    }
}

/**
 * VLBA raw data to float unpacker.
 * @param src     raw input data
 * @param dst     destination of unpacked floatingpoint data
 * @param count   how many samples to unpack
 * @param channel the channel to use, 0..nchannels-1
 * @return how many samples were unpacked
 */
size_t VLBAUnpacker::extract_samples(char const* const src, Ipp32f* dst, const size_t count, const int channel) const
{
    const size_t payloadbytes = (ms->payloadoffset>0) ? (ms->framebytes - ms->payloadoffset) : ms->framebytes;

    if (channel >= ms->nchan) {
        cerr << "Source has only " << ms->nchan << " channels but settings request channel " << (channel+1) << endl;
    }

    /* Unpack and copy the desired channel */
    #if 1 // TODO: multiply payloadbytes by fanout
    const size_t mark5access_max_size = 2*payloadbytes; // use 2^17 (~2^18 is the maximum due to some mark5access bug/limitation)
    size_t left = count;
    char const* mk5src = src;
    Ipp32f* mk5dst = dst;
    while (left > 0) {
        size_t to_unpack = std::min(mark5access_max_size, left);
        mark5_unpack(ms, (void*)mk5src, allchannels, to_unpack);
        ippsCopy_32f(allchannels[channel % ms->nchan], mk5dst, to_unpack);
        mk5src += to_unpack;
        mk5dst += to_unpack;
        left -= to_unpack;
    }
    #else
    mark5_unpack(ms, (void *)src, allchannels, count);
    ippsCopy_32f(allchannels[channel % ms->nchan], dst, count);
    #endif

    /* VLBA is non data replacement so we don't need to add randomness to "header" locations */
    return count;
}

bool VLBAUnpacker::canHandleConfig(swspect_settings_t const* settings)
{
    return ((settings->bits_per_sample == 2 || settings->bits_per_sample == 1) && ((settings->source_channels % 2) == 0));
}

/**
 * MarkIV raw data to float unpacker.
 * Initialize things required by the mark5access library.
 */
MarkIVUnpacker::MarkIVUnpacker(swspect_settings_t const* settings) 
{ 
    /* Preps */
    cfg = settings;
    size_t count = cfg->fft_points;
    fanout = *(cfg->sourceformat_str.c_str() + 6) - '0'; // x from "MKIV1_x-..."

   /* <mark5acess/docs/UserGuide>
    * 3.2.3.1 struct mark5_stream_generic *new_mark5_stream_unpacker(int noheaders)
    * This function makes a new pseudo-stream.  "noheaders" should be set to 0
    * if the data to be later unpacked is in its original form and should be 1
    * if the data to be later unpacked has its headers stripped.  When used in
    * conjunction with mark5_stream_copy, "noheaders" should be set to 1.
    */
    ms = new_mark5_stream(
         new_mark5_stream_unpacker(/*noheaders*/1),
         new_mark5_format_generic_from_string(cfg->sourceformat_str.c_str())
    );
    if (!ms) {
        cerr << "Mark5access library did not like the format '" << cfg->sourceformat_str << "'" << endl;
        ms = NULL; //and then crash
    }

    /* mark5access limitation : all channels are unpacked, need lots of memory */
    allchannels = (Ipp32f**)malloc(ms->nchan * sizeof(float*));
    cerr << "number of channels " << ms->nchan << endl;
    cerr << "MarkIVUnpacker allocating " << (ms->nchan*count*sizeof(float))/1024.0 << "kB of mark5access buffers." << endl;
    for (int ch=0; ch < ms->nchan; ch++) {
        allchannels[ch] =  (float *)memalign(128, count * sizeof(float));
        if (allchannels[ch] == NULL) {
            cerr << "Malloc of " <<  (count*sizeof(float)) << " bytes for temp channel " << ch
                 << " out of " << ms->nchan << " channels failed!" << endl;
         }
    }

    /* fill the output arrays with noise */
    for (int ch=0; ch < ms->nchan; ch++) {
        for (size_t n=0; n < count; n++) {
            allchannels[ch][n] = float(2*(std::rand()%2) - 1); // <= +-1.0f, TODO: use something better
        }
    }
}

/**
 * MarkIV raw data to float unpacker.
 * @param src     raw input data
 * @param dst     destination of unpacked floatingpoint data
 * @param count   how many samples to unpack
 * @param channel the channel to use, 0..nchannels-1
 * @return how many samples were unpacked
 */
size_t MarkIVUnpacker::extract_samples(char const* const src, Ipp32f* dst, const size_t count, const int channel) const
{
    const size_t payloadbytes = (ms->payloadoffset>0) ? (ms->framebytes - ms->payloadoffset) : ms->framebytes;

    if (channel >= ms->nchan) {
        cerr << "Source has only " << ms->nchan << " channels but settings request channel " << (channel+1) << endl;
    }

    /* Unpack and copy the desired channel */
    #if 1
    const size_t mark5access_max_size = payloadbytes*fanout; // note: ~2^18 is the maximum due to some mark5access bug/limitation
    size_t left = count;
    char const* mk5src = src;
    Ipp32f* mk5dst = dst;
    while (left > 0) {
        size_t to_unpack = std::min(mark5access_max_size, left);
        if (0 != (to_unpack % ms->samplegranularity)) {
            cerr << "to_unpack=" << to_unpack << " is not a multiple of sample granularity!" << endl;
        }
        // the follow line was commented out, GMC 04.02.2014
        size_t got = mark5_unpack(ms, (void*)mk5src, allchannels, to_unpack);
        /* Write dump from raw unpack result */
        #ifdef WRITE_DUMP
        // fout.write((char*)(allchannels[channel % ms->nchan]), std::streamsize(to_unpack * sizeof(Ipp32f)));
        #endif
        ippsCopy_32f(allchannels[channel % ms->nchan], mk5dst, to_unpack);
        mk5src += to_unpack;
        mk5dst += to_unpack;
        left -= to_unpack;
    }
    #else
    mark5_unpack(ms, (void *)src, allchannels, count);
    ippsCopy_32f(allchannels[channel % ms->nchan], dst, count);
    #endif

    /* Write dump before doing anything else */
    #ifdef WRITE_DUMP
    // fout.write((char*)dst, std::streamsize(count * sizeof(Ipp32f)));
    #endif

    /* Add randomness in place of the zeroes that mark5_unpack adds over "header" locations */
    //int headersamples = 160 * ms->nbit * ms->nchan * xxx->fanout / 8; // not correct, as 8ch 2bit 1:2 has 320, 4ch 2bit 1:4 has 640
    size_t headersamples = 0;
    size_t headeroffset = 0;
    while (dst[headeroffset]!=0) { headeroffset++; }
    while ((headersamples < count) && dst[headersamples+headeroffset] == 0) { headersamples++; }
    const size_t fanoutedframesamples = 20000 * (headersamples/160);
    // cerr << " headersamples = " << headersamples << ", payloadbytes=" << payloadbytes << ", headeroffset=" << headeroffset << ", fanoutedframesamples=" << fanoutedframesamples << endl;
    for (size_t n=headeroffset; n < count; n++) {
        if (((n-headeroffset) % fanoutedframesamples) < headersamples) {
            dst[n] = 3.3359f * float(2*(std::rand()%2) - 1);
        }
    }
    //cerr << "RAW input data:" << dst[320] << dst[260] << endl;

    /* Write dump after randomness */
    #ifdef WRITE_DUMP
    //fout.write((char*)dst, std::streamsize(count * sizeof(Ipp32f)));
    #endif

    return count;
}

bool MarkIVUnpacker::canHandleConfig(swspect_settings_t const* settings)
{
    return ((settings->bits_per_sample == 2 || settings->bits_per_sample == 1) && ((settings->source_channels % 2) == 0));
}

/**
 * Mark5B raw data to float unpacker. Mk5B mode uses the mark5access library
 * Initialize things required by the mark5access library.
 */
Mark5BUnpacker::Mark5BUnpacker(swspect_settings_t const* settings)
{
    /* Preps */
    cfg = settings;
    size_t count = cfg->fft_points;
	fanout = 1;

    ms = new_mark5_stream(
         new_mark5_stream_unpacker(/*noheaders*/1),
         new_mark5_format_generic_from_string(cfg->sourceformat_str.c_str())
    );
    if (!ms) {
        cerr << "Mark5access library did not like the format '" << cfg->sourceformat_str << "'" << endl;
        ms = NULL; //and then crash
    }

    /* mark5access limitation : all channels are unpacked, need lots of memory */
    allchannels = (Ipp32f**)malloc(ms->nchan * sizeof(float*));
    cerr << "Mark5BUnpacker allocating " << (ms->nchan*count*sizeof(float))/1024.0 << "kB of mark5access buffers." << endl;
    for (int ch=0; ch < ms->nchan; ch++) {
        allchannels[ch] =  (float *)memalign(128, count * sizeof(float));
        if (allchannels[ch] == NULL) {
            cerr << "Malloc of " <<  (count*sizeof(float)) << " bytes for temp channel " << ch
                 << " out of " << ms->nchan << " channels failed!" << endl;
         }
    }

    /* fill the output arrays with noise */
    for (int ch=0; ch < ms->nchan; ch++) {
        for (size_t n=0; n < count; n++) {
            allchannels[ch][n] = float(2*(std::rand()%2) - 1); // <= +-1.0f, TODO: use something better
        }
    }
}

/**
 * Mark5B raw data to float unpacker.
 * @param src     raw input data
 * @param dst     destination of unpacked floatingpoint data
 * @param count   how many samples to unpack
 * @param channel the channel to use, 0..nchannels-1
 * @return how many samples were unpacked
 */
size_t Mark5BUnpacker::extract_samples(char const* const src, Ipp32f* dst, const size_t count, const int channel) const
{
    const size_t payloadbytes = (ms->payloadoffset>0) ? (ms->framebytes - ms->payloadoffset) : ms->framebytes;

    if (channel >= ms->nchan) {
        cerr << "Source has only " << ms->nchan << " channels but settings request channel " << (channel+1) << endl;
    }

    /* Unpack and copy the desired channel */
    #if 0
    const size_t mark5access_max_size = payloadbytes*fanout; // note: ~2^18 is the maximum due to some mark5access bug/limitation
    size_t left = count;
    char const* mk5src = src;
    Ipp32f* mk5dst = dst;
    while (left > 0) {
        size_t to_unpack = std::min(mark5access_max_size, left);
        if (0 != (to_unpack % ms->samplegranularity)) {
            cerr << "to_unpack=" << to_unpack << " is not a multiple of sample granularity!" << endl;
        }
        //size_t got = mark5_unpack(ms, (void*)mk5src, allchannels, to_unpack);
        ippsCopy_32f(allchannels[channel % ms->nchan], mk5dst, to_unpack);
        mk5src += to_unpack;
        mk5dst += to_unpack;
        left -= to_unpack;
    }
    #else
    mark5_unpack(ms, (void *)src, allchannels, count);
    ippsCopy_32f(allchannels[channel % ms->nchan], dst, count);
    #endif

    /* Write dump before doing anything else */

    /* Add randomness in place of the zeroes that mark5_unpack adds over "header" locations */
    //int headersamples = 160 * ms->nbit * ms->nchan * xxx->fanout / 8; // not correct, as 8ch 2bit 1:2 has 320, 4ch 2bit 1:4 has 640
	size_t headeroffset = 16;
	for (size_t n=0; n < headeroffset; n++) {
		dst[n] = 3.3359f * float(2*(std::rand()%2) -1);
	}

    return count;
}

bool Mark5BUnpacker::canHandleConfig(swspect_settings_t const* settings)
{
    return ((settings->bits_per_sample == 2 || settings->bits_per_sample == 1) && ((settings->source_channels % 2) == 0));
}
