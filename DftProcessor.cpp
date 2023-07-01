#include "DftProcessor.h"

static const double TWENTY_OVER_LOG_10 = 20 / log(10);
static const double DECIBEL_CUTOFF = -100 DB;


// [-infinite, 0]
static inline double toDecibels(double sample) {
	//return 20 * log(sample) / log(10);
	return fmax(DECIBEL_CUTOFF, log(sample) * TWENTY_OVER_LOG_10);
}

DftProcessor::DftProcessor(unsigned samplesPerIteration)
	: inSamplesPerIteration(samplesPerIteration),
	outSamplesPerIteration(samplesPerIteration / 2 + 1),
	REX(outSamplesPerIteration), IMX(outSamplesPerIteration), samples(inSamplesPerIteration)
{
}

// inData is stereo, 16-bit data (L R L R, etc.)
// ⚠ useWindow method is not complete, it is valid only for the first half of the output samples; we should make a second pass, in which we shift the second part and process again, giving us a second first half that can be used.
void DftProcessor::processDFT(const int16_t* inData, double* outData, bool useConversionToFrequencyDomainValues, bool useWindow) {
	// Zero REX & IMX so they can be used as accumulators
	for (unsigned k = 0; k < outSamplesPerIteration; k++) {
		REX[k] = IMX[k] = 0;
	}

	// Avant ça on applique une fenêtre: https://en.wikipedia.org/wiki/Window_function#Flat_top_window
	const double a0 = 0.3635819, a1 = 0.4891775, a2 = 0.1365995, a3 = 0.0106411;
	for (unsigned k = 0; k < inSamplesPerIteration; k++) {
		double sample = double(*inData++) / (32768 * 2) + double(*inData++) / (32768 * 2);

		if (useWindow) {
			double angle = 2 * M_PI * k / inSamplesPerIteration;
			sample *= a0 - a1 * cos(angle) + a2 * cos(angle * 2) - a3 * cos(angle * 3);
		}
		samples[k] = sample;
	}

	// Correlate input with the cosine and sine wave
	for (unsigned k = 0; k < outSamplesPerIteration; k++) {
		// C'est que ça c'est la DFT
		for (unsigned i = 0; i < inSamplesPerIteration; i++) {
			REX[k] += samples[i] * cos(2 * M_PI * k * i / inSamplesPerIteration);
			IMX[k] += samples[i] * sin(2 * M_PI * k * i / inSamplesPerIteration);
		}

		if (useConversionToFrequencyDomainValues) {
			//// https://www.dspguide.com/ch8/5.htm
			if (k == 0 || k == outSamplesPerIteration - 1) {
				REX[k] /= inSamplesPerIteration;
			}
			else {
				REX[k] /= inSamplesPerIteration / 2;
			}
			IMX[k] = -IMX[k] / (inSamplesPerIteration / 2);
		}

		// Module du nombre complexe (distance à l'origine)
		// = pour chaque fréquence l'amplitude correspondante
		outData[k] = sqrt(REX[k] * REX[k] + IMX[k] * IMX[k]);
	}

	// Calculer en DB comme pour le volume
	// Bande de fréquence -> énergie qu'il y a dedans
	for (unsigned k = 0; k < outSamplesPerIteration; k++) {
		outData[k] = toDecibels(outData[k]);
	}
}

double DftProcessor::processVolume(const int16_t* inData) {
	const unsigned samples = inSamplesPerIteration;
	double sum = 0;

	for (unsigned k = 0; k < samples; k++) {
		double sample = double(inData[0]) / (32768 * 2) + double(inData[1]) / (32768 * 2);
		sum += sample * sample;
	}

	// TODO: demander https://dspillustrations.com/pages/posts/misc/decibel-conversion-factor-10-or-factor-20.html
	// C'est bien 20
	double linearVolume = sqrt(sum / samples);
	return toDecibels(linearVolume);
}

