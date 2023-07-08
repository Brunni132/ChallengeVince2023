#include "DrawingFloat.h"

DrawingSurface* drawingSurface;
SDL_Surface* sdlDrawingSurface;
SDL_Window* window;

Color::Color(Uint32 sdlColor) : components{ float(sdlColor >> 16 & 0xff), float(sdlColor >> 8 & 0xff), float(sdlColor & 0xff) } {
	if (drawingSurface->useHsl) throw "Unsupported operation (conversion would be needed, but performance-wise it's probably shit)";
}
