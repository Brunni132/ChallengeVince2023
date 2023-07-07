#pragma once
#include <SDL.h>
#include <memory.h>

static SDL_Surface* drawingSurface;
static SDL_Window* window;
static unsigned SCREEN_WIDTH = 240 * 3, SCREEN_HEIGHT = 160 * 3;

static inline unsigned operator"" _X(unsigned long long val) { return unsigned(val); }

static void createDrawingSurface(unsigned width, unsigned height, unsigned desiredScaling) {
	SCREEN_WIDTH = width * desiredScaling;
	SCREEN_HEIGHT = height * desiredScaling;
	SDL_SetWindowSize(window, SCREEN_WIDTH, SCREEN_HEIGHT);
	drawingSurface = SDL_CreateRGBSurface(0, width, height, 32, 0xff << 16, 0xff << 8, 0xff, 0xff << 24);
}

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

static void clearScreen(Uint32 colorWithAlpha) {
	unsigned size = unsigned(drawingSurface->pitch * drawingSurface->h) / 4;
	Uint32* ptr = (Uint32*)drawingSurface->pixels;
	while (size-- > 0) {
		*ptr++ = colorWithAlpha;
	}
}

static inline void setPixel(Uint32 x, Uint32 y, Uint32 pixel) {
	if (x >= unsigned(drawingSurface->w) || y >= unsigned(drawingSurface->h)) return;

	Uint32* target_pixel = (Uint32*)((Uint8*)drawingSurface->pixels + y * drawingSurface->pitch + x * drawingSurface->format->BytesPerPixel);
	*target_pixel = pixel;
}

static inline Uint32 getPixel(Uint32 x, Uint32 y, Uint32 colorToReturn = 0) {
	if (x >= unsigned(drawingSurface->w) || y >= unsigned(drawingSurface->h)) return colorToReturn;

	Uint32* target_pixel = (Uint32*)((Uint8*)drawingSurface->pixels + y * drawingSurface->pitch + x * drawingSurface->format->BytesPerPixel);
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
	for (unsigned y = 0; y < unsigned(drawingSurface->h); y++) {
		for (unsigned x = 0; x < unsigned(drawingSurface->w); x++) {
			Uint32 pixel = getPixel(x, y);
			setPixel(x, y, blendWrong(pixel));
		}
	}
}

static void moveScreen(int moveX, int moveY, Uint32 fillColor, Uint16 alpha = 16) {
	for (unsigned y = 0; y < unsigned(drawingSurface->h); y++) {
		for (unsigned x = 0; x < unsigned(drawingSurface->w); x++) {
			Uint32 pixel1 = getPixel(x, y);
			Uint32 pixel2 = getPixel(x - moveX, y - moveY, fillColor);
			setPixel(x, y, blend(blend(pixel1, pixel2, alpha), fillColor, 1));
		}
	}
}

