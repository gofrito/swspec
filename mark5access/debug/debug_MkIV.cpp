
#include <iostream>
#include <fstream>
#include <algorithm>
using std::endl;
using std::cerr;
using std::cout;

extern "C" {
    #include <mark5access.h>
}

#include <memory.h>
#include <malloc.h>

#define MAX_UNPACK_SAMPLES (6*80000)

// Globals
float** allchannels;
float*  singlechannel;
char*   rawbuffer;

// Declarations
mark5_stream* open_stream(std::string const&, std::string const&, const int);
mark5_stream* unpack_init(mark5_stream* m5s, std::string const&);
void          move_to_integer_second(mark5_stream*);
int           read_stream(mark5_stream*, char*, const int);

void printstamp(int mjd, int sec, double ns, std::string tail) { 
   cerr << mjd << " " << sec << " " << " " << ns << tail;
}

/**
 * Do a seek() to the first integer second in the stream.
 */
void move_to_integer_second(mark5_stream* m5s)
{
   int mjd = 1, sec = 2, newsec = 2;
   double ns = 0.5f;
   mark5_stream_get_sample_time(m5s, &mjd, &sec, &ns);
   if (ns > 0.0f) {
       cerr << "Stream at non-integer second: ";
       printstamp(mjd, sec, ns, "\n");
       cout << "Stream is at non-integer second: " << endl;
	   mark5_stream_print(m5s);
       if (mark5_stream_seek(m5s, mjd, sec + 1, 0.0f) != 0) {
           cerr << "move_to_integer_second: error during mark5_stream_seek" << endl;
       }
       mark5_stream_get_sample_time(m5s, &mjd, &newsec, &ns);
       cerr << "Stream after seek: ";
       printstamp(mjd, newsec, ns, "\n");
	   if (newsec != (sec+1) ||  ns!=0.0f) {
			cerr << "Stream seek unsuccessful: timestamp is not ";
            printstamp(mjd, sec+1, ns, "\n");
       } else {
           cerr << "Testing mark5_stream_print: timestamp should match the above one" << endl;
       }
       cout << "Stream after seek: " << endl;
	   mark5_stream_print(m5s);
	} else {
       if (mark5_stream_seek(m5s, mjd, sec, ns) != 0) {
          cerr << "Reseek fail!" << endl;
       }
       cerr << "Stream is at integer second: ";
       printstamp(mjd, sec, ns, "\n");
       cout << "Stream is at integer second: " << endl;
       mark5_stream_print(m5s);
    }
}


/*
 * Create and return new opened file stream. Pre-seeks to the first integer
 * second plus user-specified seconds offset.
 */
mark5_stream* open_stream(std::string const& file, std::string const& format, const int seconds_to_skip)
{
   /* Create from file */
   mark5_stream* m5s = new_mark5_stream( 
           new_mark5_stream_file(file.c_str(), /*offset:*/0),
           new_mark5_format_generic_from_string(format.c_str())
   );
   if (!m5s) return NULL;

   /* Get to correct spot */
   cout << "Before move_to_integer_second():" << endl;
   mark5_stream_print(m5s);
   move_to_integer_second(m5s);
   
   /* Skip a certain amout of seconds */
   if (seconds_to_skip > 0) {
        int mjd, sec; 
        double ns;
        mark5_stream_get_sample_time(m5s, &mjd, &sec, &ns);
        cerr << "Skipping from ";
        printstamp(mjd, sec, ns, " to ");
        printstamp(mjd, sec + seconds_to_skip, 0.0f, "\n");
        sec += seconds_to_skip; // TODO: detect day boundaries?
        if (mark5_stream_seek(m5s, mjd, sec, 0.0f) != 0) {
            cout << "open_stream: error during mark5_stream_seek" << endl;
        } else {
            cout << "Testing mark5_stream_print: timestamp should match the above one" << endl;
            mark5_stream_print(m5s);
		}
	}
    return m5s;
}

/*
 * Read raw data from the stream.
 */
int read_stream(mark5_stream* m5s, char* buf, int bufsize) 
{
    if (mark5_stream_copy(m5s, bufsize, buf) != 0) {
	    int bytegranularity = m5s->samplegranularity * m5s->nbit * m5s->nchan / 8;
        cerr << "mark5_stream_copy returned error (or EOF!) - check that bufsize " << bufsize 
             << " matches granularity " << bytegranularity << endl;
        return 0;
    } else {
        return bufsize;
   }
}

/*
 * Create new unpacker, allocate buffers (global 'allchannels').
 */
