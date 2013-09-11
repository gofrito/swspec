#include "PCal.h"
#include <iostream>
#include <cmath>

#include <sys/time.h>
#include <time.h>
#include <stdlib.h> // random
#include <malloc.h> // memalign
#include <string.h> // memcpy

#define N_ITER 2

using std::cerr;
using std::endl;

void print_32fc_vec(const Ipp32fc* data, int n)
{
   while (n-->0) {
      printf("%f+%fi ", data[0].re, data[0].im);
      data++;
   }
}

int main(int argc, char** argv)
{
   struct timeval t_start, t_stop;
   double dT, r_Msps;

   /* Settings */
   size_t samplelen = (1<<20);
   double samplingfreq = 16e6;
   double pcalspacinghz = 1e6;
   #if 0
      double pcaloffsethz = 0.0f; // tests class PCalExtractorTrivial
   #else
      double pcaloffsethz = 10e3; // tests class PCalExtractorShifting
   #endif

   /* Initialize */
   PCal* extractor = PCal::getNew(samplingfreq/2, pcalspacinghz, pcaloffsethz);
   const size_t pcalLen = extractor->getLength();
   Ipp32fc pcal[pcalLen];

   /* Prepare a test feed */
   Ipp32f* samples = (Ipp32f*)memalign(128, sizeof(Ipp32f)*samplelen);
   Ipp32f* samples_orig = (Ipp32f*)memalign(128, sizeof(Ipp32f)*samplelen);
   for (size_t i=0; i<samplelen; i++) {
      // samples_orig[i] = (Ipp32f)random() / 65536.0;
      samples_orig[i] = 3.335 * sinf((float)(i)*2*M_PI*(1e6+10e3)/16e6);
   }

   /* Reference implementation */
   ippsZero_32fc(pcal, pcalLen);
   memcpy(samples, samples_orig, samplelen*sizeof(Ipp32f));
   cerr << "Direct method extract_analytic(): ";

   extractor->clear();
   gettimeofday(&t_start, NULL);
   for (int i=0; i<N_ITER; i++) {
       cerr << extractor->extractAndIntegrate_reference(samples, samplelen, pcal) << endl;
   }
   gettimeofday(&t_stop, NULL);

   dT = (t_stop.tv_sec - t_start.tv_sec) + 1e-6*(t_stop.tv_usec - t_start.tv_usec);
   r_Msps = ((N_ITER*samplelen)/dT)*1e-6;
   cerr << "Samples: " << N_ITER * samplelen << endl;
   cerr << "Integration time: " << extractor->getSeconds() << "s" << endl;
   cerr << "Exec time: " << (1e6*dT) << " usec" << endl;
   cerr << "Rate: " << r_Msps << " Ms/s, " << r_Msps*(4e-3*sizeof(float)) << " GB/s(dec)" << endl;
   print_32fc_vec(pcal, pcalLen); printf("\n\n");

   /* Vectorized implementation */
   ippsZero_32fc(pcal, pcalLen);
   memcpy(samples, samples_orig, samplelen*sizeof(Ipp32f));
   cerr << "Precooked method extract(): ";

   extractor->clear();
   gettimeofday(&t_start, NULL);
   for (int i=0; i<N_ITER; i++) {
      cerr << extractor->extractAndIntegrate(samples, samplelen) << endl;
   }
   extractor->getFinalPCal(pcal);
   gettimeofday(&t_stop, NULL);

   dT = (t_stop.tv_sec - t_start.tv_sec) + 1e-6*(t_stop.tv_usec - t_start.tv_usec);
   r_Msps = ((N_ITER*samplelen)/dT)*1e-6;
   cerr << "Samples: " << N_ITER * samplelen << endl;
   cerr << "Integration time: " << extractor->getSeconds() << "s" << endl;
   cerr << "Exec time: " << (1e6*dT) << " usec" << endl;
   cerr << "Rate: " << r_Msps << " Ms/s, " << r_Msps*(4e-3*sizeof(float)) << " GB/s(dec)" << endl;
   print_32fc_vec(pcal, pcalLen); printf("\n\n");

   return 0;
}
