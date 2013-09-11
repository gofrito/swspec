#ifndef SETTINGS_H
#define SETTINGS_H
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

#include "DataSource.h"
#include "DataSink.h"
#include "Buffer.h"
#include "LogFile.h"

#include <vector>

#if defined(INTEL_IPP)
   #define PLATFORM_MAX_RAW_BUF_SIZE_MB     32
#elif defined(IBM_CELL)
   #define PLATFORM_MAX_RAW_BUF_SIZE_MB      8
#else
   #define PLATFORM_MAX_RAW_BUF_SIZE_MB     16
#endif

typedef float               swsfloat_t;
typedef unsigned long long  swsint64_t;

typedef struct _swscomplex_t {
   swsfloat_t re, im;
} swscomplex_t;

enum WindowFunctionType { None, Cosine, Cosine2, Hamming, Hann, Blackman };
enum OutputFormat { Ascii, Binary };
enum InputFormat  { Unknown=-1, RawSigned, RawUnsigned, Mark5B, iBOB, VDIF, VLBA, MKIV, Mk5B, Maxim };

class DataSink;
class DataSource;
class Buffer;
class LogFile;
class TeeStream;

// ------------------------------------------------------------------------
//   Settings Struct
// ------------------------------------------------------------------------

typedef struct swspect_settings_tt {

   // -- command line / INI : calculation parameters

   int num_cores;                // max number of CPU cores to use

   size_t fft_points;            // number of FFT/DFT points
   swsfloat_t fft_integ_seconds; // seconds of data integrated into a "dynamic spectrum"
   int fft_overlap_factor;       // add (fft_points/fft_overlap_factor) new samples to each next overlapped FFT
   WindowFunctionType wf_type;   // window function to be used for FFT/DFT

   int bits_per_sample;          // raw input data bits per sample (1,2,4,8,16,...)
   bool channelorder_increasing; // how the channels are ordered, channel#0 in MSB or channel#0 in LSB of first byte
   int seconds_to_skip;          // skip ahead in the input data by x seconds

   int source_channels;          // number of channels in the data source(s) (CH)
   int use_channel_file1;        // which one of the source channel(s) in input file 1 to use in calcs (1..CH)
   int use_channel_file2;        // which one of the source channel(s) in input file 2 to use in calcs (1..CH)

   swsfloat_t samplingfreq;      // sampling frequency in Hz (2 * BandwidthHz INI)
   swsfloat_t pcaloffsethz;      // offset in Hz of the phase calibration from the "n*1 MHz" comb spikes
   swsfloat_t pcalharmonicshz;   // typically 1 MHz i.e. the "n*1 MHz" comb spikes

   size_t max_rawbuf_size;       // the max size in Megabyte to use for each raw input source

   bool extract_PCal;            // true to extract the phase of the multitone phase-cal signal
   bool calc_Xpol;               // true to calculate cross-polarization
   bool costas_loop;             // true to calculate the Costas Loop for S/C signal with a carrier
   bool use_live_plot;           // plot the data in addition to writing to an output sink

   std::string basefilename1_pattern;  // base output file name with path and placeholders
   std::string basefilename2_pattern;

   // -- command line : file specifications

   std::vector<DataSource*> sources;   // all input data sources (can be either one or two)
   std::vector<DataSink*>   sinks;     // all output data sinks (can be one, two or with cross-pol three)
   std::vector<DataSink*>   pcalsinks; // all additional PCal signal output sinks

   int num_sources;
   int num_sinks;
   int num_xpols;

   InputFormat  sourceformat;
   std::string  sourceformat_str;
   OutputFormat sinkformat;

   // -- internal parameters

   Buffer**** rawbuffers;      // pointers to #cores of #sources double-buffered [2] buffers
   Buffer***  outbuffers;      // pointers to #cores of #sources of output buffers - channel N spectra

   Buffer***  outbuffersXpol;  // pointers to #cores of #crosssinks output buffers - channel pair {n,k} cross spectra

   Buffer***  outbuffersPCal;  // pointers to #cores of #sources output buffers - PCal detection results

   // -- "derived" parameters

   std::string basefilename1;  // placeholders filled, final string prepended to all output file names
                               // name derived from this (<basename>_swspec.bin, <basename>_runlog.txt, etc)
   std::string basefilename2;

   swsfloat_t dt;              // sample time resolution
   swsfloat_t df;              // FFT bin frequency resolution

   int averaged_ffts;            // how many non-overlapped FFTs have to be added for one spectrum
   int averaged_overlapped_ffts; // the overlapped FFTs every CPU core has to do for one full/partial spectrum

   int fft_overlap_points;       // number of samples in the fresh-data part in overlapped DFT/FFT
   int fft_ssb_points;           // single sideband points including Nyquist (fft_points/2 + 1)

   double rawbytes_per_channelsample; // input bytes consumed to get a single sample from a channel
   size_t raw_fullfft_bytes;          // raw bytes needed to get enough samples to do a full FFT
   size_t raw_overlap_bytes;          // raw bytes needed to shift in fresh samples for an overlapped FFT
   size_t fft_bytes;                  // real-valued double-sideband spectrum output bytes (sizeof(real)*fftpoints)
   size_t fft_bytes_ssb;              // real-valued single-sideband spectrum output bytes (sizeof(real)*(fftpoints/2+1))
   size_t fft_bytes_xpol;             // complex-valued single-sideband cross spectrum output bytes (2*fft_bytes_ssb)

   size_t rawbuf_size;                // how large chunks of raw data per each TaskCore to allocate
   int max_spectra_per_buffer;        // maximum number of spectra from one raw buffer processing pass
   int max_specffts_per_buffer;       // maximum number of non-overlapped FFTs out of #averaged_ffts that
                                      // can be done on one raw buffer (if several spectra fit, the value
                                      // is ==averaged_ffts)
   int max_buffers_per_spectrum;      // the other way round, how many raw bufs are needed at most for
                                      // one spectrum
   int core_overlapped_ffts;          // number of overlapped FFTs of one spectrum a core can do (=1..averaged_overlapped_ffts)
   int core_averaged_ffts;            // number of non-overlapped -"- ...

   int pcal_tonebins;                 // bin count for final pcal tones result, fs MHz / f_pcal 1MHz
   int pcal_rotatorlen;               // after how many bins the pcal signal repeats, given a pcal offset frequency Hz
   int pcal_pulses;                   // how many _tonebins pulses fit into _veclen
   int pcal_pulses_per_fft;           // how many _veclen vectors can be taken from input samples of one FFT
   size_t pcal_result_bytes;          // how many bytes needed for pcal_tonebins of complex numbers

   // -- text-based output files

   LogFile* logIO;
   LogFile* log;
   TeeStream* tlog;

} swspect_settings_t;

#endif // SETTINGS_H