mark5_stream* unpack_init(mark5_stream* m5s, std::string const& format)
{ 
    /* Create pseudo-stream */
    mark5_stream* m5ups = new_mark5_stream(
        new_mark5_stream_unpacker(/*noheaders:*/ 1),
        new_mark5_format_generic_from_string(format.c_str())
    );

    /* <mark5access/doc/UserGuide>
     *
     * 3.2.3.1 struct mark5_stream_generic *new_mark5_stream_unpacker(int noheaders)
     *
     * This function makes a new pseudo-stream.  "noheaders" should be set to 0
     * if the data to be later unpacked is in its original form and should be 1
     * if the data to be later unpacked has its headers stripped.  When used in
     * conjunction with mark5_stream_copy, "noheaders" should be set to 1.
     */

    if (!m5ups) return NULL;

    /* Allocate unpacker output buffer */
    allchannels = (float**)malloc(m5s->nchan * sizeof(float*));
    for (int ch=0; ch < m5s->nchan; ch++) {
        allchannels[ch] =  (float *)memalign(128, MAX_UNPACK_SAMPLES * sizeof(float));
        if (!allchannels[ch]) { cerr << "malloc fail" << endl; }
    }
    return m5ups;
}

/*
 * Take a raw data buffer and use specified pseudo-stream to unpack one channel.
 */
size_t unpack_samples(mark5_stream* m5ups, char const* const src, float* dst, const int count, const int channel)
{
    /* Report sample time */
	if (0) {
        int mjd, sec; double ns;
        // this segfaults:
        mark5_stream_get_sample_time(m5ups, &mjd, &sec, &ns);
        cerr << "extract_samples at timestamp: ";
        printstamp(mjd, sec, ns, "\n");
	}

    /* Unpack the raw data and copy the desired channel */
    int ch = (channel % m5ups->nchan);
    int n =  (count % (MAX_UNPACK_SAMPLES+1));
    mark5_unpack(m5ups, (void *)src, allchannels, n);
	for (int i=0; i<n; i++) {
		dst[i] = allchannels[ch][i];
    }
    return count;
}

/*
 * Test driver
 */
int main(int argc, char* argv[])
{
    /* Usage */
    if (argc < 2) {
	    cerr << "usage: debug_MkIV <filename> [outname]" << endl;
        return 0;
    }

    /* Alloc globals */
    rawbuffer = new char[MAX_UNPACK_SAMPLES + 80000];
    singlechannel = new float[MAX_UNPACK_SAMPLES];

    /* Our vars */
//    std::string format("VLBA1_4-256-4-2");/*"MkIV1_4-128-4-2");*/
    std::string format("MkIV1_4-128-4-2");
    mark5_stream* m5s = NULL;
    mark5_stream* m5ups = NULL;
    int seconds_to_skip = 0;
    int channel_nr = 0;

    /* Stream init wrappers */
    m5s = open_stream(argv[1], format, seconds_to_skip);
    m5ups = unpack_init(m5s, format);

    /* Read file stream into memory */
    read_stream(m5s, rawbuffer, MAX_UNPACK_SAMPLES);
    cout << "Raw read result: " << endl << std::hex;
    for (int i=0; i<32 && i<MAX_UNPACK_SAMPLES; i++) {
        unsigned char c = (unsigned char)rawbuffer[i];
        cout << (unsigned int)c << " ";
        if (i%16 == 15) cout << endl;
    }
    cout << endl << std::oct;

    /* Dump the memory into a file */
    if (argc == 3) {
        std::ofstream fout(argv[2]);
        fout.write(rawbuffer, MAX_UNPACK_SAMPLES);
        fout.close();
    }

    /* Fill result buffer with data to see headers (where mk5access wont touch original resukt buf data) */
    for (int i=0; i<MAX_UNPACK_SAMPLES; i++) {
		for (int ch=0; ch < m5s->nchan; ch++) {
			allchannels[ch][i] = float(i);
	   }
    } 
   
    /* Unpack from memory */
    unpack_samples(m5ups, rawbuffer, singlechannel, MAX_UNPACK_SAMPLES, channel_nr);

    /* Dump to screen */
    //int len = 16;
    int len = MAX_UNPACK_SAMPLES;
    for (int i=0; i<len; i++) {
        cout << std::fixed << singlechannel[i] << " ";
        if (i%16 == 15) cout << endl;
    }
    cout << endl;


    /* Dump the unpacked data into a file */
    if (argc == 4) {
        std::ofstream fout(argv[3]);
        fout.write((char*)singlechannel, sizeof(float)*MAX_UNPACK_SAMPLES);
        fout.close();
    }
    return 0;
}
