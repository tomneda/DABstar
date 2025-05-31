#
//
//	This LUT implementation of atan2 is a C++ translation of
//	a Java discussion on the net
//	http://www.java-gaming.org/index.php?topic=14647.0

#include	"Xtan2.h"

#define	SIZE		8192
#define	EZIS		(-SIZE)

	compAtan::compAtan (void) {

	Stretch		= M_PI;
//	private static final i32           SIZE                 = 1024;
//	private static final f32         Stretch            = (f32)Math.PI;
// Output will swing from -Stretch to Stretch (default: Math.PI)
// Useful to change to 1 if you would normally do "atan2(y, x) / Math.PI"

	ATAN2_TABLE_PPY    = new f32 [SIZE + 1];
	ATAN2_TABLE_PPX    = new f32 [SIZE + 1];
	ATAN2_TABLE_PNY    = new f32 [SIZE + 1];
	ATAN2_TABLE_PNX    = new f32 [SIZE + 1];
	ATAN2_TABLE_NPY    = new f32 [SIZE + 1];
	ATAN2_TABLE_NPX    = new f32 [SIZE + 1];
	ATAN2_TABLE_NNY    = new f32 [SIZE + 1];
	ATAN2_TABLE_NNX    = new f32 [SIZE + 1];
        for (i32 i = 0; i <= SIZE; i++) {
            f32 f = (f32)i / SIZE;
            ATAN2_TABLE_PPY [i] = atan(f) * Stretch / M_PI;
            ATAN2_TABLE_PPX [i] = Stretch * 0.5f - ATAN2_TABLE_PPY[i];
            ATAN2_TABLE_PNY [i] = -ATAN2_TABLE_PPY [i];
            ATAN2_TABLE_PNX [i] = ATAN2_TABLE_PPY [i] - Stretch * 0.5f;
            ATAN2_TABLE_NPY [i] = Stretch - ATAN2_TABLE_PPY [i];
            ATAN2_TABLE_NPX [i] = ATAN2_TABLE_PPY [i] + Stretch * 0.5f;
            ATAN2_TABLE_NNY [i] = ATAN2_TABLE_PPY [i] - Stretch;
            ATAN2_TABLE_NNX [i] = -Stretch * 0.5f - ATAN2_TABLE_PPY [i];
        }
}

	compAtan::~compAtan	(void) {
	delete	ATAN2_TABLE_PPY;
	delete	ATAN2_TABLE_PPX;
	delete	ATAN2_TABLE_PNY;
	delete	ATAN2_TABLE_PNX;
	delete	ATAN2_TABLE_NPY;
	delete	ATAN2_TABLE_NPX;
	delete	ATAN2_TABLE_NNY;
	delete	ATAN2_TABLE_NNX;
}

/**
  * ATAN2 : performance degrades due to the many "0" tests
  */

f32	compAtan::atan2 (f32 y, f32 x) {
	if (x == 0) {
	   if (y == 0)  return 0;
//	      return std::numeric_limits<f32>::infinity ();
	   else
	   if (y > 0)
	      return  M_PI / 2;
	   else		// y < 0
	      return  - M_PI / 2;
	}

	if (x > 0) {
	   if (y >= 0) {
	      if (x >= y) 
	         return ATAN2_TABLE_PPY[(i32)(SIZE * y / x + 0.5)];
	      else
	         return ATAN2_TABLE_PPX[(i32)(SIZE * x / y + 0.5)];
	      
	   }
	   else {
	      if (x >= -y) 
	         return ATAN2_TABLE_PNY[(i32)(EZIS * y / x + 0.5)];
	      else
	         return ATAN2_TABLE_PNX[(i32)(EZIS * x / y + 0.5)];
	   }
        }
	else {
	   if (y >= 0) {
	      if (-x >= y) 
	         return ATAN2_TABLE_NPY[(i32)(EZIS * y / x + 0.5)];
	      else
	         return ATAN2_TABLE_NPX[(i32)(EZIS * x / y + 0.5)];
	   }
	   else {
	      if (x <= y) // (-x >= -y)
	         return ATAN2_TABLE_NNY[(i32)(SIZE * y / x + 0.5)];
	      else
	         return ATAN2_TABLE_NNX[(i32)(SIZE * x / y + 0.5)];
	   }
	}
}

f32	compAtan::argX	(cf32 v) {
	return this -> atan2 (imag (v), real (v));
}
