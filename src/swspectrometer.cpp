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

#include "swspectrometer.h"
#include "TaskCore.h"
#include "TaskDispatcher.h"
#include "Buffer.h"
#include "Settings.h"

#include "DataSource.h"
#include "DataSink.h"

#include "TeeSink.h"

#include "FileSource.h"
#include "FileSink.h"
#ifdef HAVE_PLPLOT
#include "PlplotSink.h"
#endif
#include "Helpers.h"

#include "IniParser.h"
#include <algorithm>

using std::endl;
using std::cerr;
using std::cout;

int main(int argc, char* argv[])
{
 
   /* Banner */
   cout << endl << VERSION_STR << endl;
   cout << "Build date " << (unsigned long)&__BUILD_DATE << " "
        << "number " << (unsigned long)&__BUILD_NUMBER << endl << endl;

   /* Check options */
   if (!(argc==3 || argc==4)) {
      cout << " Usage: swpsectrometer <inifile> <infile1> [<infile2>]" << endl
           << endl
           << "   inifile : file and path to an INI file with run settings" << endl
           << "   datafile1 : input data resource for channel 1 data" << endl
           << "   datafile2 : input data resource for channel 2 data (optional)" << endl << endl
           << "   If the INI file specifies that cross-polarization should be" << endl
           << "   calculated, please provide <infile2> as well." << endl
           << endl;
      return 0;
   }

   /* Set default options */
   swspect_settings_t sset;
   sset.num_cores           = 1;
   sset.max_rawbuf_size     = PLATFORM_MAX_RAW_BUF_SIZE_MB*1024*1024;
   sset.fft_points          = 320000;
   sset.fft_integ_seconds   = 20;
   sset.wf_type             = Cosine2;
   sset.fft_overlap_factor  = 2;       // 50% overlap
   sset.samplingfreq        = 16e6;    // 16 MHz
   sset.pcaloffsethz        = 10e3;    // at +10 kHz from n*1MHz, typical
   sset.pcalharmonicshz     = 1e6;     // 1 MHz
   sset.bits_per_sample     = 8;
   sset.channelorder_increasing = true;
   sset.source_channels     = 1;
   sset.use_channel_file1   = 1;
   sset.use_channel_file2   = 1;
   sset.seconds_to_skip     = 0;
   sset.extract_PCal        = false;
   sset.use_live_plot       = false;
   sset.calc_Xpol           = false;
   sset.costas_loop         = false;
   sset.sourceformat_str    = std::string("RawSigned");
   sset.sinkformat          = Binary;
   sset.basefilename1_pattern = std::string("ProjDate_StationID_Instrument_ScanNo_\%fftpoints\%_\%integrtime\%_\%channel\%");
   sset.basefilename2_pattern = std::string("");

   /* Open the INI file for parsing */
   std::string uri_inifile = std::string(argv[1]);
   IniParser iniParser(argv[1]);
   if(!iniParser.selectSection("Spectrometer")) {
      cerr << "Error: INI could not be read or it is missing the [Spectrometer] section" << endl;
      return -1;
   }

   /* Load individual keys */
   std::string keyval;
   iniParser.getKeyValue("NumCPUCores", sset.num_cores);
   if (iniParser.getKeyValue("MaxSourceBufferMB", sset.max_rawbuf_size)) {
      sset.max_rawbuf_size *= 1024*1024/2; // MByte, double-buffered
   }

   iniParser.getKeyValue("FFTpoints", sset.fft_points);
   iniParser.getKeyValue("FFTIntegrationTimeSec", sset.fft_integ_seconds);
   iniParser.getKeyValue("FFToverlapFactor", sset.fft_overlap_factor);
   if (iniParser.getKeyValue("WindowType", keyval)) {
      sset.wf_type = Helpers::parse_Windowing(keyval.c_str());
   }

   if (iniParser.getKeyValue("BandwidthHz", sset.samplingfreq)) {
      sset.samplingfreq *= 2.0; // fs=2*BW
   }
   iniParser.getKeyValue("PCalOffsetHz", sset.pcaloffsethz);

   iniParser.getKeyValue("SourceFormat", sset.sourceformat_str);
   iniParser.getKeyValue("SourceSkipSeconds", sset.seconds_to_skip);
   iniParser.getKeyValue("BitsPerSample", sset.bits_per_sample);
   iniParser.getKeyValue("ChannelOrderIncreasing", sset.channelorder_increasing);
   iniParser.getKeyValue("SourceChannels", sset.source_channels);

   iniParser.getKeyValue("UseFile1Channel", sset.use_channel_file1);
   iniParser.getKeyValue("UseFile2Channel", sset.use_channel_file2);

   iniParser.getKeyValue("ExtractPCal", sset.extract_PCal);
   iniParser.getKeyValue("PlotProgress", sset.use_live_plot);
   iniParser.getKeyValue("DoCrossPolarization", sset.calc_Xpol);
   iniParser.getKeyValue("DoCostasLoop",sset.costas_loop);

   iniParser.getKeyValue("SinkFormat", keyval);
   if (Helpers::cicompare(keyval, std::string("ASCII")) == Helpers::FullMatch) {
      sset.sinkformat = Ascii;
   }
   if (!iniParser.getKeyValue("BaseFilename1", sset.basefilename1_pattern)) {
      cerr << "Error: BaseFilename1 setting is missing from the INI file!" << endl;
      return -1;
   }
   if (!iniParser.getKeyValue("BaseFilename2", sset.basefilename2_pattern)) {
      cerr << "Warning: No BaseFilename2 specified in INI, will use 'BaseFilename1_file2'" << endl;
      sset.basefilename2_pattern = sset.basefilename1_pattern + std::string("_file2");
   }

   /* some extra checks */
   if ((sset.use_channel_file1 < 1) || (sset.use_channel_file1 > sset.source_channels)) {
      cerr << "Error: UseFile1Channel setting " << sset.use_channel_file1 << " is an invalid channel number" << endl;
      return -1;
   }
   if ((sset.use_channel_file2 < 1) || (sset.use_channel_file2 > sset.source_channels)) {
      cerr << "Error: UseFile2Channel setting " << sset.use_channel_file2 << " is an invalid channel number" << endl;
      return -1;
   }
   if (sset.calc_Xpol && (argc != 4)) {
      cerr << "Warning: only one of two input files provided, disabling cross-pol spectrum calculation." << endl;
      sset.calc_Xpol = false;
   }

   /* Correct some of the indexing */
   sset.use_channel_file1--; // 0..#NCH-1 vs 1..NCH
   sset.use_channel_file2--;
  
   /* Derive some parameters from the settings */
   sset.dt                   = 1.0 / sset.samplingfreq;
   sset.df                   = sset.samplingfreq / sset.fft_points;
   sset.averaged_ffts        = (sset.samplingfreq * sset.fft_integ_seconds) / sset.fft_points;
   sset.fft_overlap_points   = sset.fft_points / sset.fft_overlap_factor;
   sset.fft_ssb_points       = (sset.fft_points / 2) + 1;
   sset.rawbytes_per_channelsample = (sset.bits_per_sample * sset.source_channels) / 8.0;
   sset.raw_fullfft_bytes    = int(sset.fft_points * sset.rawbytes_per_channelsample);
   sset.raw_overlap_bytes    = int(sset.fft_overlap_points * sset.rawbytes_per_channelsample);
   sset.fft_bytes            = sset.fft_points * sizeof(swsfloat_t);
   sset.fft_bytes_ssb        = sset.fft_ssb_points * sizeof(swsfloat_t);
   sset.fft_bytes_xpol       = 2 * sset.fft_bytes_ssb; // complex data but single-sideband

   /* Derive some PCal extraction parameters */
   if (sset.extract_PCal) {
       sset.pcal_tonebins   = sset.samplingfreq / sset.pcalharmonicshz;
       sset.pcal_rotatorlen = sset.samplingfreq / Helpers::gcd(sset.samplingfreq, sset.pcaloffsethz);
       sset.pcal_pulses     = sset.pcal_rotatorlen / sset.pcal_tonebins;
       sset.pcal_result_bytes   = sset.pcal_tonebins * 2*sizeof(float);
       sset.pcal_pulses_per_fft = sset.fft_points / sset.pcal_rotatorlen;
       if ((sset.fft_points % sset.pcal_rotatorlen) != 0) {
           cerr << "Warning: disabling PCal extraction because " << sset.pcal_rotatorlen << "-length vectors did not fit evenly into " 
                << sset.fft_points << " DFT points." << endl;
           sset.extract_PCal = false;
       }
   } else {
       sset.pcal_tonebins   = 0;
       sset.pcal_rotatorlen = 0;
       sset.pcal_pulses     = 0;
       sset.pcal_result_bytes   = 0;
       sset.pcal_pulses_per_fft = 0;
   }

   /* Normalize the maximum buf size by #cores */
   sset.max_rawbuf_size = sset.max_rawbuf_size / sset.num_cores;

   /* Determine raw buf size: even split accross CPU cores, size-limited by MaxSourceBufferMB */
   size_t tent_rawbuf_size = sset.raw_fullfft_bytes * sset.averaged_ffts;
   if (tent_rawbuf_size > sset.max_rawbuf_size) {
      // spectrum input too big -- start with just 1 FFT and many buffers, then keep
      // doubling FFTs and reducing buffers until MaxSourceBufferMB is best filled
      // (this could also be computed with log2()'s instead of the loop)
      sset.max_spectra_per_buffer   = 0;
      sset.max_specffts_per_buffer  = 1;
      sset.max_buffers_per_spectrum = sset.averaged_ffts;
      tent_rawbuf_size              = sset.raw_fullfft_bytes;
      while ((tent_rawbuf_size <= sset.max_rawbuf_size) && ((sset.max_buffers_per_spectrum % 2) == 0)) {
          tent_rawbuf_size *= 2;
          sset.max_buffers_per_spectrum /= 2;
          sset.max_specffts_per_buffer *= 2;
      }
   } else {
      // one full spectrum input fits -- now try to fit even more spectra
      sset.max_spectra_per_buffer   = 1;
      sset.max_specffts_per_buffer  = sset.averaged_ffts;
      sset.max_buffers_per_spectrum = 0;
      while ((2*tent_rawbuf_size) <= sset.max_rawbuf_size) {
          tent_rawbuf_size *= 2;
          sset.max_spectra_per_buffer *= 2;
          // sset.max_specffts_per_buffer: used for single-spectrum scaling, don't double it
      }
   }
   sset.rawbuf_size = tent_rawbuf_size;

   /* Derive some per-core parameters from the settings and the memory allocation */
   sset.core_averaged_ffts   = sset.max_specffts_per_buffer;
   sset.core_overlapped_ffts = sset.fft_overlap_factor*sset.max_specffts_per_buffer - (sset.fft_overlap_factor - 1);

   /* Prepare log files */
   sset.basefilename1 = cfg_to_filename(sset.basefilename1_pattern, sset, 1);
   sset.basefilename2 = cfg_to_filename(sset.basefilename1_pattern, sset, 2);
   sset.logIO = new LogFile(sset.basefilename1 + std::string("_starttiming.txt")); 
   //TODO: sset.logIO2 = new LogFile(sset.basefilename2 + std::string("_starttiming.txt")); 
   sset.log = new LogFile(sset.basefilename1 + std::string("_runlog.txt"));
   sset.tlog = new TeeStream(std::cerr, sset.log->stream());
   std::ostream* out = sset.tlog;

   /* Get the file or resource names */
   std::string uri_input1(argv[2]);
   std::string uri_input2("");
   if (argc == 4) {
      uri_input2 = std::string(argv[3]);
   }

   /* Prepare output file names derived from the INI */
   std::string uri_output1(sset.basefilename1 + "_swspec.bin");
   std::string uri_output2(sset.basefilename2 + "_swspec.bin");
   std::string uri_pcal1(sset.basefilename1 + "_pcal.bin");
   std::string uri_pcal2(sset.basefilename2 + "_pcal.bin");
   std::string uri_outputX(sset.basefilename1 + "_xpol_swspec.bin");

   /* Display config */
   *out << "Config file  : " << uri_inifile << endl;
   *out << "Input file 1 : " << uri_input1 << endl;
   *out << "Input file 2 : " << uri_input2 << endl;
   *out << "Core setup   : " << sset.num_cores << " parallel processing thread(s)" << endl;
   *out << "File format  : " << sset.bits_per_sample << " bits/sample, "
                             << sset.source_channels << " channels, selected "
                             << "channel " << (sset.use_channel_file1+1) << " of input 1, "
                             << "channel " << (sset.use_channel_file2+1) << " of input 2" << endl;
   *out << "Analog info  : " << 0.5*sset.samplingfreq/1e3 << " kHz bandwidth, "
                             << sset.samplingfreq/1e3 << " kHz sampling rate" << ", dt=" << sset.dt << endl;
   *out << "DFT info     : " << sset.fft_points << " points, "
                             << sset.df << " Hz resolution, "
                             << sset.averaged_ffts << "-fold averaging "
                             << "(" << (sset.averaged_ffts * sset.fft_points) / sset.samplingfreq << "s), "
                             << 100.0*(1.0 - 1.0/sset.fft_overlap_factor) << "% overlap" << endl;
   *out << "PCal extract : ";
   if (sset.extract_PCal) { 
       *out << "on, " << sset.pcaloffsethz << " Hz offset, "
            << sset.pcal_tonebins << " bins, period " 
            << sset.pcal_rotatorlen << ", " << sset.pcal_pulses_per_fft << " pulses per DFT" << endl;  
   } else { 
       *out << "off"<< endl;  
   }
   *out << "Cross-pol    : ";
   if (sset.calc_Xpol) { *out<<"on"<<endl; } else { *out<<"off"<<endl; }
   if (sset.costas_loop) { *out<<"Spacecraft signal without carrier, using Costas loop"<<endl;} else {}
   *out << "Raw buffers  : " << (sset.rawbuf_size/1024.0) << " kB per source" << endl;
   if (sset.max_buffers_per_spectrum > 0) {
       *out << "Buffer use   : 1 averaged spectrum consumes "
            << sset.max_buffers_per_spectrum << " core runs" << endl;
   }
   if (sset.max_spectra_per_buffer > 0) {
       *out << "Buffer use   : 1 core run gives "
            << sset.max_spectra_per_buffer << " averaged spectra" << endl;
   }

   /* Open input 1 data, output 1 spectrum and output 1 pcal */
   if (!addOpenSource(uri_input1, sset.sources, sset)) { 
       *out << "Error: could not addOpenSource() " << uri_input1 << endl;
       return -1; 
   }
   if (sset.use_live_plot) {
      if (!addOpenPlotSink(uri_output1, sset.sinks, sset, "File 1 Power Spectrum")) { 
          *out << "Error: could not addOpenPlotSink() " << uri_output1 << endl;
          return -1; 
      }
   } else {
      if (!addOpenSink(uri_output1, sset.sinks, sset)) { 
          *out << "Error: could not addOpenSink() " << uri_output1 << endl;
          return -1; 
      }
   }
   if (sset.extract_PCal) {
      if (!addOpenSink(uri_pcal1, sset.pcalsinks, sset)) { 
          *out << "Error: could not addOpenSink() " << uri_pcal1 << endl;
          return -1; 
      }
   }

   /* Open input 2 data, output 2 spectrum and output 2 pcal */
   if (argc == 4) {
      if (!addOpenSource(uri_input2, sset.sources, sset)) { 
           *out << "Error: could not addOpenSource() " << uri_input2 << endl;
           return -1; 
      }
      if (sset.use_live_plot) {
         if (!addOpenPlotSink(uri_output2, sset.sinks, sset, "File 2 Power Spectrum")) { 
             *out << "Error: could not addOpenPlotSink() " << uri_output2 << endl;
             return -1; 
         }
      } else {
         if (!addOpenSink(uri_output2, sset.sinks, sset)) { 
             *out << "Error: could not addOpenSink() " << uri_output2 << endl;
             return -1; 
         }
      }
      if (sset.extract_PCal) {
         if (!addOpenSink(uri_pcal2, sset.pcalsinks, sset)) {
             *out << "Error: could not addOpenSink() " << uri_pcal2 << endl;
             return -1; 
         }
      }
   }

   /* Open output xpol */
   if (sset.calc_Xpol) {
      if (sset.use_live_plot) {
         if (!addOpenPlotSink(uri_outputX, sset.sinks, sset, "Cross-polarization Spectrum")) { 
             *out << "Error: could not addOpenPlotSink() " << uri_outputX << endl;
             return -1; 
         }
      } else {
         if (!addOpenSink(uri_outputX, sset.sinks, sset)) { 
             *out << "Error: could not addOpenSink() " << uri_outputX << endl;
             return -1; 
         }
      }
      sset.num_xpols = 1;
   } else {
      sset.num_xpols = 0;
   }

   /* Copy the final counts */
   sset.num_sources = sset.sources.size();
   sset.num_sinks   = sset.sinks.size();

   /*
    * Create the double-buffered raw input bufs
    * To make cross-pol spectra each core needs data from all source files.
    * We use a layout of Buffer*[cores][doublebuffers][sources], where
    * every core gets a double-buffered set, and each set has data from all
    * sources.
    */
   double ramMB_1source = 2 * sset.num_cores * sset.rawbuf_size/(1024.0*1024.0);
   double ramMB_total   = ramMB_1source * sset.sources.size();
   *out << "Raw buffers  : " << ramMB_total << " MByte in total with double-buffering" << endl;
   sset.rawbuffers = new Buffer***[sset.num_cores];
   for (int c=0; c<sset.num_cores; c++) {
      sset.rawbuffers[c] = new Buffer**[2];
      for (int b=0; b<2; b++) {
         sset.rawbuffers[c][b] = new Buffer*[sset.num_sources];
         for (int s=0; s<sset.num_sources; s++) {
            sset.rawbuffers[c][b][s] = new Buffer(sset.rawbuf_size);
         }
      }
   }

   /*
    * Create complex data output buffers.
    * Every core places its integrated spectra or (sub)spectrum into its output
    * buffer. The TaskDispatcher collects and combines them and writes to the sink..
    * No double-buffering is needed, the spectrum integration means heavy data reduction.
    * So we will use a layout of Buffer*[cores][sources].
    * The cross-pol needs an own additional Buffer*[cores].
    */
   size_t outbuf_size_auto = std::max(sset.max_spectra_per_buffer, 1) * sset.fft_bytes_ssb;
   size_t outbuf_size_xpol = std::max(sset.max_spectra_per_buffer, 1) * sset.fft_bytes_xpol /* *sset.num_xpols */;
   size_t outbuf_size_pcal = std::max(sset.max_spectra_per_buffer, 1) * sset.pcal_result_bytes;
   double ramMB_outtotal = sset.num_cores * (outbuf_size_auto*sset.num_sources + outbuf_size_xpol*sset.num_xpols) / (1024.0*1024.0);
   *out << "Out buffers  : " << ramMB_outtotal << " MByte in total" << endl;
   sset.outbuffers     = new Buffer**[sset.num_cores];
   sset.outbuffersXpol = new Buffer**[sset.num_cores];
   sset.outbuffersPCal = new Buffer**[sset.num_cores];
   for (int c=0; c<sset.num_cores; c++) {
      sset.outbuffers[c] = new Buffer*[sset.num_sources];
      for (int s=0; s<sset.num_sources; s++) {
         sset.outbuffers[c][s] = new Buffer(outbuf_size_auto);
      }
      sset.outbuffersXpol[c] = new Buffer*[sset.num_xpols];
      for (int x=0; x<sset.num_xpols; x++) {
         sset.outbuffersXpol[c][x] = new Buffer(outbuf_size_xpol);
      }
      sset.outbuffersPCal[c] = new Buffer*[sset.num_sources];
      for (int s=0; s<sset.num_sources; s++) {
         sset.outbuffersPCal[c][s] = new Buffer(outbuf_size_pcal);
      }
   }
   *out << endl;

   /* Process the data */
   TaskDispatcher* td = new TaskDispatcher(&sset);
   td->run();

   /* Clean-up */
   if (sset.use_live_plot) {
      *out << "Processing complete. Press 'return' key to close the plot." << endl;
      getchar();
   }

   return 0;
}


