#pragma once

#include "config.h"
#include <sherpa-onnx/c-api/c-api.h>
#include <json/json.h>
#include <mutex>
#include <string>
#include <vector>
#include <memory>

struct TtsResult {
    bool success = false;
    std::vector<uint8_t> wav_data;
    float duration = 0.0f;
    int32_t sample_rate = 0;
    std::string error_code;
    std::string error_message;
};

struct SttResult {
    bool success = false;
    std::string text;
    float duration = 0.0f;
    int32_t sample_rate = 0;
    std::vector<std::string> tokens;
    std::vector<float> timestamps;
    std::string error_code;
    std::string error_message;
};

class SherpaEngine {
public:
    static SherpaEngine& instance();

    void init(const AppConfig& config);

    bool is_tts_loaded() const;
    bool is_stt_loaded() const;

    TtsResult synthesize(const std::string& text, int32_t sid, float speed, float silence_scale, int32_t num_steps);
    SttResult recognize(const char* wav_data, size_t wav_len);

    Json::Value get_models_status();

    ~SherpaEngine();

private:
    SherpaEngine() = default;
    SherpaEngine(const SherpaEngine&) = delete;
    SherpaEngine& operator=(const SherpaEngine&) = delete;

    bool ensure_tts_loaded(std::string& err);
    bool ensure_stt_loaded(std::string& err);

    AppConfig config_;

    mutable std::mutex tts_mutex_;
    const SherpaOnnxOfflineTts* tts_ = nullptr;
    bool tts_loaded_ = false;
    std::string tts_load_error_;
    int32_t tts_sample_rate_ = 0;
    int32_t tts_num_speakers_ = 0;

    mutable std::mutex stt_mutex_;
    const SherpaOnnxOfflineRecognizer* stt_ = nullptr;
    bool stt_loaded_ = false;
    std::string stt_load_error_;
    int32_t stt_sample_rate_ = 16000;
};
