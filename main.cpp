#include "DftProcessor.h"
#include "Drawing.h"

const unsigned SCREEN_WIDTH = 240 * 3, SCREEN_HEIGHT = 160 * 3;
static auto MUSIC_FILENAME = "music-3.wav";
static const double TWENTY_OVER_LOG_10 = 20 / log(10);

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

	double theta = 0;
	const unsigned drawWidth = 240, drawHeight = 160;
	auto drawSurface = SDL_CreateRGBSurface(0, drawWidth, drawHeight, 32, 0xff << 16, 0xff << 8, 0xff, 0xff << 24);
	setCurrentSurface(drawSurface);
	clearScreen(RGB(0, 255, 23));
	SDL_SetRenderDrawColor(renderer, 0, 67, 23, 255);
	SDL_RenderClear(renderer);

	while (!quit && !dftProcessor.wouldOverflowWavFile()) {
		SDL_Event e;
		if (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) quit = true;
		}

		// Process frame
		if (needsRerender) {
			auto screenSurface = SDL_GetWindowSurface(window);

			//auto& dftOut(dftProcessor.currentDFT());
			//processor.useConversionToFrequencyDomainValues = true;
			//processor.useWindow = false;
			//for (unsigned i = 0; i < 256; i++) {
			//	float angle = i * 320.0f / 256;
			//	double dftValue = processor.getDftPointInterpolated(to_array(dftOut), i / 256.0, 50 Hz, wavSpec.freq / 2, true);
			//	double volume = processor.convertPointToDecibels(dftValue, 80 DB);
			//	unsigned vol = unsigned(volume * 256);
			//	for (unsigned j = 0; j < 256; j++) {
			//		uint32_t color = j > vol ? RGB(0, 0, 0) : HSV(angle, j * 140.0f / 256.f, 50 + j * 90.0f / 256.f);
			//		setPixel(j, i, color);
			//	}
			//}

			// 💡 GOOD
			//const unsigned BAR_HEIGHT = 4;
			//unsigned y = 0;
			//auto& dftOut(dftProcessor.currentDFT());
			//processor.useConversionToFrequencyDomainValues = false;
			//processor.useWindow = false;
			//for (unsigned i = 0; i < dftOut.size(); i++) {
			//	float angle = i * 360.0f / dftOut.size();
			//	double fraction = double(i) / (dftOut.size() - 1);
			//	double sample = processor.getDftPointInterpolated(to_array(dftOut), fraction, 50 Hz, wavSpec.freq / 2, false);
			//	double volume = processor.convertPointToDecibels(sample, 50 DB);
			//	unsigned vol = unsigned(volume * 256);
			//	for (unsigned j = 0; j < 256; j++) {
			//		uint32_t color = j > vol ? RGB(0, 0, 0) : HSV(angle, j * 140.0f / 256.f, 50 + j * 90.0f / 256.f);
			//		for (unsigned k = 0; k < BAR_HEIGHT; k++) {
			//			setPixel(j, y + k, color);
			//		}
			//	}
			//	y += BAR_HEIGHT;
			//}
			// 💡 END GOOD

			auto& dftOut(dftProcessor.currentDFT());
			processor.useConversionToFrequencyDomainValues = false;
			processor.useWindow = false;

			for (unsigned k = 0; k < 20; k++) {
				double rmax = 100, n = 6, d = 8;
				double r = rmax * cos(n / d * theta);
				double x = r * cos(theta);
				double y = r * sin(theta);
				setPixel(x + drawWidth / 2, y + drawHeight / 2, RGB(255, 255, 255));
				theta += 0.002;
			}

			// Dim image weirdly
			//dimScreenWeirdly();
			moveScreen(1, 0, 8);

			//Uint32 c1 = RGB(255, 255, 255), c2 = RGB(0, 0, 0);
			//Uint16 a = 16;
			//printf("Blending: %08x %08x a=%02x = %08x\n", c1, c2, a, blend(c1, c2, a));

			SDL_Rect srcRect = { 0, 0, drawWidth, drawHeight };
			SDL_Rect dstRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
			SDL_BlitScaled(drawSurface, &srcRect, screenSurface, &dstRect);

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
