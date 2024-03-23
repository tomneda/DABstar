#ifndef PROCESS_PARAMS_H
#define PROCESS_PARAMS_H

#include  <stdint.h>
#include  <complex>
#include  "ringbuffer.h"
#include  "qwt_defs.h"

class ProcessParams
{
public:
  uint8_t dabMode = 0;
  int16_t threshold = 0;
  int16_t diff_length = 0;
  int16_t tii_delay = 0;
  int16_t tii_depth = 0;
  int16_t echo_depth = 0;
  int16_t bitDepth = 0;
  RingBuffer<float> * responseBuffer = nullptr;
  RingBuffer<cmplx> * spectrumBuffer = nullptr;
  RingBuffer<cmplx> * iqBuffer = nullptr;
  RingBuffer<TQwtData> * carrBuffer = nullptr;
  RingBuffer<uint8_t> * frameBuffer = nullptr;
};

#endif

