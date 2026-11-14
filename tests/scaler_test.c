/* Compares the optimised scaler with a plain floating point bilinear
 * reference on random premultiplied-style data. Includes the module source
 * to reach its static function. */
#include "../src/input/shake_to_locate.c"
#include <stdio.h>

static double refpx(const uint8_t *src, size_t stride, int sw, int sh, int x, int y, int dw, int dh, int ch) {
	double fx = (x + 0.5) * ((double)sw / dw) - 0.5, fy = (y + 0.5) * ((double)sh / dh) - 0.5;
	if (fx < 0) fx = 0; if (fy < 0) fy = 0;
	int ix = (int)fx, iy = (int)fy; if (ix > sw - 1) ix = sw - 1; if (iy > sh - 1) iy = sh - 1;
	int ix1 = ix + 1 < sw ? ix + 1 : ix, iy1 = iy + 1 < sh ? iy + 1 : iy;
	double tx = fx - ix, ty = fy - iy;
	#define P(xx, yy) ((double)src[(size_t)(yy) * stride + (xx) * 4 + ch])
	return (P(ix, iy) * (1 - tx) + P(ix1, iy) * tx) * (1 - ty) + (P(ix, iy1) * (1 - tx) + P(ix1, iy1) * tx) * ty;
}

int main(void) {
	int sw = 37, sh = 29; size_t stride = (size_t)sw * 4 + 12;
	uint8_t *src = malloc(stride * sh); srand(7);
	for (size_t i = 0; i < stride * sh; i++) src[i] = (uint8_t)rand();
	int sizes[][2] = {{1,1},{5,3},{24,24},{37,29},{80,61},{300,200},{513,411}};
	double worst = 0;
	for (unsigned k = 0; k < sizeof(sizes)/sizeof(*sizes); k++) {
		int dw = sizes[k][0], dh = sizes[k][1];
		uint32_t *dst = malloc((size_t)dw * dh * 4);
		if (!bilinear_scale(src, stride, sw, sh, dst, dw, dh)) { printf("alloc fail\n"); return 1; }
		for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) for (int ch = 0; ch < 4; ch++) {
			double e = fabs(((dst[(size_t)y * dw + x] >> (ch * 8)) & 0xff) - refpx(src, stride, sw, sh, x, y, dw, dh, ch));
			if (e > worst) worst = e;
		}
		free(dst);
	}
	free(src);
	printf("worst per-channel error vs float reference: %.2f (limit 2.5)\n", worst);
	return worst > 2.5;
}
