#ifndef PROCESS_PARAMS_H
#define PROCESS_PARAMS_H

#include  <stdint.h>
#include  <complex>
#include  "ringbuffer.h"

class ProcessParams
{
public:
  float threshold = 0;
  int16_t tiiFramesToCount = 0;
  RingBuffer<float> * responseBuffer = nullptr;
  RingBuffer<cmplx> * spectrumBuffer = nullptr;
  RingBuffer<cmplx> * cirBuffer = nullptr;
  RingBuffer<cmplx> * iqBuffer = nullptr;
  RingBuffer<float> * carrBuffer = nullptr;
  RingBuffer<uint8_t> * frameBuffer = nullptr;
};

#endif

