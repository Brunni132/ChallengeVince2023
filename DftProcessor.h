#pragma once

#include <SDL.h>
#include <stdio.h>
#include <memory.h>
#include <vector>
#include "Ref.h"
#include <inttypes.h>
#include <math.h>
#include <malloc.h>

using std::vector;

#define stackArray(type, count)	((type*)alloca(sizeof(type) * count))

template <typename T, unsigned N> constexpr unsigned numberof(const T(&)[N]) { return N; }

#define DB
#define Hz

struct DftProcessor {
	DftProcessor(unsigned samplesPerIteration);

	void processDFT(const int16_t* inData, double* outData, bool useConversionToFrequencyDomainValues = false, bool useWindow = false);
	double processVolume(const int16_t* inData);
	double getDftPointInterpolated(const double* dftOutData, double positionInSpectrumBetween0And1, unsigned minFrequency, unsigned maxFrequency, bool useLogarithmicScale);
	static double convertPointToDecibels(double sample, double cutoffDbLevel);

	const unsigned inSamplesPerIteration, outSamplesPerIteration;

private:
	vector<double> REX, IMX, samples;
};

struct DftProcessorForWav {
	DftProcessorForWav(DftProcessor& processor, const int16_t* wavBuffer, uint32_t wavLength, const SDL_AudioSpec& wavSpec);

	DftProcessor& processor;
	const int16_t* const wavBuffer;
	const SDL_AudioSpec& wavSpec;
	uint32_t waveBufferOffset;
	const uint32_t waveTotalSamples;

	void processDFT();
	void processDFTInChunksAndSmooth(unsigned processingChunks, double alpha);
	void processVolumeOnly(unsigned processingChunks, double alpha);
	const vector<double>& lastProcessDFTResult();
	bool wouldOverflowWavFile();

private:
	vector<double> nextValues, temp;
	vector<double> dftOut;
};

