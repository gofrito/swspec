/********************************************************************************************************
 * @file PCal.cpp
 * Multi-tone Phase Cal Extraction
 *
 * @brief Extracts and integrates multi-tone phase calibration signal information from an input signal.
 *
 * The principle relies on the fact that with a comb spacing of say 1 MHz and a sampling rate of say
 * 32 MHz the single 1 MHz and also all of its multiples (1, 2, .. 16 MHz in the band) have at least
 * one full sine period in 32MHz/1MHz = 32 samples. For extraction and time integration, we simply
 * have to segment the input signal into 32-sample pieces (in the above example) and integrate these
 * pieces.
 *
 * A tiny FFT performed on the integrated 32-bin result gives you the amplitude and phase
 * of every tone. As the PCal amplitude is in practice constant over a short frequency band,
 * the amplitude and phase info after the FFT directly gives you the equipment filter response.
 *
 * The extracted PCal can also be analyzed in the time domain (no FFT). The relative, average instrumental
 * delay time can be found directly by estimating the position of the peak in the time-domain data.
 * 
 * @author   Jan Wagner
 * @author   Sergei Pogrebenko
 * @author   Walter Brisken
 * @version  1.1/2009
 * @license  GNU GPL v3
 *
 * Changelog:
 *   05Oct2009 - added support for arbitrary input segment lengths
 *   08Oct2009 - added Briskens rotationless method
 *   02Nov2009 - added sub-subintegration sample offset, DFT for f-d results, tone bin coping to user buf
 *   03Nov2009 - added unit test, included DFT in extractAndIntegrate_reference(), fix rotation direction
 *
 ********************************************************************************************************/

#include "PCal.h"
#include "PCal_impl.h"
#include <ippcore.h>
#include <ipps.h>
#include <iostream>
#include <cmath>
using std::cerr;
using std::endl;

using namespace std;

#include <malloc.h>    // memalign
#define UNROLL_BY_4(x) { x }{ x }{ x }{ x }
#define VALIGN __attribute__((aligned(16)))

void print_f32(const Ipp32f* v, const int len)
{
   for (int i=0; i<len; i++) cerr << v[i] << " ";
}

void print_fc32(const Ipp32fc* v, const int len)
{
   for (int i=0;i<len; i++) cerr << v[i].re << "+i" << v[i].im << " ";
}

