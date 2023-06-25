#include <SDL.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>

const unsigned SCREEN_WIDTH = 640, SCREEN_HEIGHT = 360;
static auto MUSIC_FILENAME = "music.wav";
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

void setCurrentSurface(SDL_Surface* surface) {
	currentSurface = surface;
}

void setPixel(Uint32 x, Uint32 y, Uint32 pixel) {
	if (x >= unsigned(currentSurface->w) || y >= unsigned(currentSurface->h)) return;

	Uint32 *target_pixel = (Uint32*)((Uint8*)currentSurface->pixels + y * currentSurface->pitch + x * currentSurface->format->BytesPerPixel);
	*target_pixel = pixel;
}

static const unsigned IN_SAMPLES_PER_ITERATION = 512;
static const unsigned OUT_SAMPLES_PER_ITERATION = IN_SAMPLES_PER_ITERATION / 2 + 1;
static const double TWENTY_OVER_LOG_10 = 20 / log(10);

// [-infinite, 0]
static inline double toDecibels(double sample) {
	//return 20 * log(sample) / log(10);
	return log(sample) * TWENTY_OVER_LOG_10;
}

// inData is stereo, 16-bit data (L R L R, etc.)
void processFFT(const int16_t* inData, double *outData) {
	// Zero REX & IMX so they can be used as accumulators
	double REX[OUT_SAMPLES_PER_ITERATION], IMX[OUT_SAMPLES_PER_ITERATION];
	for (unsigned k = 0; k < OUT_SAMPLES_PER_ITERATION; k++) {
		REX[k] = IMX[k] = 0;
	}

	double samples[IN_SAMPLES_PER_ITERATION];
	for (unsigned k = 0; k < IN_SAMPLES_PER_ITERATION; k++) {
		samples[k] = double(*inData++) / (32768 * 2) + double(*inData++) / (32768 * 2);
	}

	// Correlate input with the cosine and sine wave
	for (unsigned k = 0; k < OUT_SAMPLES_PER_ITERATION; k++) {
		for (unsigned i = 0; i < IN_SAMPLES_PER_ITERATION; i++) {
			double sample = samples[i];
			REX[k] += sample * cos(2 * M_PI * k * i / IN_SAMPLES_PER_ITERATION);
			IMX[k] += sample * sin(2 * M_PI * k * i / IN_SAMPLES_PER_ITERATION);
		}

		// Module du nombre complexe (distance � l'origine)
		// = pour chaque fr�quence l'amplitude correspondante
		outData[k] = sqrt(REX[k] * REX[k] + IMX[k] * IMX[k]);
	}

	// Calculer en DB comme pour le volume
	// Bande de fr�quence -> �nergie qu'il y a dedans
	for (unsigned k = 0; k < OUT_SAMPLES_PER_ITERATION; k++) {
		outData[k] = toDecibels(outData[k]);
	}
}

double processVolume(const int16_t* inData) {
	const unsigned samples = IN_SAMPLES_PER_ITERATION;
	double sum = 0;

	for (unsigned k = 0; k < samples; k++) {
		double sample = double(inData[0]) / (32768 * 2) + double(inData[1]) / (32768 * 2);
		sum += sample * sample;
	}

	// TODO: demander https://dspillustrations.com/pages/posts/misc/decibel-conversion-factor-10-or-factor-20.html
	double linearVolume = sqrt(sum / samples);
	return toDecibels(linearVolume);
}

int main(int argc, char* args[]) {
#define QUIT() { system("pause"); return -1; }

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		QUIT();
	}

	auto window = SDL_CreateWindow("SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
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
	double fftOut[OUT_SAMPLES_PER_ITERATION];
	int16_t* const wavBufferForDFT = (int16_t*)wavBuffer;
	unsigned waveBufferOffset = 0;
	unsigned waveTotalSamples = wavLength / sizeof(wavBufferForDFT[0]) / wavSpec.channels;
	auto wouldOverflowWavFile = [&] {
		return waveBufferOffset + IN_SAMPLES_PER_ITERATION > waveTotalSamples;
	};
	//auto getSample = [&](unsigned index) {
	//	return double(wavBufferForDFT[waveBufferOffset + index]) / (32768 * 2) + double(wavBufferForDFT[waveBufferOffset + index + 1]) / (32768 * 2);
	//};

	processFFT(wavBufferForDFT + waveBufferOffset * wavSpec.channels, fftOut);
	waveBufferOffset += IN_SAMPLES_PER_ITERATION;
	lastProcessedTime = getTime();
	SDL_PauseAudioDevice(deviceId, 0);

	double currentVolume = 0;
	SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
	SDL_RenderClear(renderer);

	while (!quit && !wouldOverflowWavFile()) {
		SDL_Event e;
		if (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) quit = true;
		}

		// Process frame
		if (needsRerender) {
			auto screenSurface = SDL_GetWindowSurface(window);
			setCurrentSurface(screenSurface);

			//double volume = 1 - (fmin(50, -currentVolume) / 50);
			//unsigned vol = unsigned(volume * 256);
			//for (unsigned i = 0; i < 256; i++) {
			//	for (unsigned j = 0; j < 256; j++) {
			//		setPixel(j, i, j > vol ? RGB(0, 0, 0) : RGB(255, 255, 0));
			//	}
			//}

			for (unsigned i = 0; i < 256; i++) {
				double volume = 1 - (fmin(50, -fftOut[i]) / 50);
				unsigned vol = unsigned(volume * 256);
				for (unsigned j = 0; j < 256; j++) {
					setPixel(j, i, j > vol ? RGB(0, 0, 0) : RGB(255, 255, 0));
				}
			}

			SDL_UpdateWindowSurface(window);
			SDL_RenderPresent(renderer);
			needsRerender = false;
		}

		// Wait until we have played the whole DFT'ed sample
		double time = getTime();
		if ((time - lastProcessedTime) * wavSpec.freq >= IN_SAMPLES_PER_ITERATION) {
			lastProcessedTime += double(IN_SAMPLES_PER_ITERATION) / wavSpec.freq;
			//printf("Position: %f\n", double(waveBufferOffset) / waveTotalSamples);
			//currentVolume = processVolume(wavBufferForDFT + waveBufferOffset * wavSpec.channels);
			processFFT(wavBufferForDFT + waveBufferOffset * wavSpec.channels, fftOut);
			waveBufferOffset += IN_SAMPLES_PER_ITERATION;
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