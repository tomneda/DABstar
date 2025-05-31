#
//
//	For the different formats for input, we have
//	different readers, with one "mother" reader.
//	Note that the cardreader is quite different here
//	and its code is elsewhere
#ifndef	__COMMON_READERS
#define	__COMMON_READERS

#include	"virtual-reader.h"

class	reader_16: public virtualReader {
public:
	reader_16	(RingBuffer<cf32> *p, i32, i32);
	~reader_16	(void);
void	processData	(f32 IQoffs, void *data, i32 cnt);
i16 bitDepth	(void);
};

class	reader_24: public virtualReader {
public:
	reader_24	(RingBuffer<cf32> *p, i32, i32);
	~reader_24	(void);
void	processData	(f32 IQoffs, void *data, i32 cnt);
i16 bitDepth	(void);
};

class	reader_32: public virtualReader {
public:
	reader_32	(RingBuffer<cf32> *p, i32, i32);
	~reader_32	(void);
void	processData	(f32 IQoffs, void *data, i32 cnt);
i16	bitDepth	(void);
};

//
//	This is the only one we actually need for
//	elad s2 as input device for DAB
class	reader_float: public virtualReader {
public:
	reader_float	(RingBuffer<cf32> *p, i32);
	~reader_float	(void);
void	processData	(f32 IQoffs, void *data, i32 cnt);
i16	bitDepth	(void);
private:
	i16		mapTable_int	[2048];
	f32		mapTable_float	[2048];
	cf32	convBuffer	[3072 + 1];
	i16		convIndex;
};

#endif

