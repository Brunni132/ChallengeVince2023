#include "DftProcessor.h"

const unsigned SCREEN_WIDTH = 640, SCREEN_HEIGHT = 360;
static auto MUSIC_FILENAME = "music-3.wav";
static const double MIN_DECIBELS = 60;
static const double TWENTY_OVER_LOG_10 = 20 / log(10);

SDL_Surface *currentSurface;

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

Uint32 HSV(float H, float S, float V) {
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

void setCurrentSurface(SDL_Surface* surface) {
	currentSurface = surface;
}

void setPixel(Uint32 x, Uint32 y, Uint32 pixel) {
	if (x >= unsigned(currentSurface->w) || y >= unsigned(currentSurface->h)) return;

	Uint32 *target_pixel = (Uint32*)((Uint8*)currentSurface->pixels + y * currentSurface->pitch + x * currentSurface->format->BytesPerPixel);
	*target_pixel = pixel;
}

int main(int argc, char* args[]) {
#define QUIT() { system("pause"); return -1; }

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		QUIT();
	}

	auto window = SDL_CreateWindow("Challenge Vince", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	if (!window) {
		fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
		QUIT();
	}

	auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (!renderer) {
		fprintf(stderr, "Failed to create renderer! SDL_Error: %s\n", SDL_GetError());
		QUIT();
	}

	SDL_AudioSpec wavSpec;
	uint32_t wavLength;
	uint8_t* wavBuffer;

	SDL_Init(SDL_INIT_AUDIO);
	auto audioSpec = SDL_LoadWAV(MUSIC_FILENAME, &wavSpec, &wavBuffer, &wavLength);
	if (!audioSpec) {
		fprintf(stderr, "Failed to load WAV file\n");
		QUIT();
	}

	if (wavSpec.channels != 2 ||  wavSpec.format != 0x8010) {
		fprintf(stderr, "Only 16-bit, stereo WAV files supported\n");
		QUIT();
	}

	SDL_AudioDeviceID deviceId = SDL_OpenAudioDevice(NULL, 0, &wavSpec, NULL, 0);
	if (SDL_QueueAudio(deviceId, wavBuffer, wavLength)) {
		fprintf(stderr, "Failed to queue audio\n");
		QUIT();
	}

	const uint64_t performanceCounterFreq = SDL_GetPerformanceFrequency();
	auto getTime = [=] {
		return double(SDL_GetPerformanceCounter()) / performanceCounterFreq;
	};
	bool quit = false, needsRerender = true;
	double lastProcessedTime;
	DftProcessor processor(128);
	DftProcessorForWav dftProcessor(processor, (int16_t*)wavBuffer, wavLength, wavSpec);
	const unsigned CHUNKS = 6;

	// Process a first sample
	dftProcessor.processDFT();
	printf("Target framerate: %f\n", 1.0 / (double(processor.inSamplesPerIteration * CHUNKS) / wavSpec.freq));
	lastProcessedTime = getTime();
	SDL_PauseAudioDevice(deviceId, 0);

	//double currentVolume = 0;
	SDL_SetRenderDrawColor(renderer, 136, 23, 152, 255);
	SDL_RenderClear(renderer);

	while (!quit && !dftProcessor.wouldOverflowWavFile()) {
		SDL_Event e;
		if (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) quit = true;
		}

		// Process frame
		if (needsRerender) {
			auto screenSurface = SDL_GetWindowSurface(window);
			setCurrentSurface(screenSurface);

			//for (unsigned i = 0; i < 256; i++) {
			//	float angle = i * 320.0f / 256;
			//	double dftValue = getDftPointInterpolated(dftOut, i / 256.0, 80, wavSpec.freq, true);
			//	double volume = convertPointToDecibels(dftValue, MIN_DECIBELS);
			//	unsigned vol = unsigned(volume * 256);
			//	for (unsigned j = 0; j < 256; j++) {
			//		uint32_t color = j > vol ? RGB(0, 0, 0) : HSV(angle, j * 140.0f / 256.f, 50 + j * 90.0f / 256.f);
			//		setPixel(j, i, color);
			//	}
			//}

			const unsigned BAR_HEIGHT = 4;
			unsigned y = 0;
			const vector<double>& dftOut(dftProcessor.lastProcessDFTResult());
			for (unsigned i = 0; i < dftOut.size(); i++) {
				float angle = i * 360.0f / dftOut.size();
				double fraction = double(i) / (dftOut.size() - 1);
				double sample = processor.getDftPointInterpolated(&dftOut[0], fraction, 50 Hz, wavSpec.freq / 2, false);
				double volume = processor.convertPointToDecibels(sample, 50 DB);
				unsigned vol = unsigned(volume * 256);
				for (unsigned j = 0; j < 256; j++) {
					uint32_t color = j > vol ? RGB(0, 0, 0) : HSV(angle, j * 140.0f / 256.f, 50 + j * 90.0f / 256.f);
					for (unsigned k = 0; k < BAR_HEIGHT; k++) {
						setPixel(j, y + k, color);
					}
				}
				y += BAR_HEIGHT;
			}

			SDL_UpdateWindowSurface(window);
			SDL_RenderPresent(renderer);
			needsRerender = false;
		}

		// Wait until we have played the whole DFT'ed sample
		double time = getTime();
		if ((time - lastProcessedTime) * wavSpec.freq >= processor.inSamplesPerIteration * CHUNKS) {
			lastProcessedTime += double(processor.inSamplesPerIteration * CHUNKS) / wavSpec.freq;
			dftProcessor.processDFTInChunksAndSmooth(CHUNKS, 0.2);
			needsRerender = true;
		}

		SDL_Delay(1);
	}

	SDL_DestroyWindow(window);
	SDL_CloseAudioDevice(deviceId);
	SDL_FreeWAV(wavBuffer);
	SDL_Quit();
#undef QUIT
	return 0;
}
