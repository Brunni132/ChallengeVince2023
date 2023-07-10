#include "DftProcessor.h"
//#include "DrawingSurface.h"
#include "DrawingFloat.h"
#include "Coroutines.h"

/*
 *	Idées:
 *  1. Un fichier de config permettant de choisir l'effet, ou alors pouvoir passer d'une coroutine à l'autre en faisant gauche/droite.
 *  2. Utiliser le HSV pour faire un effet où les pixels sont colorés au centre puis à mesure qu'ils s'éloignent perdent leur saturation (mais gardent leur valeur), ou l'inverse.
 *  3. Effet simple de découpage de l'écran en 2, on dessine une wave et tout bouge en haut et en bas à chaque frame, sans blending/flou. La vitesse dépend du volume global (peut-être changer l'API pour avoir les 2).
 *  4. Effet radar (en HSL), où on dessine juste une ligne en vague représentant l'equalizer.
 */

#define DRAWING_ROUTINE_TO_USE colorfulRotatingParticles
static auto DEFAULT_MUSIC_FILENAME = "music-3.wav";
static const double MAX_RENDERED_FRAMERATE = 60;

struct Globals {
	unsigned processChunksAtOnce = 6;
	bool wantsFullFrequencies = true; // if false, just computes the volume, same value on all bands
	SDL_Scancode lastPressedKey = SDL_SCANCODE_UNKNOWN;
	// For programs using the rosace
	double n = 6, d = 8, extraSensitivity = 0;
};

ReturnObject testWithHSLFramebuffer(Globals& globals, DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
	auto& ds = createDrawingSurface(240, 160, 3_X);
	ds.protectOverflow = true;
	ds.useHsl = true;
	globals.processChunksAtOnce = 3;
	globals.wantsFullFrequencies = false;

	double s = 0;
	while (true) {
		for (unsigned y = 0; y < ds.h; y++) {
			float level = float(y) / ds.h;
			for (unsigned x = 0; x < ds.w; x++) {
				float hlevel = float(x) / ds.w;
				ds.setPixel(x, y, Color(s, hlevel * 2, level));
			}
		}
		s += 0.001;

		co_await std::suspend_always{};
	}
}

ReturnObject pointCloudLateralScrollingOnly(Globals& globals, DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
	double theta = 0;
	auto currentColor = [&] {
		return HSV(fmodf(theta / 4, 360), 100, 50);
	};
	auto currentAccentColor = [&] {
		return HSV(fmodf(theta / 4 + 180, 360), 50, 100);
	};

	auto& ds = createDrawingSurface(240, 160, 3_X);
	ds.clearScreen(currentColor());
	ds.protectOverflow = true;
	globals.processChunksAtOnce = 3;
	globals.wantsFullFrequencies = false;

	ScreenMover screen;
	while (true) {
		auto& dftOut(dftProcessor.currentDFT());
		processor.useConversionToFrequencyDomainValues = false;
		processor.useWindow = false;

		// https://www.asc.ohio-state.edu/orban.14/math_coding/rose/rose.html
		double volume = processor.convertPointToDecibels(dftOut[0], 35_DB + globals.extraSensitivity);
		Color color(currentAccentColor());
		for (unsigned k = 0; k < 40; k++) {
			double rmax = volume * 160;
			double r = rmax * cos(globals.n / globals.d * theta);
			double x = r * cos(theta);
			double y = r * sin(theta);
			ds.setPixel(x + ds.w / 2, y + ds.h / 2, Color(300, 300, 300));
			theta += 0.004;
		}

		screen.stashMove(0.5, 0);
		screen.performMove(ds, currentColor(), 40);
		co_await std::suspend_always{};
	}
}