// Typical: minFrequency = 50 or 80, maxFrequency = wavSpec.freq / 2
double DftProcessor::getDftPointInterpolated(const double* dftOutData, double positionInSpectrumBetween0And1, unsigned minFrequency, unsigned maxFrequency, bool useLogarithmicScale) {
	double frequencyEquivalentInFft;
	if (useLogarithmicScale) {
		// Min frequency in the FFT corresponds to the first entry, and it's actually 0
		// Max frequency in the FFT is the same, it corresponds to the last entry, outSamplesPerIteration - 1
		double frequency = minFrequency * exp(positionInSpectrumBetween0And1 * log(maxFrequency / minFrequency));
		frequencyEquivalentInFft = outSamplesPerIteration * frequency / maxFrequency;
	}
	else {
		frequencyEquivalentInFft = positionInSpectrumBetween0And1 * (outSamplesPerIteration - 1);
	}
	//printf("getDftPointInterpolated: %f -> %f, entry = %d (%f) %d\n", positionInSpectrumBetween0And1, frequency, integerIndexValue, realIndexValue, nextIndexValue);

	// Interpolate in the DFT table
	unsigned integerIndexValue = unsigned(frequencyEquivalentInFft);
	if (integerIndexValue >= outSamplesPerIteration - 1) return dftOutData[integerIndexValue];

	double realIndexValue = frequencyEquivalentInFft - floor(frequencyEquivalentInFft);
	return dftOutData[integerIndexValue] * (1 - realIndexValue) + dftOutData[integerIndexValue + 1] * realIndexValue;
}

double DftProcessor::convertPointToDecibels(double sample, double cutoffDbLevel) {
	return 1 - (fmin(cutoffDbLevel, -sample) / cutoffDbLevel);
}

// -------------------------------------------------------
DftProcessorForWav::DftProcessorForWav(DftProcessor& processor, const int16_t* wavBuffer, uint32_t wavLength, const SDL_AudioSpec& wavSpec)
	: processor(processor),
	wavBuffer(wavBuffer),
	wavSpec(wavSpec),
	dftOut(processor.outSamplesPerIteration),
	waveBufferOffset(0),
	waveTotalSamples(wavLength / sizeof(wavBuffer[0]) / wavSpec.channels),
	nextValues(processor.outSamplesPerIteration),
	temp(processor.outSamplesPerIteration)
{
}

bool DftProcessorForWav::wouldOverflowWavFile()
{
	return waveBufferOffset + processor.inSamplesPerIteration > waveTotalSamples;
}

void DftProcessorForWav::processDFTInChunksAndSmooth(unsigned processingChunks, double alpha) {
	for (unsigned i = 0; i < processingChunks; i++) {
		if (wouldOverflowWavFile()) return;

		if (i == 0) {
			processor.processDFT(wavBuffer + waveBufferOffset * wavSpec.channels, &nextValues[0]);
		}
		else {
			processor.processDFT(wavBuffer + waveBufferOffset * wavSpec.channels, &temp[0]);
			for (unsigned i = 0; i < processor.outSamplesPerIteration; i++) nextValues[i] = fmax(nextValues[i], temp[i]);
		}

		waveBufferOffset += processor.inSamplesPerIteration;
	}

	for (unsigned i = 0; i < processor.outSamplesPerIteration; i++) {
		dftOut[i] = (1 - alpha) * dftOut[i] + alpha * nextValues[i];
	}
}

const vector<double>& DftProcessorForWav::lastProcessDFTResult() {
	return dftOut;
}

void DftProcessorForWav::processVolumeOnly(unsigned processingChunks, double alpha) {
	double volume;
	for (unsigned i = 0; i < processingChunks; i++) {
		if (wouldOverflowWavFile()) return;

		if (i == 0) {
			volume = processor.processVolume(wavBuffer + waveBufferOffset * wavSpec.channels);
		}
		else {
			double temp = processor.processVolume(wavBuffer + waveBufferOffset * wavSpec.channels);
			volume = fmax(volume, temp);
		}

		waveBufferOffset += processor.inSamplesPerIteration;
	}

	for (unsigned i = 0; i < processor.outSamplesPerIteration; i++) {
		dftOut[i] = (1 - alpha) * dftOut[i] + alpha * volume;
	}
}

void DftProcessorForWav::processDFT() {
	processor.processDFT(wavBuffer + waveBufferOffset * wavSpec.channels, &dftOut[0]);
	waveBufferOffset += processor.inSamplesPerIteration;
}