class pcal_config_pimpl {
  public:
    pcal_config_pimpl()  {};
    ~pcal_config_pimpl() {};
  public:
    double   dphi;
    Ipp32fc* rotator;        // pre-cooked oscillator values
    Ipp32fc* rotated;        // temporary
    Ipp32fc* pcal_complex;   // temporary unassembled output, later final output
    Ipp32f*  pcal_real;      // temporary unassembled output for the pcaloffsethz==0.0f case
    size_t   rotatorlen;
    size_t   pcal_index;     // zero, changes when extract() is called at least once with "leftover" samples
    size_t   rotator_index;  // zero, changes when extract() is called at least once with "leftover" samples
  public:
    IppsDFTSpec_C_32fc* dftspec;
    Ipp8u* dftworkbuf;
    Ipp32fc*  dft_out; // unnecessary onces Intel implements inplace DFTFwd_CtoC_32fc_I (counterpart of inplace DFTFwd_RtoC)
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// BASE CLASS: factory and helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Factory that returns a new PCal extractor object. The optimal implementation
 * is selected based on the input parameters.
 * @param bandwidth_hz     Bandwidth of the input signal in Hertz
 * @param pcal_spacing_hz  Spacing of the PCal signal, comb spacing, typically 1e6 Hertz
 * @param pcal_offset_hz   Offset of the first PCal signal from 0Hz/DC, typically 10e3 Hertz
 * @param sampleoffset     Offset of the first sample as referenced to start of subintegration interval
 * @return new PCal extractor class instance
 */
PCal* PCal::getNew(double bandwidth_hz, double pcal_spacing_hz, int pcal_offset_hz, const size_t sampleoffset) 
{
    //NOTE Added for testing
    //return new PCalExtractorDummy(bandwidth_hz, pcal_spacing_hz, sampleoffset);

    //cout << "bandwidth_hz = " << bandwidth_hz << ", pcal_spacing_hz = " << pcal_spacing_hz << ", pcal_offset_hz = " << pcal_offset_hz << ", sampleoffset = " << sampleoffset << endl;

    if (pcal_offset_hz == 0.0f) {
        return new PCalExtractorTrivial(bandwidth_hz, pcal_spacing_hz, sampleoffset);
    }
    // if ( __unlikely ((2*bandwidth_hz / gcd(2*bandwidth_hz,pcal_spacing_hz)) > someLengthLimit) ) {
    //    use oscillator implementation instead
    // } else {
    int No, Np;
    No = 2*bandwidth_hz / gcd(pcal_offset_hz, 2*bandwidth_hz);
    Np = 2*bandwidth_hz / gcd(pcal_spacing_hz, 2*bandwidth_hz);
    if ((No % Np) == 0 /* && (!want_timedomain_delay) */) {
        return new PCalExtractorImplicitShift(bandwidth_hz, pcal_spacing_hz, pcal_offset_hz, sampleoffset);
    }
    return new PCalExtractorShifting(bandwidth_hz, pcal_spacing_hz, pcal_offset_hz, sampleoffset);
}

/**
 * Greatest common divisor.
 */
long long PCal::gcd(long a, long b)
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

/**
 * Processes samples and accumulates the detected phase calibration tone vector.
 * Computation uses the slowest thinkable direct method. This function is 
 * intended for testing and comparison only!
 * @param  data          pointer to input sample vector
 * @param  len           length of input vector
 * @param  pcalout       output array of sufficient size to store extracted PCal
 */
bool PCal::extractAndIntegrate_reference(Ipp32f const* data, const size_t len, Ipp32fc* out, const uint64_t sampleoffset)
{
    size_t Nbins = 2*_N_tones;
    size_t maxtoneperiod = _fs_hz/gcd(_pcaloffset_hz,_fs_hz);
    double dphi = 2*M_PI * (-_pcaloffset_hz/_fs_hz);

    Ipp32fc pcalout[Nbins];
    ippsZero_32fc(pcalout, Nbins);

    for (size_t n=0; n<len; n++) {
        size_t no = n + sampleoffset;
        int bin = (no % Nbins);
        double phi = dphi * ((_samplecount + no) % maxtoneperiod);
        pcalout[bin].re += cos(phi) * data[n];
        pcalout[bin].im += sin(phi) * data[n];
    }
    _samplecount += len;
    cerr << "PCal::extractAndIntegrate_reference Ntones=" << _N_tones << " Nbins=" << Nbins 
         << " toneperiod=" << maxtoneperiod << endl;

    int wbufsize = 0;
    IppsDFTSpec_C_32fc* dftspec;
    IppStatus s = ippsDFTInitAlloc_C_32fc(&dftspec, Nbins, IPP_FFT_DIV_FWD_BY_N, ippAlgHintAccurate);
    if (s != ippStsNoErr) { cerr << "DFTInitAlloc err " << ippGetStatusString(s); } 
    s = ippsDFTGetBufSize_C_32fc(dftspec, &wbufsize);
    if (s != ippStsNoErr) { cerr << "DFTGetBufSize err " << ippGetStatusString(s); }
    Ipp8u* dftworkbuf = (Ipp8u*)memalign(128, wbufsize);

    Ipp32fc dftout[Nbins];
    s = ippsDFTFwd_CToC_32fc(pcalout, dftout, dftspec, dftworkbuf);
    if (s != ippStsNoErr) cerr << "DFTFwd err " << ippGetStatusString(s); 
    ippsCopy_32fc(dftout, out, _N_tones);

    ippsDFTFree_C_32fc(dftspec);
    free(dftworkbuf);
    return true;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
// DERIVED CLASS: extraction of zero-offset PCal signals
/////////////////////////////////////////////////////////////////////////////////////////////////////////

PCalExtractorTrivial::PCalExtractorTrivial(double bandwidth_hz, int pcal_spacing_hz, const size_t sampleoffset)
{
    /* Derive config */
    _cfg = new pcal_config_pimpl();
    _fs_hz   = 2*bandwidth_hz;
    _pcaloffset_hz  = 0;
    _pcalspacing_hz = pcal_spacing_hz;
    _N_bins  = _fs_hz / gcd(std::abs((double)pcal_spacing_hz), _fs_hz);
    _N_tones = std::floor(bandwidth_hz / pcal_spacing_hz);

    /* Prep for FFT/DFT */
    // TODO: is IPP_FFT_DIV_FWD_BY_N or is IPP_FFT_DIV_INV_BY_N expected by AIPS&co?
    int wbufsize = 0;
    ippsDFTInitAlloc_C_32fc(&(_cfg->dftspec), _N_bins, IPP_FFT_DIV_FWD_BY_N, ippAlgHintAccurate);
    ippsDFTGetBufSize_C_32fc(_cfg->dftspec, &wbufsize);
    _cfg->dftworkbuf = (Ipp8u*)memalign(128, wbufsize);

    /* Allocate */
    _cfg->pcal_complex = (Ipp32fc*)memalign(128, sizeof(Ipp32fc) * _N_bins * 2);
    _cfg->pcal_real    = (Ipp32f*)memalign(128, sizeof(Ipp32f) * _N_bins * 2);
    _cfg->dft_out      = (Ipp32fc*)memalign(128, sizeof(Ipp32fc) * _N_bins * 1);
    this->clear();
    cerr << "PCalExtractorTrivial: _Ntones=" << _N_tones << ", _N_bins=" << _N_bins << ", wbufsize=" << wbufsize << endl;
}

PCalExtractorTrivial::~PCalExtractorTrivial()
{
    free(_cfg->pcal_complex);
    free(_cfg->pcal_real);
    ippsDFTFree_C_32fc(_cfg->dftspec);
    free(_cfg->dftworkbuf);
    free(_cfg->dft_out);
    delete _cfg;
}

/**
 * Set the extracted and accumulated PCal data back to zero.
 * When several PCal are run in parallel in segments of a 
 * time slice of data, and the PCal results from each segment
 * should be combined later, care must be taken to tell the PCal
 * extraction the number/offset of the first sample in the segment
 * Typically 0 for the first segment, len(segment) for the second
 * segment, and so on, until offset of the last segment
 * which is len(subintegration_subslice)-len(segment).
 * @param sampleoffset referenced back to start of subintegration interval
 */
void PCalExtractorTrivial::clear()
{
    _samplecount = 0;
    _finalized   = false;
    ippsZero_32fc(_cfg->pcal_complex, _N_bins * 2);
    ippsZero_32f (_cfg->pcal_real,    _N_bins * 2);
}

/**
 * Adjust the sample offset. Should be called before extractAndIntegrate()
 * every time there is a gap or backwards shift in the otherwise contiguous
 * sample stream.
 * @param sampleoffset referenced back to start of subintegration interval
 */
void PCalExtractorTrivial::adjustSampleOffset(const size_t sampleoffset)
{
    _cfg->rotator_index = 0; // unused
    _cfg->pcal_index = (sampleoffset) % _N_bins;
}

/**
 * Extracts multi-tone PCal information from a single-channel signal segment
 * and integrates it to the class-internal PCal extraction result buffer.
 * There are no restrictions to the segment length.
 *
 * If you integrate over a longer time and several segments, i.e. perform
 * multiple calls to this function, take care to keep the input
 * continuous (i.e. don't leave out samples).
 *
 * If extraction has been finalized by calling getFinalPCal() this function
 * returns False. You need to call clear() to reset.
 *
 * @paran samples Chunk of the input signal consisting of 'float' samples
 * @param len     Length of the input signal chunk
 * @return true on success
 */
bool PCalExtractorTrivial::extractAndIntegrate(Ipp32f const* samples, const size_t len)
{
    if (_finalized) { return false; }

    Ipp32f const* src = samples;
    Ipp32f* dst = &(_cfg->pcal_real[_cfg->pcal_index]);
    size_t tail = (len % _N_bins);
    size_t end  = len - tail;

    /* Process the first part that fits perfectly */
    for (size_t n = 0; n < end; n+=_N_bins, src+=_N_bins) {
        ippsAdd_32f_I(src, dst, _N_bins);
    }

    /* Handle any samples that didn't fit */
    if (tail != 0) {
        ippsAdd_32f_I(src, dst, tail);
        _cfg->pcal_index = (_cfg->pcal_index + tail) % _N_bins;
    }

    /* Done! */
    _samplecount += len;
    return true;
}

/**
 * Performs finalization steps on the internal PCal results if necessary
 * and then copies these PCal results into the specified output array.
 * Data in the output array is overwritten with PCal results.
 *
 * @param pointer to user PCal array with getLength() values
 * @return number of samples that were integrated for the result
 */
uint64_t PCalExtractorTrivial::getFinalPCal(Ipp32fc* out)
{
    if (!_finalized) {
        _finalized = true;
        ippsAdd_32f_I(/*src*/&(_cfg->pcal_real[_N_bins]), /*srcdst*/&(_cfg->pcal_real[0]), _N_bins);
        ippsRealToCplx_32f(/*srcRe*/_cfg->pcal_real, /*srcIm*/NULL, _cfg->pcal_complex, _N_bins);
        IppStatus r = ippsDFTFwd_CToC_32fc(/*src*/_cfg->pcal_complex, _cfg->dft_out, _cfg->dftspec, _cfg->dftworkbuf);
        if (r != ippStsNoErr) {
            cerr << "ippsDFTFwd error " << ippGetStatusString(r);
        }
    }

    // Copy only the tone bins: in PCalExtractorTrivial case
    // this should be all bins... _N_tones==_N_bins/2
    ippsCopy_32fc(_cfg->dft_out, (Ipp32fc*)out, _N_tones);
    return _samplecount;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// DERIVED CLASS: extraction of PCal signals with non-zero offset
/////////////////////////////////////////////////////////////////////////////////////////////////////////

PCalExtractorShifting::PCalExtractorShifting(double bandwidth_hz, double pcal_spacing_hz, int pcal_offset_hz, const size_t sampleoffset)
{
    /* Derive config */
    _fs_hz          = 2 * bandwidth_hz;
    _pcaloffset_hz  = pcal_offset_hz;
    _pcalspacing_hz = pcal_spacing_hz;
    _N_bins         = _fs_hz / gcd(std::abs((double)pcal_spacing_hz), _fs_hz);
    _N_tones        = std::floor((bandwidth_hz - pcal_offset_hz) / pcal_spacing_hz) + 1;
    _cfg = new pcal_config_pimpl();
    _cfg->rotatorlen = _fs_hz / gcd(std::abs((double)_pcaloffset_hz), _fs_hz);

    /* Prep for FFT/DFT */
    // TODO: is IPP_FFT_DIV_FWD_BY_N or is IPP_FFT_DIV_INV_BY_N expected by AIPS&co?
    int wbufsize = 0;
    IppStatus r;
    r = ippsDFTInitAlloc_C_32fc(&(_cfg->dftspec), _N_bins, IPP_FFT_DIV_FWD_BY_N, ippAlgHintAccurate);
    if (r != ippStsNoErr) {
        cerr << "ippsDFTInitAlloc _N_bins=" << _N_bins << " error " << ippGetStatusString(r);
    }
    r = ippsDFTGetBufSize_C_32fc(_cfg->dftspec, &wbufsize);
    if (r != ippStsNoErr) {
        cerr << "ippsDFTGetBufSize error " << ippGetStatusString(r);
    }
    _cfg->dftworkbuf = (Ipp8u*)memalign(128, wbufsize);

    /* Allocate */
    _cfg->pcal_complex = (Ipp32fc*)memalign(128, sizeof(Ipp32fc) * _N_bins * 2);
    _cfg->pcal_real    = (Ipp32f*)memalign(128, sizeof(Ipp32f) * _N_bins * 2);
    _cfg->rotator = (Ipp32fc*)memalign(128, sizeof(Ipp32fc) * _cfg->rotatorlen * 2);
    _cfg->rotated = (Ipp32fc*)memalign(128, sizeof(Ipp32fc) * _cfg->rotatorlen * 2);
    _cfg->dft_out = (Ipp32fc*)memalign(128, sizeof(Ipp32fc) * _N_bins * 1);
    this->clear();

    /* Prepare frequency shifter/mixer lookup */
    _cfg->dphi = 2*M_PI * (-_pcaloffset_hz/_fs_hz);
    for (size_t n = 0; n < (2 * _cfg->rotatorlen); n++) {
        double arg = _cfg->dphi * double(n);
        _cfg->rotator[n].re = Ipp32f(cos(arg));
        _cfg->rotator[n].im = Ipp32f(sin(arg));
    }
    cerr << "PcalExtractorShifting: _Ntones=" << _N_tones << ", _N_bins=" << _N_bins << ", wbufsize=" << wbufsize << endl;
}

PCalExtractorShifting::~PCalExtractorShifting()
{
    free(_cfg->pcal_complex);
    free(_cfg->pcal_real);
    free(_cfg->rotator);
    free(_cfg->rotated);
    ippsDFTFree_C_32fc(_cfg->dftspec);
    free(_cfg->dftworkbuf);
    free(_cfg->dft_out);
    delete _cfg;
}

/**
 * Set the extracted and accumulated PCal data back to zero.
 * When several PCal are run in parallel in segments of a 
 * time slice of data, and the PCal results from each segment
 * should be combined later, care must be taken to tell the PCal
 * extraction the number/offset of the first sample in the segment
 * Typically 0 for the first segment, len(segment) for the second
 * segment, and so on, until offset of the last segment
 * which is len(subintegration_subslice)-len(segment).
 * @param sampleoffset referenced back to start of subintegration interval
 */
void PCalExtractorShifting::clear()
{
    _samplecount = 0;
    _finalized   = false;
    ippsZero_32fc(_cfg->pcal_complex, _N_bins * 2);
    ippsZero_32f (_cfg->pcal_real,    _N_bins * 2);
    ippsZero_32fc(_cfg->rotated,      _cfg->rotatorlen * 2);
}

/**
 * Adjust the sample offset. Should be called before extractAndIntegrate()
 * every time there is a gap or backwards shift in the otherwise contiguous
 * sample stream.
 * @param sampleoffset referenced back to start of subintegration interval
 */
void PCalExtractorShifting::adjustSampleOffset(const size_t sampleoffset)
{
    _cfg->rotator_index = (sampleoffset)% _cfg->rotatorlen;
    _cfg->pcal_index    = (sampleoffset)% _N_bins;
}

/**
 * Extracts multi-tone PCal information from a single-channel signal segment
 * and integrates it to the class-internal PCal extraction result buffer.
 * There are no restrictions to the segment length.
 *
 * If you integrate over a longer time and several segments, i.e. perform
 * multiple calls to this function, take care to keep the input
 * continuous (i.e. don't leave out samples).
 *
 * If extraction has been finalized by calling getFinalPCal() this function
 * returns False. You need to call clear() to reset.
 *
 * @paran samples Chunk of the input signal consisting of 'float' samples
 * @param len     Length of the input signal chunk
 * @return true on success
 */
bool PCalExtractorShifting::extractAndIntegrate(Ipp32f const* samples, const size_t len)
{
    if (_finalized) { return false; }

    Ipp32f const* src = samples;
    Ipp32fc* dst = &(_cfg->pcal_complex[_cfg->pcal_index]);
    size_t tail  = (len % _cfg->rotatorlen);
    size_t end   = len - tail;

    /* This method is only marginally different from the PCalExtractorTrivial method.
     * Because now our multi-tone PCal signal tones do not reside at integer MHz frequencies,
     * or rather, not at integer multiples of the tone spacing of the comb, the first PCal
     * tone is found at some offset '_pcaloffset_hz' away from 0Hz/DC. 
     * So we just use a complex oscillator to shift the input signal back into place.
     * The complex oscillator has a period of _fs_hz/gcd(_fs_hz,_pcaloffset_hz).
     * The period is usually very short, say, 1600 samples. We precomputed those
     * in the constructor and use them here.
     */

    /* Process the first part that fits perfectly (and note: rotatorlen modulo _N_bins is 0!) */
    for (size_t n = 0; n < end; n+=_cfg->rotatorlen, src+=_cfg->rotatorlen) {
        ippsMul_32f32fc(/*A*/src, 
                        /*B*/&(_cfg->rotator[_cfg->rotator_index]), 
                        /*dst*/&(_cfg->rotated[_cfg->rotator_index]), 
                        /*len*/_cfg->rotatorlen);
        Ipp32fc* pulse = &(_cfg->rotated[_cfg->rotator_index]);
        for (size_t p = 0; p < (_cfg->rotatorlen/_N_bins); p++) {
            ippsAdd_32fc_I(/*src*/pulse, /*srcdst*/dst, _N_bins);
            pulse += _N_bins;
        }
    }

    /* Handle any samples that didn't fit */
    if (tail != 0) {
        ippsMul_32f32fc(
            /*A*/src, 
            /*B*/&(_cfg->rotator[_cfg->rotator_index]),
            /*dst*/&(_cfg->rotated[_cfg->rotator_index]), 
            /*len*/tail);
        Ipp32fc* pulse = &(_cfg->rotated[_cfg->rotator_index]);
        size_t tail2 = (tail % _N_bins);
        size_t end2  = tail - tail2;
        for (size_t p = 0; p < (end2/_N_bins); p++) {
            ippsAdd_32fc_I(/*src*/pulse, /*srcdst*/dst, _N_bins);
            pulse += _N_bins;
        }
        /* Samples that didn't fit _N_bins */
        ippsAdd_32fc_I(/*src*/pulse, /*srcdst*/dst, tail2);
        _cfg->rotator_index = (_cfg->rotator_index + tail) % _cfg->rotatorlen;
        _cfg->pcal_index    = (_cfg->pcal_index + tail2)   % _N_bins;
    }

    /* Done! */
    _samplecount += len;
    return true;
}

/**
 * Performs finalization steps on the internal PCal results if necessary
 * and then copies these PCal results into the specified output array.
 * Data in the output array is overwritten with PCal results.
 *
 * @param pointer to user PCal array with getLength() values
 * @return number of samples that were integrated for the result
 */
uint64_t PCalExtractorShifting::getFinalPCal(Ipp32fc* out)
{
    if (!_finalized) {
        _finalized = true;
        ippsAdd_32fc_I(/*src*/&(_cfg->pcal_complex[_N_bins]), /*srcdst*/&(_cfg->pcal_complex[0]), _N_bins);
        IppStatus r = ippsDFTFwd_CToC_32fc(/*src*/_cfg->pcal_complex, _cfg->dft_out, _cfg->dftspec, _cfg->dftworkbuf);
        if (r != ippStsNoErr) {
            cerr << "ippsDFTFwd error " << ippGetStatusString(r);
        }
    }

    if (false && _pcalspacing_hz == 1e6) {
        // Copy only the tone bins: in PCalExtractorTrivial case
        // this should be all bins... _N_tones==_N_bins/2
        ippsCopy_32fc(_cfg->dft_out, (Ipp32fc*)out, _N_tones);
    } else {
        // Copy only the interesting bins
        size_t step = std::floor(_N_bins*(_pcalspacing_hz/_fs_hz));
        for (size_t n=0; n<(size_t)_N_tones; n++) {
            size_t idx = n*step;
            if (idx >= (size_t)_N_bins) { break; }
            out[n].re = _cfg->dft_out[idx].re;
            out[n].im = _cfg->dft_out[idx].im;
        }
    }
    return _samplecount;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// DERIVED CLASS: extraction of PCal signals with non-zero offset and FFT-implicit rotation possible
/////////////////////////////////////////////////////////////////////////////////////////////////////////

PCalExtractorImplicitShift::PCalExtractorImplicitShift(double bandwidth_hz, double pcal_spacing_hz, int pcal_offset_hz, const size_t sampleoffset)
{
    /* Derive config */
    _fs_hz          = 2 * bandwidth_hz;
    _pcalspacing_hz = pcal_spacing_hz;
    _pcaloffset_hz  = pcal_offset_hz;
    _N_bins         = _fs_hz / gcd(std::abs((double)_pcaloffset_hz), _fs_hz);
    _N_tones        = std::floor((bandwidth_hz - pcal_offset_hz) / pcal_spacing_hz) + 1;
    _cfg = new pcal_config_pimpl();

    /* Prep for FFT/DFT */
    // TODO: is IPP_FFT_DIV_FWD_BY_N or is IPP_FFT_DIV_INV_BY_N expected by AIPS&co?
    int wbufsize = 0;
    IppStatus r;
    r = ippsDFTInitAlloc_C_32fc(&(_cfg->dftspec), _N_bins, IPP_FFT_DIV_FWD_BY_N, ippAlgHintAccurate);
    if (r != ippStsNoErr) {
        cerr << "ippsDFTInitAlloc _N_bins=" << _N_bins << " error " << ippGetStatusString(r);
    }
    r = ippsDFTGetBufSize_C_32fc(_cfg->dftspec, &wbufsize);
    if (r != ippStsNoErr) {
        cerr << "ippsDFTGetBufSize error " << ippGetStatusString(r);
    }
    _cfg->dftworkbuf = (Ipp8u*)memalign(128, wbufsize);

    /* Allocate */
    _cfg->pcal_complex = (Ipp32fc*)memalign(128, sizeof(Ipp32fc) * _N_bins * 2);
    _cfg->pcal_real    = (Ipp32f*) memalign(128, sizeof(Ipp32f)  * _N_bins * 2);
    _cfg->dft_out      = (Ipp32fc*)memalign(128, sizeof(Ipp32fc) * _N_bins * 1);
    this->clear();
    cerr << "PCalExtractorImplicitShift: _Ntones=" << _N_tones << ", _N_bins=" << _N_bins << ", wbufsize=" << wbufsize << endl;
}


PCalExtractorImplicitShift::~PCalExtractorImplicitShift()
{
    free(_cfg->pcal_complex);
    free(_cfg->pcal_real);
    ippsDFTFree_C_32fc(_cfg->dftspec);
    free(_cfg->dftworkbuf);
    free(_cfg->dft_out);
    delete _cfg;
}

/**
 * Set the extracted and accumulated PCal data back to zero.
 * When several PCal are run in parallel in segments of a 
 * time slice of data, and the PCal results from each segment
 * should be combined later, care must be taken to tell the PCal
 * extraction the number/offset of the first sample in the segment
 * Typically 0 for the first segment, len(segment) for the second
 * segment, and so on, until offset of the last segment
 * which is len(subintegration_subslice)-len(segment).
 * @param sampleoffset referenced back to start of subintegration interval
 */
void PCalExtractorImplicitShift::clear()
{
    _samplecount = 0;
    _finalized   = false;
    ippsZero_32fc(_cfg->pcal_complex, _N_bins * 2);
    ippsZero_32f (_cfg->pcal_real,    _N_bins * 2);
}

/**
 * Adjust the sample offset. Should be called before extractAndIntegrate()
 * every time there is a gap or backwards shift in the otherwise contiguous
 * sample stream.
 * @param sampleoffset referenced back to start of subintegration interval
 */
void PCalExtractorImplicitShift::adjustSampleOffset(const size_t sampleoffset)
{
    _cfg->pcal_index = (sampleoffset)% _N_bins;
}

/**
 * Extracts multi-tone PCal information from a single-channel signal segment
 * and integrates it to the class-internal PCal extraction result buffer.
 * There are no restrictions to the segment length.
 *
 * If you integrate over a longer time and several segments, i.e. perform
 * multiple calls to this function, take care to keep the input
 * continuous (i.e. don't leave out samples).
 *
 * If extraction has been finalized by calling getFinalPCal() this function
 * returns False. You need to call clear() to reset.
 *
 * @paran samples Chunk of the input signal consisting of 'float' samples
 * @param len     Length of the input signal chunk
 * @return true on success
 */
bool PCalExtractorImplicitShift::extractAndIntegrate(Ipp32f const* samples, const size_t len)
{
    if (_finalized) { 
        cerr << "PCalExtractorImplicitShift::extractAndIntegrate on finalized class!" << endl;
        return false; 
    }

    Ipp32f const* src = samples;
    Ipp32f* dst = &(_cfg->pcal_real[_cfg->pcal_index]);
    size_t tail = (len % _N_bins);
    size_t end  = len - tail;

    /* This method is from Walter Brisken, it works perfectly for smallish 'len'
     * and when offset and tone spacing have suitable properties.
     * Instead of rotating the input to counteract the offset, we bin
     * into a long vector with size of the offset repeat length (again *2 to avoid
     * buffer wraps). After long-term integration, we copy desired FFT bins
     * into PCal. The time-domain PCal can be derived from inverse FFT.
     */

    /* Process the first part that fits perfectly */
    for (size_t n = 0; n < end; n+=_N_bins, src+=_N_bins) {
        ippsAdd_32f_I(src, /*srcdst*/dst, _N_bins);
    }

    /* Handle any samples that didn't fit */
    if (tail != 0) {
        ippsAdd_32f_I(src, /*srcdst*/dst, tail);
        _cfg->pcal_index = (_cfg->pcal_index + tail) % _N_bins;
    }

    /* Done! */
    _samplecount += len;
    return true;
}

/**
 * Performs finalization steps on the internal PCal results if necessary
 * and then copies these PCal results into the specified output array.
 * Data in the output array is overwritten with PCal results.
 *
 * @param pointer to user PCal array with getLength() values
 * @return number of samples that were integrated for the result
 */
uint64_t PCalExtractorImplicitShift::getFinalPCal(Ipp32fc* out)
{
   if (!_finalized) {
        _finalized = true;
        //print_f32(_cfg->pcal_real, 8);
        ippsRealToCplx_32f(/*srcRe*/_cfg->pcal_real, /*srcIm*/NULL, _cfg->pcal_complex, 2*_N_bins);
        ippsAdd_32fc_I(/*src*/&(_cfg->pcal_complex[_N_bins]), /*srcdst*/&(_cfg->pcal_complex[0]), _N_bins);
        IppStatus r = ippsDFTFwd_CToC_32fc(/*src*/ _cfg->pcal_complex, _cfg->dft_out, _cfg->dftspec, _cfg->dftworkbuf);
        if (r != ippStsNoErr) {
            cerr << "ippsDFTFwd error " << ippGetStatusString(r);
        }
    }
 
    /* Copy only the interesting bins */
    size_t step = std::floor(double(_N_bins)*(_pcalspacing_hz/_fs_hz));
    size_t offset = std::floor(double(_N_bins*_pcaloffset_hz)/_fs_hz);
    // cout << "step=" << step << " offset=" << offset << endl;
    for (size_t n=0; n<(size_t)_N_tones; n++) {
        size_t idx = offset + n*step;
        if (idx >= (size_t)_N_bins) { break; }
        out[n].re = _cfg->dft_out[idx].re;
        out[n].im = _cfg->dft_out[idx].im;
    }
    return _samplecount;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// FOR FUTURE REFERENCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
/**
 * Process samples and accumulate the detected phase calibration tone vector.
 * Accumulation takes place into the internal (ipp32fc*)pcalcfg.pcal.
 * @param  data    pointer to input sample vector
 * @param  len     length of input vector
 */
void PCal::extract_continuous(Ipp32f* data, size_t len)
{
    /* Works on almost the same principle as extract(), but
     * instead of a precomputed pcalcfg.rotatevec[], we use
     * SSE/MMX C instrinsics to compute the phase angles
     * online in registers, faster than cache access!
     */

    // Oscillator: x[1] = 2*cos(phi) * x[0] - x[-1]
    // Rewritten as two oscillators with step 2phi and offset +0,phi
    //    x1[1] = 2*cos(2phi) * x1[0] - x1[-1]
    //    x2[1] = 2*cos(2phi) * x2[0] - x2[-1]
    // Complex data layout:
    //    vec128bit = vec4float = [cos((n-1)*phi) ; sin((n-1)*phi) ; cos(n*phi)  ; sin(n*phi)]
    //    multiplicand          * [2*cos(2phi)    ; 2*cos(2phi)    ; 2*cos(2phi) ; 2*cos(2phi)]

    static __m128 mmxOldComplex_1, mmxOldComplex_0, mmxMult;

    if (sample_count == 0) {
//    if (1) {
        float dphi = pcalcfg.phase_inc;
        float p0 = sample_count * dphi;
        mmxOldComplex_0 = _mm_set_ps( sinf(p0+1*dphi), cosf(p0+1*dphi), 
                                      sinf(p0+0*dphi), cosf(p0+0*dphi) );
        mmxOldComplex_1 = _mm_set_ps( sinf(p0+3*dphi), cosf(p0+3*dphi), 
                                      sinf(p0+2*dphi), cosf(p0+2*dphi) );
        mmxMult = _mm_set1_ps( 2*cosf(2*dphi) );
    }

    float *accu = (float*)&pcalcfg.pcal[0];
    float *data_end = data + len;
    //cerr << " data=" << data << " data_end=" << data_end << " diff=" << (data_end-data) << " len=" << len << endl;
    //while (data < data_end) {
    for (size_t xxx=0; xxx<len; xxx+=pcalcfg.tonebins) {
        __m128 mmxData, mmxPcal, mmxTmp, mmxLoad;
        for (int bin=0; bin<int(pcalcfg.tonebins); bin+=4, data+=4, sample_count+=4) {
            /* rotate and bin first 2 samples */
            mmxLoad = _mm_load_ps(data);  // _mm_loadu_ps() if unaligned
            mmxData = _mm_unpacklo_ps(mmxLoad, mmxLoad);
            mmxPcal = _mm_load_ps(&accu[2*bin+0]);
            mmxData = _mm_mul_ps(mmxData, mmxOldComplex_0);
            mmxPcal = _mm_add_ps(mmxPcal, mmxData);
            _mm_store_ps(&accu[2*bin+0], mmxPcal);

//_mm_store_ps(v4tmp, mmxOldComplex_1);
//printf(" %f %f %f %f\n", v4tmp[0], v4tmp[1], v4tmp[2], v4tmp[3]);

            /* rotate and bin next 2 samples */
            mmxData = _mm_unpackhi_ps(mmxLoad, mmxLoad);
            mmxPcal = _mm_load_ps(&accu[2*bin+4]);
            mmxData = _mm_mul_ps(mmxData, mmxOldComplex_1);
            mmxPcal = _mm_add_ps(mmxPcal, mmxData);
            _mm_store_ps(&accu[2*bin+4], mmxPcal);
            /* compute next four complex value pairs */
            mmxTmp = _mm_mul_ps(mmxMult, mmxOldComplex_1);
            mmxTmp = _mm_sub_ps(mmxTmp, mmxOldComplex_0);
            mmxOldComplex_0 = mmxOldComplex_1;
            mmxOldComplex_1 = mmxTmp;
            mmxTmp = _mm_mul_ps(mmxMult, mmxOldComplex_1);
            mmxTmp = _mm_sub_ps(mmxTmp, mmxOldComplex_0);
            mmxOldComplex_0 = mmxOldComplex_1;
            mmxOldComplex_1 = mmxTmp;
         }
   }

//    sample_count += len;
    return;
}

#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// DERIVED CLASS: PCal class that returns only known dummy values.
/////////////////////////////////////////////////////////////////////////////////////////////////////////

PCalExtractorDummy::PCalExtractorDummy(double bandwidth_hz, double pcal_spacing_hz, const size_t sampleoffset)
{
  /* ignore all */
  _fs_hz   = 2*bandwidth_hz;
  _N_bins  = _fs_hz / gcd(std::abs(pcal_spacing_hz), _fs_hz);
  _N_tones = std::floor(bandwidth_hz / pcal_spacing_hz);
  _cfg = (pcal_config_pimpl*)1;
  this->clear();
  cerr << "PCalExtractorDummy: _Ntones=" << _N_tones << ", _N_bins=" << _N_bins << endl;
}

PCalExtractorDummy::~PCalExtractorDummy()
{
}

/**
 * Set the extracted and accumulated PCal data back to zero.
 * When several PCal are run in parallel in segments of a 
 * time slice of data, and the PCal results from each segment
 * should be combined later, care must be taken to tell the PCal
 * extraction the number/offset of the first sample in the segment
 * Typically 0 for the first segment, len(segment) for the second
 * segment, and so on, until offset of the last segment
 * which is len(subintegration_subslice)-len(segment).
 * @param sampleoffset referenced back to start of subintegration interval
 */
void PCalExtractorDummy::clear()
{
  _samplecount = 0;
  _finalized   = false;
}

/**
 * Adjust the sample offset. Should be called before extractAndIntegrate()
 * every time there is a gap or backwards shift in the otherwise contiguous
 * sample stream.
 * @param sampleoffset referenced back to start of subintegration interval
 */
void PCalExtractorDummy::adjustSampleOffset(const size_t sampleoffset)
{
    (void)sampleoffset;
}

/**
 * Does not do much.
 *
 * If extraction has been finalized by calling getFinalPCal() this function
 * returns False. You need to call clear() to reset.
 *
 * @paran samples Chunk of the input signal consisting of 'float' samples
 * @param len     Length of the input signal chunk
 * @return true on success
 */
bool PCalExtractorDummy::extractAndIntegrate(Ipp32f const* samples, const size_t len)
{
  if (false && _finalized) { 
      cerr << "Dummy::extract on finalized results!" << endl;
      return false; 
  }
  _samplecount += len;
  if (false && _samplecount>0 && _samplecount<1024) { // print just a few times
     cerr << "_samplecount=" << _samplecount;
  }
  return true;
}

/**
 * Performs finalization steps on the internal PCal results if necessary
 * and then copies these PCal results into the specified output array.
 * Data in the output array is overwritten with PCal results.
 *
 * @param pointer to user PCal array with getLength() values
 * @return number of samples that were integrated for the result
 */
uint64_t PCalExtractorDummy::getFinalPCal(Ipp32fc* out)
{
    if (false && _samplecount == 0) {
        cerr << "Dummy::getFinalPCal called without call to extractAndIntegrate()!";
        cerr << "                           or after call to clear()!" << endl;
    }
    _finalized = true;

    // Fill the output array with dummy values  
    for (int i = 0; i < _N_tones; i++) {
        out[i].re = sqrt(2.0); // * _samplecount;
        out[i].im = sqrt(2.0) * _samplecount;
    }
    return _samplecount;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// UNIT TEST (NON-AUTOMATED, MANUAL VISUAL CHECK)
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef UNIT_TEST

/* Example:
g++ -m64 -DUNIT_TEST -Wall -O3 -I$(IPPROOT)/include/ -pthread -I.. pcal.cpp -o test \
 -L$(IPPROOT)/sharedlib -L$(IPPROOT)/lib -lippsem64t -lguide -lippvmem64t -lippcoreem64t

./test 32000 16e6 1e6 510e3 0
*/

#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdlib.h>
void print_32f(const Ipp32f* v, const size_t len);
void print_32fc(const Ipp32fc* v, const size_t len);
void print_32fc_phase(const Ipp32fc* v, const size_t len);
void compare_32fc_phase(const Ipp32fc* v, const size_t len, Ipp32f angle, Ipp32f step);

int main(int argc, char** argv)
{
   bool sloping_reference_data = true;
   bool skip_some_data = true;
   const long some_prime = 3;
   uint64_t usedsamplecount;

   if (argc < 6) {
      cerr << "Usage: " << argv[0] << " <samplecount> <bandwidthHz> <spacingHz> <offsetHz> <sampleoffset>" << endl;
      return -1;
   }
   long samplecount = atof(argv[1]);
   long bandwidth = atof(argv[2]);
   long spacing = atof(argv[3]);
   long offset = atof(argv[4]);
   long sampleoffset = atof(argv[5]);
   cerr << "BWHz=" << bandwidth << " spcHz=" << spacing << ", offHz=" << offset << ", sampOff=" << sampleoffset << endl;

   /* Get an extractor */
   PCal* extractor = PCal::getNew(bandwidth, spacing, offset, sampleoffset);
   int numtones = extractor->getLength();
   cerr << "extractor->getLength() tone count is " << numtones << endl;
   Ipp32fc* out = (Ipp32fc*)memalign(128, sizeof(Ipp32fc)*numtones);
   Ipp32fc* ref = (Ipp32fc*)memalign(128, sizeof(Ipp32fc)*numtones);

   /* Make test data for fixed -90deg or sloping -90deg+5deg*ToneNr phase */
   float* data = (float*)memalign(128, samplecount*sizeof(float));
   for (long n=0; n<samplecount; n++) {
      data[n] = 0; //rand()*1e-9;
      for (int t=0; t<numtones; t++) {
          if (sloping_reference_data) {
              data[n] += sin(M_PI*(n+sampleoffset)*(offset + t*spacing)/bandwidth + t*M_PI*5/180);
          } else {
              data[n] += sin(M_PI*(n+sampleoffset)*(offset + t*spacing)/bandwidth);
          }
      }
   }

   /* Extract with the autoselected fast method */
   extractor->extractAndIntegrate(data, samplecount);
   if (skip_some_data && (samplecount > some_prime)) {
       long offset = sampleoffset + samplecount + some_prime;
       extractor->adjustSampleOffset(offset);
       extractor->extractAndIntegrate(data + some_prime, samplecount - some_prime);
   }
   usedsamplecount = extractor->getFinalPCal(out);

   /* Check result */
   if (sloping_reference_data) {
       cerr << "Expected result: tones are sloping by -5deg each" << endl;
   } else {
       cerr << "Expected result: each tone has a fixed -90deg phase" << endl;
   }
   // cerr << "final PCal reim: ";
   // print_32fc(out, numtones);
   cerr << "final PCal phase: ";
   print_32fc_phase(out, numtones);
   compare_32fc_phase(out, numtones, -90.0f, (sloping_reference_data) ? 5.0f : 0.0f);

   /* Comparison with the (poorer) "reference" extracted result */
   extractor->clear();
   if (skip_some_data) {
       ippsAdd_32f_I(data+some_prime, data+some_prime, samplecount-some_prime);
   }
   extractor->extractAndIntegrate_reference(data, samplecount, ref, sampleoffset);
   //cerr << "reference PCal reim: ";
   //print_32fc(ref, numtones);
   cerr << "quasi-reference PCal phase: ";
   print_32fc_phase(ref, numtones); // should be ~ -90deg
   compare_32fc_phase(ref, numtones, -90.0f, (sloping_reference_data) ? +5.0f : +0.0f);

   return 0;
}

void print_32f(const Ipp32f* v, const size_t len) {
   for (size_t i=0; i<len; i++) { cerr << std::scientific << v[i] << " "; }
}

void print_32fc(const Ipp32fc* v, const size_t len) {
   for (size_t i=0; i<len; i++) { cerr << std::scientific << v[i].re << "+i" << v[i].im << " "; }
}

void print_32fc_phase(const Ipp32fc* v, const size_t len) {
   for (size_t i=0; i<len; i++) { 
      float phi = (180/M_PI)*std::atan2(v[i].im, v[i].re);
      cerr << std::scientific << phi << " ";
   }
   cerr << "deg";
}

void compare_32fc_phase(const Ipp32fc* v, const size_t len, Ipp32f angle, Ipp32f step) {
   bool pass = true;
   for (size_t i=0; i<len; i++) { 
      float phi = (180/M_PI)*std::atan2(v[i].im, v[i].re);
      if (std::abs(phi - angle) > 1e-1) { // degrees
          cerr << "tone #" << (i+1) << ": expect " << angle << ", got " << phi;
          pass = false;
      }
      angle += step;
   }
   cerr << "Extracted versus expected: " << ((pass) ? "PASS" : "NO PASS (or PASS but missed phase ambiguity)") << endl;
}

#endif
