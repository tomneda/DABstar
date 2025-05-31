#ifndef  VITERBI_SPIRAL_H
#define  VITERBI_SPIRAL_H
/*
 * 	Viterbi.h according to the SPIRAL project
 */
#include "dab-constants.h"

#define NUMSTATES 64

typedef struct {
	u32 w[NUMSTATES / 32];
} decision_t;

class ViterbiSpiral
{
public:
  explicit ViterbiSpiral(const short iWordlength, const bool iSpiralMode = false);
  ~ViterbiSpiral();

  void deconvolve(const short * const input, u8 * const output);
  void calculate_BER(const short * const input, u8 *punctureTable,
  	                 u8 const *output, i32 &bits, i32 &errors);
private:
  i32 parity(i32);
  const short mFrameBits;
  const bool mSpiral;
  decision_t *decisions  = nullptr;
};

#endif
