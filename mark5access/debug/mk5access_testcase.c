#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <memory.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>

#include <mark5access.h>

// For testing, use: MKIV1_4-128-4-2 (i.e. a format where number of raw bytes == number of samples contained)
//
// Running Linux abidal 2.6.27-7-generic #1 SMP Tue Nov 4 19:33:06 UTC 2008 x86_64 GNU/Linux
// with 64-bit userland
//
// #define FFT_POINTS 1600000 => segfault
//   Program received signal SIGSEGV, Segmentation fault.
//   0x00007fe9906299ce in blanker_mark4 (ms=0x1d6d460) at blanker_mark5.c:237
//   23  endOK   = data[e-1] != MARK5_FILL_WORD64
// 
//  works: (1<<17)
//  works: (8*16000)
//  stream copy mistake: (8*32000)
//  stream copy mistake: (1<<18)

#define FFT_POINTS (1<<18)

#define MK5EXEC(call, str) do { if(call){printf("FAILED: %s\n",str);}else{printf("Success: %s\n",str);} } while(0)
#define MK5NEW(var, call, str) do { var=call; if(!var){printf("FAILED: %s\n",str);}else{printf("Success: %s\n",str);} } while(0)

// Copied from mark5access/mark5_stream_file.c, required to get the file offset!
#define MAX_MARK5_STREAM_FILES  32
struct mark5_stream_file
{
   int64_t offset;
   int64_t filesize;
   char files[MAX_MARK5_STREAM_FILES][256];
   int nfiles;
   int buffersize;
   int curfile;
   int fetchsize;
   int in;
   uint8_t *buffer;
   uint8_t *end;
   uint8_t *last;
};

// Helper
void binary_dump(const char* fname, char* buf, size_t n)
{
   FILE* F = fopen(fname, "w");
   fwrite(buf, 1, n, F);
   fclose(F);
}

// Test case: seek to integer sec and get offset, reopen, copy samples, unpack
int main(int argc, char** argv)
{
   char* filename;
   char* format;
   off64_t integer_sec_offset;

   struct mark5_stream_file* m5_F = NULL;
   struct mark5_format_generic* m5fmt = NULL;
   struct mark5_format_generic* m5fmtu = NULL;
   struct mark5_stream_generic* m5sgen = NULL;
   struct mark5_stream_generic* m5sunp = NULL;
   struct mark5_stream* m5fs = NULL;
   struct mark5_stream* m5unp = NULL;

   int mjd, sec, nframes, i;
   double ns;

   float** unpacked;
   char*   rawbuffer;
 
   if (argc < 3) {
       printf("Usage: mk5access_testcase <format> <file>\n\n");
       return -1;
   }
   format = argv[1];
   filename = argv[2];

   /* Open and get offset of first integer second */
   MK5NEW ( m5fmt,  new_mark5_format_generic_from_string(format), "new_mark5_format_generic_from_string" );
   MK5NEW ( m5sgen, new_mark5_stream_file(filename, 0), "new_mark5_stream_file with zero offset" );
   MK5NEW ( m5fs,   new_mark5_stream(m5sgen, m5fmt), "first new_mark5_stream from 0-offset file" );

   MK5EXEC ( mark5_stream_get_sample_time(m5fs, &mjd, &sec, &ns), "mark5_stream_get_sample_time" );
   MK5EXEC ( mark5_stream_print(m5fs), "mark5_stream_print before seek" );
   MK5EXEC ( mark5_stream_seek(m5fs, mjd, sec+1, 0.0f), "mark5_stream_seek to integer second" );

   /* Ugly part to determine the file offset! */
   int32_t nsToGo = (m5fs->ns == 0) ? 0 : 1000000000 - m5fs->ns;
   int64_t nFrameOffset = nsToGo / m5fs->framens;
   integer_sec_offset = m5fs->framebytes * nFrameOffset + m5fs->frameoffset;
   printf("Determined integer_sec_offset to be %llu\n", (unsigned long long)integer_sec_offset);

   /* Open at determined offset */
   delete_mark5_stream(m5fs); // seems to delete m5sgen+m5fmt as well!
   MK5NEW  ( m5fmt,  new_mark5_format_generic_from_string(format), "new_mark5_format_generic_from_string" );
   MK5NEW  ( m5sgen, new_mark5_stream_file(filename, integer_sec_offset), "new_mark5_stream_file with non-0 offset" );
   MK5NEW  ( m5fs,   new_mark5_stream(m5sgen, m5fmt), "re-opened new_mark5_stream from non-0 offset file" );
   MK5EXEC ( mark5_stream_print(m5fs), "mark5_stream_print after reopening" );

   /* Prepare to copy and unpack */
   rawbuffer = (char*)memalign(128, FFT_POINTS);
   MK5NEW  ( m5fmtu,  new_mark5_format_generic_from_string(format), "new_mark5_format_generic_from_string for unpacker" );
   MK5NEW  ( m5sunp, new_mark5_stream_unpacker(/*noheaders:*/1), "new_mark5_stream_unpacker" );
   MK5NEW  ( m5unp,  new_mark5_stream(m5sunp, m5fmtu), "new_mark5_stream with unpacker" );
   MK5EXEC ( mark5_stream_print(m5unp), "mark5_stream_print unpacker stream" );
   unpacked  = (float**)malloc(m5unp->nchan * sizeof(float*));
   for (i=0; i<(m5unp->nchan); i++) {
      unpacked[i] = (float*)memalign(128, FFT_POINTS*sizeof(float));
   }
   printf("Alloc: input with %d bytes, output array with %dx%d samples\n", FFT_POINTS, m5unp->nchan, FFT_POINTS);

   /* Now copy */
   MK5EXEC ( mark5_stream_copy(m5fs, FFT_POINTS, rawbuffer), "mark5_stream_copy" );
   binary_dump("mark5_stream_copy.bin", rawbuffer, FFT_POINTS);
   for (i=0; i<(m5unp->framebytes); i++) {
      if (rawbuffer[i] != rawbuffer[i + m5unp->framebytes]) break;
   }
   if (i==m5unp->framebytes) {
       printf("FAIL: frame 0 and 1 are identical!\n");
   } else {
       printf("Success: frame 0 and 1 are distinct\n");
   }

   /* Now unpack and check the 0.0f-valued e.g. header vector length */
   printf("Unpacking %d samples per channel\n", FFT_POINTS);
   int actual_unpacked =  mark5_unpack(m5unp, rawbuffer, unpacked, FFT_POINTS);
   printf("Unpacked actual: %d samples per channel\n", actual_unpacked);
   binary_dump("mark5_unpack_channel0out.bin", (char*)(unpacked[0]), FFT_POINTS*sizeof(float));
   i = 0;
   while ((i<FFT_POINTS) && (unpacked[0][i] == 0)) { i++; }
   printf("Header length: %d should be 640@1:4 or 320@1:2 or 160@1:1 fanout\n", i);

   return 0;
}
