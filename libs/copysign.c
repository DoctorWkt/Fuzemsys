/* From MUSL */

#include <math.h>
#include "libm.h"

double copysign(double x, double y) {
	uint32_t hi;
	uint32_t his;
	GET_HIGH_WORD(hi, x);
	GET_HIGH_WORD(his, y);
	SET_HIGH_WORD(x, (hi & 0x7FFFFFFFUL) | (his & 0x80000000UL));
	return x;
}