ReturnObject pointCloudWithColorfulScrollingBackground(Globals& globals, DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
	double screenAngle = 0;
	double theta = 0;
	auto currentColor = [&] {
		return HSV(fmodf(theta / 4, 360), 100, 50);
	};
	auto currentAccentColor = [&] {
		return HSV(fmodf(theta / 4 + 180, 360), 50, 100);
	};
	ScreenMover screen;
	auto& ds = createDrawingSurface(240, 160, 3_X);
	ds.clearScreen(currentColor());
	ds.protectOverflow = true;
	globals.processChunksAtOnce = 1;
	globals.wantsFullFrequencies = false;


	while (true) {
		auto& dftOut(dftProcessor.currentDFT());
		processor.useConversionToFrequencyDomainValues = false;
		processor.useWindow = false;

		double volume = processor.convertPointToDecibels(dftOut[0], 35_DB + globals.extraSensitivity);
		Color color(currentAccentColor());
		for (unsigned k = 0; k < 15; k++) {
			double rmax = volume * 160;
			double r = rmax * cos(globals.n / globals.d * theta);
			double x = r * cos(theta);
			double y = r * sin(theta);
			ds.setPixel(x + ds.w / 2, y + ds.h / 2, Color(128, 128, 128).add(color));
			theta += 0.004;
		}

		screenAngle += 0.0003;
		screen.stashMove(cos(screenAngle) * 0.2, sin(screenAngle) * 0.2);
		screen.performMove(ds, currentColor(), 40);
		co_await std::suspend_always{};
	}
}

ReturnObject colorfulRosaceRGB(Globals& globals, DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
	double screenAngle = 0;
	double theta = 0;
	auto currentColor = [&] {
		return Color(64, 64, 64);
	};
	ScreenMover screen;
	auto& ds = createDrawingSurface(240, 160, 3_X);
	ds.clearScreen(currentColor());
	ds.protectOverflow = true;
	globals.processChunksAtOnce = 1;
	globals.wantsFullFrequencies = false;


	while (true) {
		auto& dftOut(dftProcessor.currentDFT());
		processor.useConversionToFrequencyDomainValues = false;
		processor.useWindow = false;

		double volume = processor.convertPointToDecibels(dftOut[0], 35_DB + globals.extraSensitivity);
		for (unsigned k = 0; k < 15; k++) {
			double rmax = volume * 160;
			double r = rmax * cos(globals.n / globals.d * theta);
			double x = r * cos(theta);
			double y = r * sin(theta);
			Uint32 color = HSV(fmodf(theta * 360, 360), 100, 100);
			ds.fillRect(x + ds.w / 2, y + ds.h / 2, 2, 2, color);
			theta += 0.004;
		}

		screenAngle += 0.0003;
		screen.stashMove(cos(screenAngle) * 0.2, sin(screenAngle) * 0.2);
		screen.performMove(ds, currentColor(), 32);
		co_await std::suspend_always{};
	}
}

ReturnObject colorfulRosaceHSL(Globals& globals, DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
	double screenAngle = 0;
	double theta = 0;
	auto currentColor = [&] {
		return Color(0, 0, 0);
	};
	ScreenMover screen;
	auto& ds = createDrawingSurface(240, 160, 3_X);
	ds.clearScreen(currentColor());
	ds.protectOverflow = true;
	ds.useHsl = true;
	globals.processChunksAtOnce = 1;
	globals.wantsFullFrequencies = false;


	while (true) {
		auto& dftOut(dftProcessor.currentDFT());
		processor.useConversionToFrequencyDomainValues = false;
		processor.useWindow = false;

		double volume = processor.convertPointToDecibels(dftOut[0], 35_DB + globals.extraSensitivity);
		for (unsigned k = 0; k < 15; k++) {
			double rmax = volume * 160;
			double r = rmax * cos(globals.n / globals.d * theta);
			double x = r * cos(theta);
			double y = r * sin(theta);
			Color color(fmodf(theta, 1), 1, .5f);
			ds.fillRect(x + ds.w / 2, y + ds.h / 2, 2, 2, color);
			theta += 0.004;
		}

		screenAngle += 0.0003;
		screen.stashMove(cos(screenAngle) * 0.2, sin(screenAngle) * 0.2);
		screen.performMoveInHSLMode(ds, currentColor(), 40);
		co_await std::suspend_always{};
	}
}

ReturnObject colorfulRotatingParticles(Globals& globals, DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
	double screenAngle = 0;
	double theta = 0;
	auto currentColor = [&] {
		return Color(0, 0, 0);
	};
	ScreenStretcher screen;
	auto& ds = createDrawingSurface(240, 160, 3_X);
	ds.clearScreen(currentColor());
	globals.processChunksAtOnce = 6;
	globals.wantsFullFrequencies = true;

	while (true) {
		auto& dftOut(dftProcessor.currentDFT());
		processor.useConversionToFrequencyDomainValues = false;
		processor.useWindow = false;

		unsigned totalSteps = 30;
		for (unsigned k = 0; k < totalSteps; k++) {
			double dftValue = processor.getDftPointInterpolated(to_array(dftOut), 1 - double(k) / totalSteps, 50_Hz, wavSpec.freq / 2, true);
			double volume = processor.convertPointToDecibels(dftValue, 35_DB + globals.extraSensitivity);
			double r = volume * 80, angle = 2 * M_PI * k / totalSteps + screenAngle;
			double x = r * cos(angle);
			double y = r * -sin(angle);
			Uint32 color = HSV(fmodf(angle * 720 / (2 * M_PI) + theta, 360), 100, 100);
			ds.fillRect(x + ds.w / 2, y + ds.h / 2, 1, 1, color);
		}

		screenAngle += 0.03;
		screen.stashMove(0.2);
		screen.performCircular(ds, currentColor(), 60, true);
		co_await std::suspend_always{};
	}
}

