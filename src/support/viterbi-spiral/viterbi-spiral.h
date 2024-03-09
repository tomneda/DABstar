#ifndef  VITERBI_SPIRAL_H
#define  VITERBI_SPIRAL_H
/*
 * 	Viterbi.h according to the SPIRAL project
 */
#include  "dab-constants.h"

//	For our particular viterbi decoder, we have
#define  RATE  4
#define NUMSTATES 64
#define DECISIONTYPE uint32_t
//#define DECISIONTYPE uint8_t
//#define DECISIONTYPE_BITSIZE 8
#define DECISIONTYPE_BITSIZE 32
#define COMPUTETYPE uint32_t

//decision_t is a BIT vector
union alignas(16) decision_t
{
  DECISIONTYPE t[NUMSTATES / DECISIONTYPE_BITSIZE];
  uint32_t w[NUMSTATES / 32];
  uint16_t s[NUMSTATES / 16];
  uint8_t c[NUMSTATES / 8];
};

union alignas(16) metric_t
{
  COMPUTETYPE t[NUMSTATES];
};

/* State info for instance of Viterbi decoder
 */

struct SMetricData
{
  /* path metric buffer 1 */
  alignas(16) metric_t metrics1;
  /* path metric buffer 2 */
  alignas(16) metric_t metrics2;
  /* Pointers to path metrics, swapped on every bit */
  metric_t * old_metrics, * new_metrics;
  decision_t * decisions;   /* decisions */
};

class ViterbiSpiral
{
public:
  explicit ViterbiSpiral(const int16_t iWordlength, const bool iSpiralMode = false);
  ~ViterbiSpiral();

  void deconvolve(const int16_t * const input, uint8_t * const output);

private:
  const int16_t mFrameBits;
  const bool mSpiral;
  SMetricData mVP{};
  alignas(16) COMPUTETYPE mBranchtab[NUMSTATES / 2 * RATE];
  uint8_t * mpData = nullptr;
  COMPUTETYPE * mpSymbols = nullptr;

  static void init_viterbi(SMetricData *, int16_t);
  static void chainback_viterbi(const SMetricData * const iVP, uint8_t * const oData, int16_t nbits, const uint16_t iEndstate);

  int parity(int);
  void update_viterbi_blk_GENERIC(SMetricData *, COMPUTETYPE *, int16_t);
  void update_viterbi_blk_SPIRAL(SMetricData *, COMPUTETYPE *, int16_t);
  void BFLY(int32_t, int, COMPUTETYPE *, SMetricData *, decision_t *);
};

#endif

