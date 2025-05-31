
#include	"bandpass-filter.h"
/*
 *	The bandfilter is for the complex domain. 
 *	We create a lowpass filter, which stretches over the
 *	positive and negative half, then shift this filter
 *	to the right position to form a nice bandfilter.
 *	For the real domain, we use the Simple BandPass version.
 */
	BandPassFIR::BandPassFIR (i16 filterSize,
	                          i32 low, i32 high,
	                          i32 sampleRate) {
f32	tmp [filterSize];
f32	lo	= (f32) ((high - low) / 2) / sampleRate;
f32	shift	= (f32) ((high + low) / 2) / sampleRate;
f32	sum	= 0.0;


	this	-> sampleRate	= sampleRate;
	this	-> filterSize	= filterSize;
	this	-> kernel	= new cf32 [filterSize];
	this	-> buffer	= new cf32 [filterSize];
	this	-> ip		= 0;

	for (i32 i = 0; i < filterSize; i ++) {
	   kernel [i] = cf32 (0, 0);
	   buffer [i] = cf32 (0, 0);
	}

	for (i32 i = 0; i < filterSize; i ++) {
	   if (i == filterSize / 2)
	      tmp [i] = 2 * M_PI * lo;
	   else 
	      tmp [i] = sin (2 * M_PI * lo * (i - filterSize /2)) / (i - filterSize/2);
//
//	windowing
	   tmp [i]  *= (0.42 -
		    0.5 * cos (2 * M_PI * (f32)i / (f32)filterSize) +
		    0.08 * cos (4 * M_PI * (f32)i / (f32)filterSize));

	   sum += tmp [i];
	}

	for (i32 i = 0; i < filterSize; i ++) {	// shifting
	   f32 v = (i - filterSize / 2) * (2 * M_PI * shift);
	   kernel [i] = cf32 (tmp [i] * cos (v) / sum,
	                                          tmp [i] * sin (v) / sum);
	}
}

	BandPassFIR::~BandPassFIR () {
	delete[] kernel;
	delete[] buffer;
}


//	we process the samples backwards rather than reversing
//	the kernel
cf32 BandPassFIR::Pass (cf32 z) {
i16	i;
cf32	tmp	= 0;

	buffer [ip]	= z;
	for (i = 0; i < filterSize; i ++) {
	   i16 index = ip - i;
	   if (index < 0)
	      index += filterSize;
	   tmp		+= buffer [index] * kernel [i];
	}

	ip = (ip + 1) % filterSize;
	return tmp;
}

f32	BandPassFIR::Pass (f32 v) {
i16		i;
f32	tmp	= 0;

	buffer [ip] = cf32 (v, 0);
	for (i = 0; i < filterSize; i ++) {
	   i16 index = ip - i;
	   if (index < 0)
	      index += filterSize;
	   tmp += real (buffer [index]) * real (kernel [i]);
	}

	ip = (ip + 1) % filterSize;
	return tmp;
}