ReturnObject smoothGraph(Globals& globals, DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
	auto& ds = createDrawingSurface(480, 320, 1_X);
	globals.processChunksAtOnce = 6;
	globals.wantsFullFrequencies = true;

	ds.clearScreen(RGB(48, 48, 255));
	while (true) {
		auto& dftOut(dftProcessor.currentDFT());
		processor.useConversionToFrequencyDomainValues = true;
		processor.useWindow = false;
		for (unsigned i = 0; i < 320; i++) {
			float angle = i * 320.0f / 320;
			double dftValue = processor.getDftPointInterpolated(to_array(dftOut), i / 320.0, 50_Hz, wavSpec.freq / 2, true);
			double volume = processor.convertPointToDecibels(dftValue, 80_DB + globals.extraSensitivity);
			unsigned vol = unsigned(volume * 256);
			for (unsigned j = 0; j < 480; j++) {
				uint32_t color = j > vol ? RGB(0, 0, 0) : HSV(angle, j * 140.0f / 256.f, 50 + j * 90.0f / 256.f);
				ds.setPixel(j, i, color);
			}
		}

		co_await std::suspend_always{};
	}
}

ReturnObject eqBars(Globals& globals, DftProcessorForWav& dftProcessor, DftProcessor& processor, SDL_AudioSpec& wavSpec) noexcept {
	bool useLinearScale = true;
	auto& ds = createDrawingSurface(480, 320, 1_X);
	globals.processChunksAtOnce = 6;
	globals.wantsFullFrequencies = true;

	ds.clearScreen(RGB(48, 48, 255));
	while (true) {
		const unsigned BAR_HEIGHT = 4;
		unsigned y = 0;
		auto& dftOut(dftProcessor.currentDFT());
		processor.useConversionToFrequencyDomainValues = false;
		processor.useWindow = false;
		for (unsigned i = 0; i < dftOut.size(); i++) {
			float angle = i * 360.0f / dftOut.size();
			double fraction = double(i) / (dftOut.size() - 1);
			double sample = processor.getDftPointInterpolated(to_array(dftOut), fraction, 50_Hz, wavSpec.freq / 2, false);
			unsigned vol;
			if (useLinearScale) {
				// Division by 60 because sometimes it goes slightly over 0
				vol = unsigned(fmax(0, sample + 50 + globals.extraSensitivity) * 256 / 60);
			}
			else {
				double volume = processor.convertPointToDecibels(sample, 50_DB + globals.extraSensitivity);
				vol = unsigned(volume * 256);
			}
			for (unsigned j = 0; j < 256; j++) {
				uint32_t color = j > vol ? RGB(0, 0, 0) : HSV(angle, j * 140.0f / 256.f, 50 + j * 90.0f / 256.f);
				for (unsigned k = 0; k < BAR_HEIGHT; k++) {
					ds.setPixel(j, y + k, color);
				}
			}
			y += BAR_HEIGHT;
		}

		co_await std::suspend_always{};
	}
}

