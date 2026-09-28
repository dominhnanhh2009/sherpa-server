#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace wav_utils {

// Encodes float PCM samples in [-1.0, 1.0] into a standard 16-bit mono WAV container in memory.
std::vector<uint8_t> pcm_float_to_wav(const float* samples, int32_t num_samples, int32_t sample_rate);

} // namespace wav_utils
