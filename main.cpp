#include "DftProcessor.h"
//#include "DrawingSurface.h"
#include "DrawingFloat.h"
#include "Coroutines.h"

#define DRAWING_ROUTINE_TO_USE drawShit2
static auto DEFAULT_MUSIC_FILENAME = "music-2.wav";
unsigned processChunksAtOnce = 6;
bool wantsFullFrequencies = true; // if false, just computes the volume, same value on all bands

/*
 *	Idées:
 *  1. Utiliser le HSV pour faire un effet où les pixels sont colorés au centre puis à mesure qu'ils s'éloignent perdent leur saturation (mais gardent leur valeur), ou l'inverse.
 *     Note: je pourrais aussi faire un framebuffer en HSV tant qu'à faire (r=H, g=S, b=V), et Color(Uint32) fait une conversion RGB -> HSV.
 *  2. Effet simple de découpage de l'écran en 2, on dessine une wave et tout bouge en haut et en bas à chaque frame, sans blending/flou. La vitesse dépend du volume global (peut-être changer l'API pour avoir les 2).
 */
ReturnObject drawShit(DftProcessorForWav &dftProcessor, DftProcessor &processor, SDL_AudioSpec& wavSpec) noexcept {
	double theta = 0;
	auto currentColor = [&] {
		return HSV(int(theta / 4) % 360, 100, 50);
	};
	auto currentAccentColor = [&] {
		return HSV(int(theta / 4 + 180) % 360, 50, 100);
	};

	auto& ds = createDrawingSurface(240, 160, 3_X);
	ds.clearScreen(currentColor());
	ds.protectOverflow = true;
	processChunksAtOnce = 3;
	wantsFullFrequencies = false;

	ScreenMover screen;
	while (true) {
		auto& dftOut(dftProcessor.currentDFT());
		processor.useConversionToFrequencyDomainValues = false;
		processor.useWindow = false;

		double volume = processor.convertPointToDecibels(dftOut[0], 35_DB);
		Color color(currentAccentColor());
		for (unsigned k = 0; k < 40; k++) {
			double rmax = volume * 160, n = 6, d = 8;
			double r = rmax * cos(n / d * theta);
			double x = r * cos(theta);
			double y = r * sin(theta);
			ds.setPixel(x + ds.w / 2, y + ds.h / 2, Color(300, 300, 300));
			theta += 0.004;
		}

		screen.addMove(0.5, 0);
		screen.performMove(currentColor(), 40);
		co_await std::suspend_always{};
	}
}

ReturnObject drawShit2(DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
	double screenAngle = 0;
	double theta = 0;
	auto currentColor = [&] {
		return HSV(int(theta / 4) % 360, 100, 50);
	};
	auto currentAccentColor = [&] {
		return HSV(int(theta / 4 + 180) % 360, 50, 100);
	};
	ScreenMover screen;
	auto& ds = createDrawingSurface(240, 160, 3_X);
	ds.clearScreen(currentColor());
	ds.protectOverflow = true;
	processChunksAtOnce = 1;
	wantsFullFrequencies = false;


	while (true) {
		auto& dftOut(dftProcessor.currentDFT());
		processor.useConversionToFrequencyDomainValues = false;
		processor.useWindow = false;

		double volume = processor.convertPointToDecibels(dftOut[0], 35_DB);
		Color color(currentAccentColor());
		for (unsigned k = 0; k < 15; k++) {
			double rmax = volume * 160, n = 6, d = 8;
			double r = rmax * cos(n / d * theta);
			double x = r * cos(theta);
			double y = r * sin(theta);
			ds.setPixel(x + ds.w / 2, y + ds.h / 2, Color(128, 128, 128).add(color));
			theta += 0.004;
		}

		screenAngle += 0.0003;
		screen.addMove(cos(screenAngle) * 0.2, sin(screenAngle) * 0.2);
		screen.performMove(currentColor(), 40);
		co_await std::suspend_always{};
	}
}

