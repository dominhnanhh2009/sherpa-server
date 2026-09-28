#include <drogon/drogon.h>
#include <iostream>
#include <fstream>
#include <string>
#include "config.h"
#include "sherpa_engine.h"
#include "controllers.h"

int run_test_infer() {
    std::cout << "[TEST] Running TTS & STT offline inference test..." << std::endl;
    AppConfig config = AppConfig::load_from_file("models.ini");
    SherpaEngine::instance().init(config);

    // Test TTS
    auto tts_res = SherpaEngine::instance().synthesize("Xin chào thế giới!", 0, 1.0f, 0.2f, 3);
    if (!tts_res.success) {
        std::cerr << "[ERROR] TTS synthesis test failed: " << tts_res.error_message << std::endl;
        return 1;
    }
    std::cout << "[TEST] TTS success: generated " << tts_res.wav_data.size()
              << " bytes of WAV audio, duration: " << tts_res.duration << "s" << std::endl;

    // Test STT with test wav
    std::string test_wav = "models/sherpa-onnx-zipformer-vi-30M-int8-2026-02-09/test_wavs/0.wav";
    std::ifstream f(test_wav, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[ERROR] Could not open test WAV file: " << test_wav << std::endl;
        return 1;
    }
    std::vector<char> buffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    auto stt_res = SherpaEngine::instance().recognize(buffer.data(), buffer.size());
    if (!stt_res.success) {
        std::cerr << "[ERROR] STT recognition test failed: " << stt_res.error_message << std::endl;
        return 1;
    }
    std::cout << "[TEST] STT success: \"" << stt_res.text << "\"" << std::endl;
    std::cout << "[TEST] Duration: " << stt_res.duration << "s, Tokens: " << stt_res.tokens.size() << std::endl;
    std::cout << "[TEST] ALL SMOKE TESTS PASSED!" << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    std::string config_path = "models.ini";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test") {
            std::cout << "[TEST] Binary executes cleanly!" << std::endl;
            return 0;
        } else if (arg == "--test-infer") {
            return run_test_infer();
        } else if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    AppConfig config = AppConfig::load_from_file(config_path);
    SherpaEngine::instance().init(config);

    controllers::register_routes(drogon::app());

    std::cout << "=================================================" << std::endl;
    std::cout << "  Sherpa Server is starting on http://"
              << config.server.host << ":" << config.server.port << std::endl;
    std::cout << "  Web UI demo: http://" << config.server.host << ":" << config.server.port << "/" << std::endl;
    std::cout << "=================================================" << std::endl;

    drogon::app()
        .addListener(config.server.host, config.server.port)
        .setThreadNum(config.server.threads)
        .setClientMaxBodySize(32 * 1024 * 1024) // 32MB max upload for audio files
        .run();

    return 0;
}
