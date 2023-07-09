#pragma once
#include <SDL.h>
#include <memory.h>
#include <functional>

struct Color {
	float components[3];

	Color() {}

	Color(float r, float g, float b) : components { r, g, b } {}

	Color(Uint32 sdlColor);

	inline float& r() { return components[0]; }
	inline float& g() { return components[1]; }
	inline float& b() { return components[2]; }
	inline float& h() { return components[0]; }
	inline float& s() { return components[1]; }
	inline float& l() { return components[2]; }

	Color add(Color other) {
		return Color(components[0] + other.components[0], components[1] + other.components[1], components[2] + other.components[2]);
	}
	Color subtract(Color other) {
		return Color(components[0] - other.components[0], components[1] - other.components[1], components[2] - other.components[2]);
	}
	Color blend(Color pixel2, float alphaPixel2 = 128) {
		alphaPixel2 /= 256.0;
		float alphaPixel1 = 1 - alphaPixel2;
		return Color(pixel2.components[0] * alphaPixel2 + components[0] * alphaPixel1, pixel2.components[1] * alphaPixel2 + components[1] * alphaPixel1, pixel2.components[2] * alphaPixel2 + components[2] * alphaPixel1);
	}
};

struct DrawingSurface {
	SDL_Surface* sdlSurface;
	float* pixels;
	unsigned w, h, pitch;
	bool protectOverflow;
	bool useHsl; // false = rgb, true = hsl

	DrawingSurface(const DrawingSurface&) = delete; // disallowed

	DrawingSurface(SDL_Surface * surface) : w(surface->w), h(surface->h), pitch(surface->pitch), sdlSurface(surface) {
		if (pitch < w * 4) throw "Error with pixel format, make sure that you use 32 bits";
		pixels = new float[h * pitch];
		protectOverflow = false;
		useHsl = false;
	}

	~DrawingSurface() {
		delete[] pixels;
	}

	void clearScreen(Color color) {
		unsigned size = pitch * h / 4;
		float* ptr = pixels;
		while (size-- > 0) {
			*ptr++ = color.components[0];
			*ptr++ = color.components[1];
			*ptr++ = color.components[2];
			ptr++;
		}
	}

	void setPixel(unsigned x, unsigned y, Color color) {
		if (x >= w || y >= h) return;

		float* ptr = pixels + y * pitch + x * 4;
		*ptr++ = color.components[0];
		*ptr++ = color.components[1];
		*ptr++ = color.components[2];
	}

	Color getPixel(unsigned x, unsigned y, Color defaultColor = Color()) {
		if (x >= w || y >= h) return defaultColor;

		float* ptr = pixels + y * pitch + x * 4;
		return Color(ptr[0], ptr[1], ptr[2]);
	}

	void fillRect(unsigned x, unsigned y, unsigned w, unsigned h, Color c) {
		for (unsigned i = 0; i < w; i++)
			for (unsigned j = 0; j < h; j++)
				setPixel(x + i, y + j, c);
	}

