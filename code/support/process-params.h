#
#ifndef	__PROCESS_PARAMS__
#define __PROCESS_PARAMS__

#include	<stdint.h>
#include	<complex>
#include	"ringbuffer.h"

class	processParams {
public:
	uint8_t	dabMode;
	int16_t	threshold;
	int16_t	diff_length;
	int16_t	tii_delay;
	int16_t	tii_depth;
	int16_t	echo_depth;
	int16_t	bitDepth;
	RingBuffer<float>	* responseBuffer;
	RingBuffer<cmplx> * spectrumBuffer;
	RingBuffer<cmplx> * iqBuffer;
	RingBuffer<cmplx> * tiiBuffer;
	RingBuffer<cmplx> * nullBuffer;
	RingBuffer<float>		* snrBuffer;
	RingBuffer<uint8_t> *frameBuffer;
};

#endif

