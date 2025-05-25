#ifndef PROCESS_PARAMS_H
#define PROCESS_PARAMS_H

#include  <stdint.h>
#include  <complex>
#include  "ringbuffer.h"

class ProcessParams
{
public:
  f32 threshold = 0;
  i16 tiiFramesToCount = 0;
  RingBuffer<f32> * responseBuffer = nullptr;
  RingBuffer<cf32> * spectrumBuffer = nullptr;
  RingBuffer<cf32> * cirBuffer = nullptr;
  RingBuffer<cf32> * iqBuffer = nullptr;
  RingBuffer<f32> * carrBuffer = nullptr;
  RingBuffer<u8> * frameBuffer = nullptr;
};

#endif

