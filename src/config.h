#pragma once

#include <string>
#include <filesystem>

struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = 5555;
    int threads = 4;
};

struct TtsConfig {
    bool enabled = true;
    std::string model_type = "supertonic";
    std::string model_dir = "models/sherpa-onnx-supertonic-3-tts-int8-2026-05-11";
    std::string duration_predictor = "duration_predictor.int8.onnx";
    std::string text_encoder = "text_encoder.int8.onnx";
    std::string vector_estimator = "vector_estimator.int8.onnx";
    std::string vocoder = "vocoder.int8.onnx";
    std::string tts_json = "tts.json";
    std::string unicode_indexer = "unicode_indexer.bin";
    std::string voice_style = "voice.bin";
    int num_threads = 2;
    int debug = 0;

    std::string get_duration_predictor() const;
    std::string get_text_encoder() const;
    std::string get_vector_estimator() const;
    std::string get_vocoder() const;
    std::string get_tts_json() const;
    std::string get_unicode_indexer() const;
    std::string get_voice_style() const;
};

struct SttConfig {
    bool enabled = true;
    std::string model_type = "zipformer";
    std::string model_dir = "models/sherpa-onnx-zipformer-vi-30M-int8-2026-02-09";
    std::string encoder = "encoder.int8.onnx";
    std::string decoder = "decoder.onnx";
    std::string joiner = "joiner.int8.onnx";
    std::string tokens = "tokens.txt";
    std::string bpe_vocab = "bpe.model";
    std::string decoding_method = "greedy_search";
    int sample_rate = 16000;
    int feature_dim = 80;
    int num_threads = 2;
    int debug = 0;

    std::string get_encoder() const;
    std::string get_decoder() const;
    std::string get_joiner() const;
    std::string get_tokens() const;
    std::string get_bpe_vocab() const;
};

struct AppConfig {
    ServerConfig server;
    TtsConfig tts;
    SttConfig stt;

    static AppConfig load_from_file(const std::string& path = "models.ini");
};
