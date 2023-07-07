#pragma once
#include <SDL.h>
#include <memory.h>

static SDL_Surface* currentSurface;

static Uint32 RGBA(int r, int g, int b, int a) {
	if (r < 0) r = 0;
	else if (r > 255) r = 255;
	if (g < 0) g = 0;
	else if (g > 255) g = 255;
	if (b < 0) b = 0;
	else if (b > 255) b = 255;
	if (a < 0) a = 0;
	else if (a > 255) a = 255;
	return (Uint8)a << 24 | (Uint8)r << 16 | (Uint8)g << 8 | (Uint8)b;
}

static Uint32 RGB(int r, int g, int b) { return RGBA(r, g, b, 0xff); }

static Uint32 HSV(float H, float S, float V) {
	H = fmaxf(0, fminf(360, H));
	S = fmaxf(0, fminf(100, S));
	V = fmaxf(0, fminf(100, V));
	float s = S / 100;
	float v = V / 100;
	float C = s * v;
	float X = C * (1 - fabsf(fmodf(H / 60.f, 2) - 1));
	float m = v - C;
	float r, g, b;
	if (H >= 0 && H < 60) r = C, g = X, b = 0;
	else if (H >= 60 && H < 120) r = X, g = C, b = 0;
	else if (H >= 120 && H < 180) r = 0, g = C, b = X;
	else if (H >= 180 && H < 240) r = 0, g = X, b = C;
	else if (H >= 240 && H < 300) r = X, g = 0, b = C;
	else r = C, g = 0, b = X;
	return RGB(int((r + m) * 255), int((g + m) * 255), int((b + m) * 255));
}

static void setCurrentSurface(SDL_Surface* surface) {
	currentSurface = surface;
}

static void clearScreen(Uint32 colorWithAlpha) {
	memset(currentSurface->pixels, colorWithAlpha, currentSurface->pitch * currentSurface->h);
}

static inline void setPixel(Uint32 x, Uint32 y, Uint32 pixel) {
	if (x >= unsigned(currentSurface->w) || y >= unsigned(currentSurface->h)) return;

	Uint32* target_pixel = (Uint32*)((Uint8*)currentSurface->pixels + y * currentSurface->pitch + x * currentSurface->format->BytesPerPixel);
	*target_pixel = pixel;
}

static inline Uint32 getPixel(Uint32 x, Uint32 y) {
	if (x >= unsigned(currentSurface->w) || y >= unsigned(currentSurface->h)) return 0;

	Uint32* target_pixel = (Uint32*)((Uint8*)currentSurface->pixels + y * currentSurface->pitch + x * currentSurface->format->BytesPerPixel);
	return *target_pixel;
}

static inline Uint32 blend(Uint32 pixel1, Uint32 pixel2, Uint16 alphaPixel2) {
	Uint16 alphaPixel1 = 256 - alphaPixel2;
	return
		0xff << 24 |
		((pixel2 & 0xff) * alphaPixel2 + (pixel1 & 0xff) * alphaPixel1) >> 8 |
		((pixel2 >> 8 & 0xff) * alphaPixel2 + (pixel1 >> 8 & 0xff) * alphaPixel1) & 0xff00 |
		(((pixel2 >> 16 & 0xff) * alphaPixel2 + (pixel1 >> 16 & 0xff) * alphaPixel1) & 0xff00) << 8;
}

static inline Uint32 blendWrong(Uint32 pixel1, Uint32 pixel2 = RGB(48, 48, 48), Uint16 alphaPixel2 = 16) {
	Uint16 alphaPixel1 = 256 - alphaPixel2;
	return
		0xff << 24 |
		Uint32(pixel2 * alphaPixel2 + pixel1 * alphaPixel1) >> 8;
}

static void dimScreenWeirdly() {
	for (unsigned y = 0; y < currentSurface->h; y++) {
		for (unsigned x = 0; x < currentSurface->w; x++) {
			Uint32 pixel = getPixel(x, y);
			setPixel(x, y, blendWrong(pixel));
		}
	}
}

static void moveScreen(int moveX, int moveY, Uint16 alpha = 16) {
	for (unsigned y = 0; y < currentSurface->h; y++) {
		for (unsigned x = 0; x < currentSurface->w; x++) {
			Uint32 pixel1 = getPixel(x, y);
			Uint32 pixel2 = getPixel(x - moveX, y - moveY);
			setPixel(x, y, blend(pixel1, pixel2, alpha));
		}
	}
}

