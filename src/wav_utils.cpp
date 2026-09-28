#include "wav_utils.h"
#include <algorithm>
#include <cstring>

namespace wav_utils {

std::vector<uint8_t> pcm_float_to_wav(const float* samples, int32_t num_samples, int32_t sample_rate) {
    if (num_samples < 0) num_samples = 0;

    const uint16_t num_channels = 1;
    const uint16_t bits_per_sample = 16;
    const uint32_t byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
    const uint16_t block_align = num_channels * (bits_per_sample / 8);
    const uint32_t data_size = static_cast<uint32_t>(num_samples * sizeof(int16_t));
    const uint32_t chunk_size = 36 + data_size;

    std::vector<uint8_t> buffer(44 + data_size);
    uint8_t* ptr = buffer.data();

    // RIFF chunk descriptor
    std::memcpy(ptr, "RIFF", 4); ptr += 4;
    std::memcpy(ptr, &chunk_size, 4); ptr += 4;
    std::memcpy(ptr, "WAVE", 4); ptr += 4;

    // fmt sub-chunk
    std::memcpy(ptr, "fmt ", 4); ptr += 4;
    uint32_t subchunk1_size = 16;
    std::memcpy(ptr, &subchunk1_size, 4); ptr += 4;
    uint16_t audio_format = 1; // PCM
    std::memcpy(ptr, &audio_format, 2); ptr += 2;
    std::memcpy(ptr, &num_channels, 2); ptr += 2;
    uint32_t sr = static_cast<uint32_t>(sample_rate);
    std::memcpy(ptr, &sr, 4); ptr += 4;
    std::memcpy(ptr, &byte_rate, 4); ptr += 4;
    std::memcpy(ptr, &block_align, 2); ptr += 2;
    std::memcpy(ptr, &bits_per_sample, 2); ptr += 2;

    // data sub-chunk
    std::memcpy(ptr, "data", 4); ptr += 4;
    std::memcpy(ptr, &data_size, 4); ptr += 4;

    // Convert float samples to 16-bit signed PCM
    int16_t* pcm_ptr = reinterpret_cast<int16_t*>(ptr);
    for (int32_t i = 0; i < num_samples; ++i) {
        float s = samples ? samples[i] : 0.0f;
        s = std::clamp(s, -1.0f, 1.0f);
        pcm_ptr[i] = static_cast<int16_t>(s * 32767.0f);
    }

    return buffer;
}

} // namespace wav_utils