ReturnObject (*drawingRoutines[])(Globals& globals, DftProcessorForWav&, DftProcessor&, SDL_AudioSpec&) noexcept = {
	colorfulRotatingParticles,
	pointCloudWithColorfulScrollingBackground,
	eqBars,
	smoothGraph,
	colorfulRosaceRGB,
	testWithHSLFramebuffer,
	pointCloudLateralScrollingOnly,
	colorfulRosaceHSL,
};

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

	Globals globals;
	int currentDrawingRoutine = 0;
	std::coroutine_handle<> drawingCoroutine;
	double firstRenderedTime = getTime();
	unsigned renderedFrames = 0, drawnFrames = 0;
	auto useDrawingRoutine = [&] {
		drawingCoroutine = drawingRoutines[currentDrawingRoutine](globals, dftProcessor, processor, wavSpec);
		printf("Target framerate: %f\n", 1.0 / (double(processor.inSamplesPerIteration * globals.processChunksAtOnce) / wavSpec.freq));
	};

	useDrawingRoutine();
	printf("Note: use left/right to cycle through effects. F1-F6 keys affect some parameters.\n");

	while (!quit && !dftProcessor.wouldOverflowWavFile()) {
		SDL_Event e;
		globals.lastPressedKey = SDL_SCANCODE_UNKNOWN;
		if (SDL_PollEvent(&e)) {
			if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
					if (++currentDrawingRoutine >= numberof(drawingRoutines)) currentDrawingRoutine = 0;
					useDrawingRoutine();
				}
				else if (e.key.keysym.scancode == SDL_SCANCODE_LEFT) {
					if (--currentDrawingRoutine < 0) currentDrawingRoutine = numberof(drawingRoutines) - 1;
					useDrawingRoutine();
				}
				else if (e.key.keysym.scancode == SDL_SCANCODE_F1) {
					globals.extraSensitivity -= 5;
					printf("extra sensitivity=%f\n", globals.extraSensitivity);
				}
				else if (e.key.keysym.scancode == SDL_SCANCODE_F2) {
					globals.extraSensitivity += 5;
					printf("extra sensitivity=%f\n", globals.extraSensitivity);
				}
				else if (e.key.keysym.scancode == SDL_SCANCODE_F3) {
					if (--globals.n < 1) globals.n = 1;
					printf("n=%f, d=%f\n", globals.n, globals.d);
				}
				else if (e.key.keysym.scancode == SDL_SCANCODE_F4) {
					globals.n++;
					printf("n=%f, d=%f\n", globals.n, globals.d);
				}
				else if (e.key.keysym.scancode == SDL_SCANCODE_F5) {
					if (--globals.d < 1) globals.d = 1;
					printf("n=%f, d=%f\n", globals.n, globals.d);
				}
				else if (e.key.keysym.scancode == SDL_SCANCODE_F6) {
					globals.d++;
					printf("n=%f, d=%f\n", globals.n, globals.d);
				}
				else {
					globals.lastPressedKey = e.key.keysym.scancode;
				}
			}
			else if (e.type == SDL_QUIT) {
				quit = true;
			}
		}

		// Process frame
		if (needsRerender) {
			auto screenSurface = SDL_GetWindowSurface(window);

			drawingCoroutine();
			needsRerender = false;
			drawnFrames += 1;

			double time = getTime();
			if (time - lastRenderedTime >= 1 / MAX_RENDERED_FRAMERATE) {
				lastRenderedTime += 1 / MAX_RENDERED_FRAMERATE;
				if (lastRenderedTime < time - 1 / MAX_RENDERED_FRAMERATE) lastRenderedTime = time - 1 / MAX_RENDERED_FRAMERATE;
				g_drawingSurface->blitToSdlSurface();
				SDL_Rect srcRect = { 0, 0, g_sdlSurface->w, g_sdlSurface->h };
				SDL_Rect dstRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
				SDL_BlitScaled(g_sdlSurface, &srcRect, screenSurface, &dstRect);

				SDL_UpdateWindowSurface(window);
				SDL_RenderPresent(renderer);

				renderedFrames += 1;
				if (time - firstRenderedTime >= 5) {
					printf("Average framerate: rendered=%f, drawn=%f\n", renderedFrames / (time - firstRenderedTime), drawnFrames / (time - firstRenderedTime));
					firstRenderedTime = time;
					renderedFrames = drawnFrames = 0;
				}
			}
		}

		// Wait until we have played the whole DFT'ed sample	
		double time = getTime();
		unsigned samplesPerProcessing = processor.inSamplesPerIteration * globals.processChunksAtOnce;
		auto processIfNecessary = [&] {
			if ((time - lastProcessedTime) * wavSpec.freq >= samplesPerProcessing) {
				lastProcessedTime += double(processor.inSamplesPerIteration * globals.processChunksAtOnce) / wavSpec.freq;
				if (globals.wantsFullFrequencies) {
					dftProcessor.processDFTInChunksAndSmooth(globals.processChunksAtOnce, 0.2);
				}
				else {
					dftProcessor.processVolumeOnly(globals.processChunksAtOnce, 0.2);
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
