#
//
//	This LUT implementation of atan2 is a C++ translation of
//	a Java discussion on the net
//	http://www.java-gaming.org/index.php?topic=14647.0

#ifndef		__COMP_ATAN
#define		__COMP_ATAN

#include	<math.h>
#include	<cstdio>
#include	<stdint.h>
#include	<cstdlib>
#include	<limits>
#include	"dab-constants.h"
#
class	compAtan {
public:
		compAtan	(void);
		~compAtan	(void);
	f32	atan2		(f32, f32);
	f32	argX		(cf32);
private:
	f32	*ATAN2_TABLE_PPY;
	f32	*ATAN2_TABLE_PPX;
	f32	*ATAN2_TABLE_PNY;
	f32	*ATAN2_TABLE_PNX;
	f32	*ATAN2_TABLE_NPY;
	f32	*ATAN2_TABLE_NPX;
	f32	*ATAN2_TABLE_NNY;
	f32	*ATAN2_TABLE_NNX;
	f32	Stretch;
};

#endif
