#include "HPSPitchDetector.h"
#include "SpectralProcessor.h"

float HPSPitchDetector::detectPitch (const float* magnitudeDb, int binCount, float binHz)
{
    return spectral_processor_hps (magnitudeDb, binCount, binHz, numHarmonics, minHz, maxHz);
}