/**
 * Helper to get a new data source, open it and add to the sources vector.
 */
bool addOpenSource(std::string const& uri, std::vector<DataSource*>& sources, swspect_settings_t& set)
{
   DataSource* dsrc = DataSource::getDataSource(uri, &set);
   if ((dsrc == NULL) || (dsrc->open(uri))) {
       cerr << "Error: problem with input data URI '" << uri << "' or format error while opening it." << endl;
       return false;
   }
   sources.push_back(dsrc);
   return true;
}


/**
 * Helper to get a new data sink, open it and add to the sinks vector.
 */
bool addOpenSink(std::string const& uri, std::vector<DataSink*>& sinks, swspect_settings_t& set)
{
   DataSink* dsink = DataSink::getDataSink(uri, &set);
   if ((dsink == NULL) || (dsink->open(uri))) {
       cerr << "Error: Invalid output data URI '" << uri << "' or could not open it " << endl;
       return false;
   }
   sinks.push_back(dsink);
   return true;
}

/**
 * Helper to get a new data sink tee'ed with a plot sink, open them and add to the sinks vector.
 */
bool addOpenPlotSink(std::string const& uri, std::vector<DataSink*>& sinks, swspect_settings_t& set, std::string const& plottitle)
{
   DataSink* datasink = DataSink::getDataSink(uri, &set);
   if ((datasink == NULL) || (datasink->open(uri))) {
       cerr << "Error: Invalid output data URI '" << uri << "' or could not open it " << endl;
       return false;
   }
#ifdef HAVE_PLPLOT
   DataSink* plotsink = new PlpotSink(&set, plottitle);

   TeeSink *combined = new TeeSink(&set);
   combined->addSink(datasink);
   combined->addSink(plotsink);
   combined->open(uri);
   sinks.push_back(combined);
   return true;
#else
   // continue as if we added plot sink
   return true;
#endif
}

/**
 * Return a file name created from 'pattern' filled out with SWspectrometer settings.
 */
std::string cfg_to_filename(std::string pattern, swspect_settings_t const& set, int filenr)
{
    typedef struct keypair_tt {
        std::string match;
        std::string replacement;
    } keypair_t;

    std::string::size_type idx;
    int ch = (filenr==1) ? set.use_channel_file1 : set.use_channel_file2;
    keypair_t keys[3] = { { std::string("\%fftpoints\%"),   Helpers::itoa((int)set.fft_points) },
                          { std::string("\%integrtime\%"),  Helpers::itoa((int)set.fft_integ_seconds) },
                          { std::string("\%channel\%"),     Helpers::itoa(ch + 1) },
                        };

    // remove spaces in the filename
    std::replace(pattern.begin(), pattern.end(), ' ', '_');
    std::string filename = pattern;

    // replace found keys with actual strings
    for (size_t i=0; i<sizeof(keys)/sizeof(keypair_t); i++) {
        while (1) {
            idx = filename.find(keys[i].match, 0);
            if (idx == std::string::npos) break;
            filename = filename.replace(idx, keys[i].match.length(), keys[i].replacement);
        }
    }
    return filename;
}

