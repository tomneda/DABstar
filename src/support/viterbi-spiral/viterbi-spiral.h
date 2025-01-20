#ifndef  VITERBI_SPIRAL_H
#define  VITERBI_SPIRAL_H
/*
 * 	Viterbi.h according to the SPIRAL project
 */
#include "dab-constants.h"

#define NUMSTATES 64

typedef struct {
	unsigned int w[NUMSTATES / 32];
} decision_t;

class ViterbiSpiral
{
public:
  explicit ViterbiSpiral(const short iWordlength, const bool iSpiralMode = false);
  ~ViterbiSpiral();

  void deconvolve(const short * const input, unsigned char * const output);
  void calculate_BER(const short * const input, unsigned char *punctureTable,
  	                 unsigned char const *output, int &bits, int &errors);
private:
  int parity(int);
  const short mFrameBits;
  const bool mSpiral;
  int * mpSymbols = nullptr;
  decision_t *decisions  = nullptr;
};

#endif
