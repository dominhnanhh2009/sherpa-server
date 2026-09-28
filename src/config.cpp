#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace {
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string resolve_path(const std::string& base, const std::string& sub) {
    if (sub.empty()) return "";
    std::filesystem::path p(sub);
    if (p.is_absolute()) return sub;
    std::filesystem::path b(base);
    return (b / p).lexically_normal().string();
}

bool parse_bool(const std::string& val) {
    std::string s = val;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return (s == "true" || s == "1" || s == "yes" || s == "on");
}
}

std::string TtsConfig::get_duration_predictor() const {
    return resolve_path(model_dir, duration_predictor);
}
std::string TtsConfig::get_text_encoder() const {
    return resolve_path(model_dir, text_encoder);
}
std::string TtsConfig::get_vector_estimator() const {
    return resolve_path(model_dir, vector_estimator);
}
std::string TtsConfig::get_vocoder() const {
    return resolve_path(model_dir, vocoder);
}
std::string TtsConfig::get_tts_json() const {
    return resolve_path(model_dir, tts_json);
}
std::string TtsConfig::get_unicode_indexer() const {
    return resolve_path(model_dir, unicode_indexer);
}
std::string TtsConfig::get_voice_style() const {
    return resolve_path(model_dir, voice_style);
}

std::string SttConfig::get_encoder() const {
    return resolve_path(model_dir, encoder);
}
std::string SttConfig::get_decoder() const {
    return resolve_path(model_dir, decoder);
}
std::string SttConfig::get_joiner() const {
    return resolve_path(model_dir, joiner);
}
std::string SttConfig::get_tokens() const {
    return resolve_path(model_dir, tokens);
}
std::string SttConfig::get_bpe_vocab() const {
    return resolve_path(model_dir, bpe_vocab);
}

AppConfig AppConfig::load_from_file(const std::string& path) {
    AppConfig config;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[CONFIG] Could not open " << path << ", using default configuration." << std::endl;
        return config;
    }

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            current_section = trim(line.substr(1, line.size() - 2));
            std::transform(current_section.begin(), current_section.end(), current_section.begin(), ::tolower);
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (current_section == "server") {
            if (key == "host") config.server.host = val;
            else if (key == "port") config.server.port = std::stoi(val);
            else if (key == "threads") config.server.threads = std::stoi(val);
        } else if (current_section == "tts") {
            if (key == "enabled") config.tts.enabled = parse_bool(val);
            else if (key == "model_type") config.tts.model_type = val;
            else if (key == "model_dir") config.tts.model_dir = val;
            else if (key == "duration_predictor") config.tts.duration_predictor = val;
            else if (key == "text_encoder") config.tts.text_encoder = val;
            else if (key == "vector_estimator") config.tts.vector_estimator = val;
            else if (key == "vocoder") config.tts.vocoder = val;
            else if (key == "tts_json") config.tts.tts_json = val;
            else if (key == "unicode_indexer") config.tts.unicode_indexer = val;
            else if (key == "voice_style") config.tts.voice_style = val;
            else if (key == "num_threads") config.tts.num_threads = std::stoi(val);
            else if (key == "debug") config.tts.debug = std::stoi(val);
        } else if (current_section == "stt") {
            if (key == "enabled") config.stt.enabled = parse_bool(val);
            else if (key == "model_type") config.stt.model_type = val;
            else if (key == "model_dir") config.stt.model_dir = val;
            else if (key == "encoder") config.stt.encoder = val;
            else if (key == "decoder") config.stt.decoder = val;
            else if (key == "joiner") config.stt.joiner = val;
            else if (key == "tokens") config.stt.tokens = val;
            else if (key == "bpe_vocab") config.stt.bpe_vocab = val;
            else if (key == "decoding_method") config.stt.decoding_method = val;
            else if (key == "sample_rate") config.stt.sample_rate = std::stoi(val);
            else if (key == "feature_dim") config.stt.feature_dim = std::stoi(val);
            else if (key == "num_threads") config.stt.num_threads = std::stoi(val);
            else if (key == "debug") config.stt.debug = std::stoi(val);
        }
    }

    std::cout << "[CONFIG] Loaded configuration from " << path << std::endl;
    return config;
}
