
#include	<math.h>

/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
/*::  This function converts decimal degrees to radians             :*/
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
f64 deg2rad (f64 deg) {
  return (deg * pi / 180);
}

f64 distance (f64 lat1, f64 lon1, f64 lat2, f64 lon2) {
f64 theta, dist;
theta = lon1 - lon2;

	dist = sin (deg2rad (lat1)) * sin (deg2rad (lat2)) +
	       cos (deg2rad (lat1)) * cos (deg2rad (lat2)) *
	                         cos (deg2rad (theta));
	dist = acos (dist);
	dist = rad2deg (dist);
	dist = dist * 60 * 1.1515;
	dist = dist * 1.609344;
	return (dist);
}

