#include <math.h>
#include <stdlib.h>
#include <emscripten.h>

typedef unsigned char byte;
typedef struct v3 {
	float x, y, z;
} v3;
v3 add(v3 a, v3 b) {
	return (v3) { .x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z };
}
v3 mul(v3 a, float k) {
	return (v3) { .x = a.x * k, .y = a.y * k, .z = a.z * k };
}
v3 a2v(float *a) {
	return (v3) { .x = a[0], .y = a[1], .z = a[2] };
}
EMSCRIPTEN_KEEPALIVE
byte *dewarp(
		byte *src, int srcWidth, int srcHeight,
		int dstWidth, int dstHeight,
		float *cpX, float *cpY,
		float *upperLeft, float *baseVec, float *normalVec, 
		float *vertical, float d
		) {
	v3 ul = a2v(upperLeft);
	v3 bv = a2v(baseVec);
	v3 nv = a2v(normalVec);
	v3 vt = a2v(vertical);
	byte *dst = malloc(4 * dstWidth * dstHeight);
	for (int x = 0; x < dstWidth; x++) {
		v3 top = add(ul, add(mul(bv, cpX[x]), mul(nv, cpY[x])));
		for (int y = 0; y < dstHeight; y++) {
			v3 c = add(top, mul(vt, (float) y / dstHeight));
			float sx = c.x / (1 + c.z / d);	
			float sy = c.y / (1 + c.z / d);	
			sx = (sx + 0.5f) * srcWidth;
			sy = sy * srcWidth + 0.5f * srcHeight;
			int di = (y * dstWidth + x) * 4;
			int si = ((int)round(sy) * srcWidth + (int)round(sx)) * 4;
			dst[di] = src[si];
			dst[di + 1] = src[si + 1];
			dst[di + 2] = src[si + 2];
			dst[di + 3] = src[si + 3];
		}
	}
	return dst;
}