//ReturnObject drawShit(DftProcessorForWav &dftProcessor, DftProcessor &processor, SDL_AudioSpec& wavSpec) noexcept {
//	const Uint32 colors[] = {
//		RGB(48, 48, 255),
//		RGB(255, 48, 255),
//		RGB(255, 255, 48)
//	};
//	double theta = 0;
//	auto currentColor = [&] {
//		return colors[unsigned(theta / 60) % numberof(colors)];
//	};
//
//	createDrawingSurface(240, 160, 3_X);
//	clearScreen(currentColor());
//
//	unsigned drawWidth = drawingSurface->w, drawHeight = drawingSurface->h;
//	while (true) {
//		auto& dftOut(dftProcessor.currentDFT());
//		processor.useConversionToFrequencyDomainValues = false;
//		processor.useWindow = false;
//
//		for (unsigned k = 0; k < 20; k++) {
//			double rmax = 80, n = 6, d = 8;
//			double r = rmax * cos(n / d * theta);
//			double x = r * cos(theta);
//			double y = r * sin(theta);
//			setPixel(x + drawWidth / 2, y + drawHeight / 2, RGB(255, 255, 255));
//			theta += 0.004;
//		}
//
//		printf("Theta: %f\n", theta);
//
//		// Dim image weirdly
//		//dimScreenWeirdly();
//		moveScreen(1, 0, currentColor());
//		co_await std::suspend_always{};
//	}
//}
//
//ReturnObject drawWithSmoothGraph(DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
//	createDrawingSurface(480, 320, 1_X);
//	clearScreen(RGB(48, 48, 255));
//
//	while (true) {
//		auto& dftOut(dftProcessor.currentDFT());
//		processor.useConversionToFrequencyDomainValues = true;
//		processor.useWindow = false;
//		for (unsigned i = 0; i < 256; i++) {
//			float angle = i * 320.0f / 256;
//			double dftValue = processor.getDftPointInterpolated(to_array(dftOut), i / 256.0, 50_Hz, wavSpec.freq / 2, true);
//			double volume = processor.convertPointToDecibels(dftValue, 80_DB);
//			unsigned vol = unsigned(volume * 256);
//			for (unsigned j = 0; j < 256; j++) {
//				uint32_t color = j > vol ? RGB(0, 0, 0) : HSV(angle, j * 140.0f / 256.f, 50 + j * 90.0f / 256.f);
//				setPixel(j, i, color);
//			}
//		}
//
//		co_await std::suspend_always{};
//	}
//}
//
//ReturnObject drawWithBars(DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
//	bool useLinearScale = true;
//	createDrawingSurface(256, 256, 2_X);
//	clearScreen(RGB(48, 48, 255));
//
//	while (true) {
//		const unsigned BAR_HEIGHT = 4;
//		unsigned y = 0;
//		auto& dftOut(dftProcessor.currentDFT());
//		processor.useConversionToFrequencyDomainValues = false;
//		processor.useWindow = false;
//		for (unsigned i = 0; i < dftOut.size(); i++) {
//			float angle = i * 360.0f / dftOut.size();
//			double fraction = double(i) / (dftOut.size() - 1);
//			double sample = processor.getDftPointInterpolated(to_array(dftOut), fraction, 50_Hz, wavSpec.freq / 2, false);
//			unsigned vol;
//			if (useLinearScale) {
//				// Division by 60 because sometimes it goes slightly over 0
//				vol = fmax(0, sample + 50) * 256 / 60;
//			}
//			else {
//				double volume = processor.convertPointToDecibels(sample, 50_DB);
//				vol = unsigned(volume * 256);
//			}
//			for (unsigned j = 0; j < 256; j++) {
//				uint32_t color = j > vol ? RGB(0, 0, 0) : HSV(angle, j * 140.0f / 256.f, 50 + j * 90.0f / 256.f);
//				for (unsigned k = 0; k < BAR_HEIGHT; k++) {
//					setPixel(j, y + k, color);
//				}
//			}
//			y += BAR_HEIGHT;
//		}
//
//		co_await std::suspend_always{};
//	}
//}


