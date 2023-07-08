#pragma once
#include <SDL.h>
#include <memory.h>
#include <functional>

typedef float DrawingSurfacePrecision;

struct Color {
	DrawingSurfacePrecision r, g, b;

	Color() {}

	Color(DrawingSurfacePrecision r, DrawingSurfacePrecision g, DrawingSurfacePrecision b) : r(r), g(g), b(b) {}

	Color(Uint32 sdlColor) : r(DrawingSurfacePrecision(sdlColor >> 16 & 0xff)), g(DrawingSurfacePrecision(sdlColor >> 8 & 0xff)), b(DrawingSurfacePrecision(sdlColor & 0xff)) {}

	Color add(Color other) {
		return Color(r + other.r, g + other.g, b + other.b);
	}
	Color subtract(Color other) {
		return Color(r - other.r, g - other.g, b - other.b);
	}
};

struct DrawingSurface {
	SDL_Surface* sdlSurface;
	DrawingSurfacePrecision* pixels;
	unsigned w, h, pitch;
	bool protectOverflow;

	DrawingSurface(SDL_Surface * surface) : w(surface->w), h(surface->h), pitch(surface->pitch), sdlSurface(surface) {
		if (pitch < w * 4) throw "Error with pixel format, make sure that you use 32 bits";
		pixels = new DrawingSurfacePrecision[h * pitch];
		protectOverflow = false;
	}

	~DrawingSurface() { delete[] pixels; }

	void clearScreen(Color color) {
		unsigned size = pitch * h / 4;
		DrawingSurfacePrecision* ptr = pixels;
		while (size-- > 0) {
			*ptr++ = color.r;
			*ptr++ = color.g;
			*ptr++ = color.b;
			ptr++;
		}
	}

	void setPixel(unsigned x, unsigned y, Color color) {
		if (x >= w || y >= h) return;

		DrawingSurfacePrecision* ptr = pixels + y * pitch + x * 4;
		*ptr++ = color.r;
		*ptr++ = color.g;
		*ptr++ = color.b;
	}

	Color getPixel(unsigned x, unsigned y, Color defaultColor = Color()) {
		if (x >= w || y >= h) return defaultColor;

		DrawingSurfacePrecision* ptr = pixels + y * pitch + x * 4;
		return Color(ptr[0], ptr[1], ptr[2]);
	}

	void blitToSdlSurface() {
		DrawingSurfacePrecision* srcPtr = pixels;
		Uint32* dstPtr = (Uint32*)sdlSurface->pixels;
		void* dstPtrEnd = (Uint8*)dstPtr + pitch * h;

		if (protectOverflow) {
			while ((void*)dstPtr < dstPtrEnd) {
				int r = int(srcPtr[0]), g = int(srcPtr[1]), b = int(srcPtr[2]);
				if (r < 0) r = 0; if (r > 255) r = 255;
				if (g < 0) g = 0; if (g > 255) g = 255;
				if (b < 0) b = 0; if (b > 255) b = 255;
				*dstPtr++ = 0xff << 24 | Uint8(r) << 16 | Uint8(g) << 8 | Uint8(b);
				srcPtr += 4;
			}
		}
		else {
			while ((void*)dstPtr < dstPtrEnd) {
				*dstPtr++ = 0xff << 24 | (Uint8)(srcPtr[0]) << 16 | (Uint8)(srcPtr[1]) << 8 | (Uint8)(srcPtr[2]);
				srcPtr += 4;
			}
		}
	}
};

static const bool isSDLSurface;

static DrawingSurface* drawingSurface;
static SDL_Surface* sdlDrawingSurface;
static SDL_Window* window;
static unsigned SCREEN_WIDTH = 240 * 3, SCREEN_HEIGHT = 160 * 3;

static inline unsigned operator"" _X(unsigned long long val) { return unsigned(val); }

static DrawingSurface& createDrawingSurface(unsigned width, unsigned height, unsigned desiredScaling) {
	SCREEN_WIDTH = width * desiredScaling;
	SCREEN_HEIGHT = height * desiredScaling;
	SDL_SetWindowSize(window, SCREEN_WIDTH, SCREEN_HEIGHT);
	sdlDrawingSurface = SDL_CreateRGBSurface(0, width, height, 32, 0xff << 16, 0xff << 8, 0xff, 0xff << 24);
	drawingSurface = new DrawingSurface(sdlDrawingSurface);
	return *drawingSurface;
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

static inline Color blend(Color pixel1, Color pixel2, DrawingSurfacePrecision alphaPixel2) {
	alphaPixel2 /= 256.0;
	DrawingSurfacePrecision alphaPixel1 = 1 - alphaPixel2;
	return Color(pixel2.r * alphaPixel2 + pixel1.r * alphaPixel1, pixel2.g * alphaPixel2 + pixel1.g * alphaPixel1, pixel2.b * alphaPixel2 + pixel1.b * alphaPixel1);
}

struct ScreenMover {
	double positionX = 0, positionY = 0;

	void addMove(double deltaX, double deltaY) {
		positionX += deltaX, positionY += deltaY;
	}

	void performMove(Color fillColor, DrawingSurfacePrecision alpha = 16) {
		int moveX = int(fmax(-1, fmin(+1, positionX))), moveY = int(fmax(-1, fmin(+1, positionY)));
		positionX -= moveX, positionY -= moveY;
		if (moveX || moveY) {
			for (unsigned y = 0; y < unsigned(drawingSurface->h); y++) {
				for (unsigned x = 0; x < unsigned(drawingSurface->w); x++) {
					Color pixel1 = drawingSurface->getPixel(x, y);
					Color pixel2 = drawingSurface->getPixel(x - moveX, y - moveY, fillColor);
					drawingSurface->setPixel(x, y, blend(pixel1, pixel2, alpha));
				}
			}
		}
	}
};
