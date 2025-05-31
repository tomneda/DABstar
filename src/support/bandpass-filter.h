
#ifndef	__BANDPASSFIR__
#define	__BANDPASSFIR__
/*
 *	The bandfilter is for the complex domain. 
 *	We create a lowpass filter, which stretches over the
 *	positive and negative half, then shift this filter
 *	to the right position to form a nice bandfilter.
 *	For the real domain, we use the Simple BandPass version.
 */

#include	"dab-constants.h"

class	BandPassFIR {
public:
		BandPassFIR	(i16 filterSize,
	                          i32 low, i32 high,
	                          i32 sampleRate);
		~BandPassFIR	();
cf32	Pass	(cf32);
f32		Pass		(f32);
private:

	i32	filterSize;
	i32	sampleRate;
	i32	ip;
	cf32 *kernel;
	cf32 *buffer;
};
#endif