	void blitToSdlSurface() {
		float* srcPtr = pixels;
		Uint32* dstPtr = (Uint32*)sdlSurface->pixels;
		void* dstPtrEnd = (Uint8*)dstPtr + pitch * h;

		if (useHsl) {
			while ((void*)dstPtr < dstPtrEnd) {
				//float h = fmodf(srcPtr[0], 1);
				//float l = fminf(1, fmaxf(0, srcPtr[2]));
				//float s = fminf(1, fmaxf(0, srcPtr[1]));
				float h = protectOverflow ? fmodf(srcPtr[0], 1) : srcPtr[0];
				float s = protectOverflow ? clamp(srcPtr[1]) : srcPtr[1];
				float l = protectOverflow ? clamp(computeSaturatedL(srcPtr[1], srcPtr[2])) : srcPtr[2];

				float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
				float p = 2 * l - q;
				*dstPtr++ = 0xff << 24 |
					Uint8(hue2rgb(p, q, h + float(1. / 3)) * 255) << 16 |
					Uint8(hue2rgb(p, q, h) * 255) << 8 |
					Uint8(hue2rgb(p, q, h - float(1. / 3)) * 255);
				srcPtr += 4;
			}
		}
		else if (protectOverflow) {
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

	DrawingSurface* clone() {
		DrawingSurface* dest = new DrawingSurface(sdlSurface);
		memcpy(dest->pixels, pixels, h * pitch * sizeof(float));
		return dest;
	}

private:
	static float hue2rgb(float p, float q, float t) {
		if (t < 0)
			t += 1;
		if (t > 1)
			t -= 1;
		if (t < float(1. / 6))
			return p + (q - p) * 6 * t;
		if (t < float(1. / 2))
			return q;
		if (t < float(2. / 3))
			return p + (q - p) * (float(2. / 3) - t) * 6;
		return p;
	}

	static inline float computeSaturatedL(float s, float l) {
		if (s > 1) {
			if (l < 0.5f) {
				l *= s;
				if (l >= 0.5f) l = 0.5f;
			}
			else {
				l = 1 - ((1 - l) * s);
				if (l <= 0.5f) l = 0.5f;
			}
		}
		return l;
	}

	static inline float clamp(float v) {
		if (v < 0) return 0;
		if (v > 1) return 1;
		return v;
	}
};

static const bool isSDLSurface;

extern SDL_Surface* g_sdlSurface;
extern DrawingSurface* g_drawingSurface;
extern SDL_Window* window;
static unsigned SCREEN_WIDTH = 240 * 3, SCREEN_HEIGHT = 160 * 3;

static inline unsigned operator"" _X(unsigned long long val) { return unsigned(val); }

static DrawingSurface& createDrawingSurface(unsigned width, unsigned height, unsigned desiredScaling) {
	SCREEN_WIDTH = width * desiredScaling;
	SCREEN_HEIGHT = height * desiredScaling;
	SDL_SetWindowSize(window, SCREEN_WIDTH, SCREEN_HEIGHT);
	if (g_sdlSurface) SDL_FreeSurface(g_sdlSurface);
	if (g_drawingSurface) delete g_drawingSurface;
	g_sdlSurface = SDL_CreateRGBSurface(0, width, height, 32, 0xff << 16, 0xff << 8, 0xff, 0xff << 24);
	g_drawingSurface = new DrawingSurface(g_sdlSurface);
	return *g_drawingSurface;
}

template<typename T>
static T clamp(T value, T min, T max) {
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

// Can only be used if the DrawingSurface::useHsl is set to false
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
	H = clamp(H, 0.f, 360.f);
	S = clamp(S, 0.f, 100.f);
	V = clamp(V, 0.f, 100.f);
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

struct ScreenMover {
	double positionX = 0, positionY = 0;

	void stashMove(double deltaX, double deltaY) {
		positionX += deltaX, positionY += deltaY;
	}

	void performMove(DrawingSurface& ds, Color fillColor, float alpha = 16) {
		int moveX = int(clamp(positionX, -1.0, +1.0)), moveY = int(clamp(positionY, -1.0, +1.0));
		positionX -= moveX, positionY -= moveY;
		if (moveX || moveY) {
			for (unsigned y = 0; y < unsigned(ds.h); y++) {
				for (unsigned x = 0; x < unsigned(ds.w); x++) {
					Color pixel1 = ds.getPixel(x, y);
					Color pixel2 = ds.getPixel(x - moveX, y - moveY, fillColor);
					ds.setPixel(x, y, pixel1.blend(pixel2, alpha));
				}
			}
		}
	}

	void performMoveInHSLMode(DrawingSurface& ds, Color fillColor, float alpha = 16) {
		int moveX = int(clamp(positionX, -1.0, +1.0)), moveY = int(clamp(positionY, -1.0, +1.0));
		positionX -= moveX, positionY -= moveY;
		if (moveX || moveY) {
			for (unsigned y = 0; y < unsigned(ds.h); y++) {
				for (unsigned x = 0; x < unsigned(ds.w); x++) {
					Color pixel1 = ds.getPixel(x, y);
					Color pixel2 = ds.getPixel(x - moveX, y - moveY, fillColor);
					pixel2.components[0] = pixel1.components[0];
					pixel2.components[1] = pixel1.components[1];
					ds.setPixel(x, y, pixel1.blend(pixel2, alpha).blend(Color(0, 0, 0), 2));
				}
			}
		}
	}
};

struct ScreenStretcher {
	double move = 0;

	void stashMove(double delta) {
		move += delta;
	}

	void performStretch(DrawingSurface& ds, Color fillColor, float alpha = 16) {
		int moveInt = int(clamp(move, -1.0, +1.0));
		move -= moveInt;
		if (moveInt) {
			unsigned w(ds.w), h(ds.h);
			for (unsigned y = 0; y < h; y++) {
				for (unsigned x = 0; x < w; x++) {
					Color pixel1 = ds.getPixel(x, y);
					unsigned nextPixX = x < w / 2 ? (x + 1) : (x - 1);
					unsigned nextPixY = y < h / 2 ? (y + 1) : (y - 1);
					Color pixel2 = ds.getPixel(nextPixX, nextPixY, fillColor);
					//if (x == w / 2 || y == h / 2) pixel2 = pixel2.blend(fillColor, 60);
					pixel2 = pixel2.blend(fillColor, 16);
					//pixel2 = pixel2.subtract(Color(4, 4, 4));
					ds.setPixel(x, y, pixel1.blend(pixel2, alpha));
				}
			}
		}
	}

	void performCircular(DrawingSurface& ds, Color fillColor, float alpha = 16, bool expandOrContract = false) {
		int moveInt = int(clamp(move, -1.0, +1.0));
		move -= moveInt;
		if (moveInt) {
			auto copy = ds.clone();
			unsigned w(ds.w), h(ds.h);
			for (unsigned y = 0; y < h; y++) {
				for (unsigned x = 0; x < w; x++) {
					float xFromCenter = float(x) - w / 2, yFromCenter = h / 2 - float(y);
					float angle = atan2f(yFromCenter, xFromCenter);
					Color pixel1 = ds.getPixel(x, y);
					unsigned nextPixX = expandOrContract ? roundf(x - 1 * cos(angle)) : roundf(x + 1 * cos(angle));
					unsigned nextPixY = expandOrContract ? roundf(y + 1 * sin(angle)) : roundf(y - 1 * sin(angle));
					Color pixel2 = ds.getPixel(nextPixX, nextPixY, fillColor);
					//if (x == w / 2 || y == h / 2) pixel2 = pixel2.blend(fillColor, 60);
					pixel2 = pixel2.blend(fillColor, 16);
					//pixel2 = pixel2.subtract(Color(4, 4, 4));
					ds.setPixel(x, y, pixel1.blend(pixel2, alpha));
				}
			}
			delete copy;
		}
	}
};