int main(int argc, char* args[]) {
#define QUIT() { system("pause"); return -1; }

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		QUIT();
	}

	window = SDL_CreateWindow("Challenge Vince", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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
	char fileName[4096];

	if (argc == 2) {
		strcpy_s(fileName, args[1]);
	} else {
		strcpy_s(fileName, DEFAULT_MUSIC_FILENAME);
		fprintf(stdout, "Note: you can pass the wav file to play as an argument (drag & drop on the executable)\nPlaying %s by default.\n", fileName);
	}

	SDL_Init(SDL_INIT_AUDIO);
	auto audioSpec = SDL_LoadWAV(fileName, &wavSpec, &wavBuffer, &wavLength);
	if (!audioSpec) {
		fprintf(stderr, "Failed to load WAV file %s\n", fileName);
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
	double lastProcessedTime, lastRenderedTime;
	DftProcessor processor(128);
	DftProcessorForWav dftProcessor(processor, (int16_t*)wavBuffer, wavLength, wavSpec);

	// Process a first sample
	dftProcessor.processDFT();
	lastRenderedTime = lastProcessedTime = getTime();
	SDL_PauseAudioDevice(deviceId, 0);

	//double currentVolume = 0;

	SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
	SDL_RenderClear(renderer);

	std::coroutine_handle<> drawingCoroutine = DRAWING_ROUTINE_TO_USE(dftProcessor, processor, wavSpec);
	double firstRenderedTime = getTime();
	unsigned renderedFrames = 0, drawnFrames = 0;
	printf("Target framerate: %f\n", 1.0 / (double(processor.inSamplesPerIteration * processChunksAtOnce) / wavSpec.freq));

	while (!quit && !dftProcessor.wouldOverflowWavFile()) {
		SDL_Event e;
		if (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) quit = true;
		}

		// Process frame
		if (needsRerender) {
			auto screenSurface = SDL_GetWindowSurface(window);

			//moveScreen(1, 0, 8);
			//Uint32 c1 = RGB(255, 255, 255), c2 = RGB(0, 0, 0);
			//Uint16 a = 16;
			//printf("Blending: %08x %08x a=%02x = %08x\n", c1, c2, a, blend(c1, c2, a));

			drawingCoroutine();
			needsRerender = false;
			drawnFrames += 1;

			double time = getTime();
			if (time - lastRenderedTime >= 1 / 60.0) {
				lastRenderedTime += 1 / 60.0;
				if (lastRenderedTime < time - 1 / 60.0) lastRenderedTime = time - 1 / 60.0;
				drawingSurface->blitToSdlSurface();
				SDL_Rect srcRect = { 0, 0, sdlDrawingSurface->w, sdlDrawingSurface->h };
				SDL_Rect dstRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
				SDL_BlitScaled(sdlDrawingSurface, &srcRect, screenSurface, &dstRect);

				SDL_UpdateWindowSurface(window);
				SDL_RenderPresent(renderer);

				renderedFrames += 1;
				if (renderedFrames >= 200) {
					printf("Average framerate: rendered=%f, drawn=%f\n", renderedFrames / (time - firstRenderedTime), drawnFrames / (time - firstRenderedTime));
					firstRenderedTime = time;
					renderedFrames = drawnFrames = 0;
				}
			}
		}

		// Wait until we have played the whole DFT'ed sample	
		double time = getTime();
		unsigned samplesPerProcessing = processor.inSamplesPerIteration * processChunksAtOnce;
		auto processIfNecessary = [&] {
			if ((time - lastProcessedTime) * wavSpec.freq >= samplesPerProcessing) {
				lastProcessedTime += double(processor.inSamplesPerIteration * processChunksAtOnce) / wavSpec.freq;
				if (wantsFullFrequencies) {
					dftProcessor.processDFTInChunksAndSmooth(processChunksAtOnce, 0.2);
				}
				else {
					dftProcessor.processVolumeOnly(processChunksAtOnce, 0.2);
				}

				needsRerender = true;
				return true;
			}
			return false;
		};
		processIfNecessary();
		if ((time - lastProcessedTime) * wavSpec.freq >= samplesPerProcessing * 20) {
			printf("Warning: lagging (lateness: %dx)\n", int((time - lastProcessedTime) * wavSpec.freq / samplesPerProcessing));
			while (processIfNecessary());
		}

		SDL_Delay(1);
	}

	drawingCoroutine.destroy();
	SDL_DestroyWindow(window);
	SDL_CloseAudioDevice(deviceId);
	SDL_FreeWAV(wavBuffer);
	SDL_Quit();
#undef QUIT
	return 0;
}
