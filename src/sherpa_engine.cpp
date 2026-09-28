#include "sherpa_engine.h"
#include "wav_utils.h"
#include <iostream>
#include <cstring>

SherpaEngine& SherpaEngine::instance() {
    static SherpaEngine s_instance;
    return s_instance;
}

void SherpaEngine::init(const AppConfig& config) {
    config_ = config;
    std::cout << "[ENGINE] Initialized with config. Lazy loading enabled." << std::endl;
}

bool SherpaEngine::is_tts_loaded() const {
    std::lock_guard<std::mutex> lock(tts_mutex_);
    return tts_loaded_;
}

bool SherpaEngine::is_stt_loaded() const {
    std::lock_guard<std::mutex> lock(stt_mutex_);
    return stt_loaded_;
}

bool SherpaEngine::ensure_tts_loaded(std::string& err) {
    std::lock_guard<std::mutex> lock(tts_mutex_);
    if (tts_loaded_) return true;

    if (!config_.tts.enabled) {
        err = "TTS is disabled in configuration";
        return false;
    }

    std::cout << "[ENGINE] Lazy loading TTS model (" << config_.tts.model_dir << ")..." << std::endl;

    SherpaOnnxOfflineTtsConfig tts_cfg;
    std::memset(&tts_cfg, 0, sizeof(tts_cfg));
    tts_cfg.model.num_threads = config_.tts.num_threads;
    tts_cfg.model.provider = "cpu";
    tts_cfg.model.debug = config_.tts.debug;

    std::string dp = config_.tts.get_duration_predictor();
    std::string te = config_.tts.get_text_encoder();
    std::string ve = config_.tts.get_vector_estimator();
    std::string vo = config_.tts.get_vocoder();
    std::string tj = config_.tts.get_tts_json();
    std::string ui = config_.tts.get_unicode_indexer();
    std::string vs = config_.tts.get_voice_style();

    tts_cfg.model.supertonic.duration_predictor = dp.c_str();
    tts_cfg.model.supertonic.text_encoder = te.c_str();
    tts_cfg.model.supertonic.vector_estimator = ve.c_str();
    tts_cfg.model.supertonic.vocoder = vo.c_str();
    tts_cfg.model.supertonic.tts_json = tj.c_str();
    tts_cfg.model.supertonic.unicode_indexer = ui.c_str();
    tts_cfg.model.supertonic.voice_style = vs.c_str();

    tts_ = SherpaOnnxCreateOfflineTts(&tts_cfg);
    if (!tts_) {
        err = "Failed to load Supertonic TTS model from " + config_.tts.model_dir;
        tts_load_error_ = err;
        std::cerr << "[ENGINE] " << err << std::endl;
        return false;
    }

    tts_sample_rate_ = SherpaOnnxOfflineTtsSampleRate(tts_);
    tts_num_speakers_ = SherpaOnnxOfflineTtsNumSpeakers(tts_);
    tts_loaded_ = true;
    std::cout << "[ENGINE] TTS loaded into RAM successfully! Sample rate: "
              << tts_sample_rate_ << ", Speakers: " << tts_num_speakers_ << std::endl;
    return true;
}

bool SherpaEngine::ensure_stt_loaded(std::string& err) {
    std::lock_guard<std::mutex> lock(stt_mutex_);
    if (stt_loaded_) return true;

    if (!config_.stt.enabled) {
        err = "STT is disabled in configuration";
        return false;
    }

    std::cout << "[ENGINE] Lazy loading STT model (" << config_.stt.model_dir << ")..." << std::endl;

    SherpaOnnxOfflineRecognizerConfig asr_cfg;
    std::memset(&asr_cfg, 0, sizeof(asr_cfg));
    asr_cfg.feat_config.sample_rate = config_.stt.sample_rate;
    asr_cfg.feat_config.feature_dim = config_.stt.feature_dim;
    asr_cfg.model_config.num_threads = config_.stt.num_threads;
    asr_cfg.model_config.provider = "cpu";
    asr_cfg.model_config.debug = config_.stt.debug;

    std::string enc = config_.stt.get_encoder();
    std::string dec = config_.stt.get_decoder();
    std::string join = config_.stt.get_joiner();
    std::string tok = config_.stt.get_tokens();
    std::string bpe = config_.stt.get_bpe_vocab();

    asr_cfg.model_config.transducer.encoder = enc.c_str();
    asr_cfg.model_config.transducer.decoder = dec.c_str();
    asr_cfg.model_config.transducer.joiner = join.c_str();
    asr_cfg.model_config.tokens = tok.c_str();
    asr_cfg.model_config.bpe_vocab = bpe.c_str();
    asr_cfg.decoding_method = config_.stt.decoding_method.c_str();

    stt_ = SherpaOnnxCreateOfflineRecognizer(&asr_cfg);
    if (!stt_) {
        err = "Failed to load Zipformer STT model from " + config_.stt.model_dir;
        stt_load_error_ = err;
        std::cerr << "[ENGINE] " << err << std::endl;
        return false;
    }

    stt_sample_rate_ = config_.stt.sample_rate;
    stt_loaded_ = true;
    std::cout << "[ENGINE] STT loaded into RAM successfully!" << std::endl;
    return true;
}

TtsResult SherpaEngine::synthesize(const std::string& text, int32_t sid, float speed, float silence_scale, int32_t num_steps) {
    TtsResult result;
    std::string err;
    if (!ensure_tts_loaded(err)) {
        result.success = false;
        result.error_code = "MODEL_LOAD_FAILED";
        result.error_message = err;
        return result;
    }

    std::lock_guard<std::mutex> lock(tts_mutex_);
    SherpaOnnxGenerationConfig gen_cfg;
    std::memset(&gen_cfg, 0, sizeof(gen_cfg));
    gen_cfg.sid = sid;
    gen_cfg.speed = speed;
    gen_cfg.silence_scale = silence_scale;
    gen_cfg.num_steps = num_steps;

    const SherpaOnnxGeneratedAudio* audio =
        SherpaOnnxOfflineTtsGenerateWithConfig(tts_, text.c_str(), &gen_cfg, nullptr, nullptr);

    if (!audio || audio->n <= 0) {
        result.success = false;
        result.error_code = "INTERNAL_INFERENCE_ERROR";
        result.error_message = "Supertonic TTS synthesis failed or produced no audio.";
        if (audio) SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);
        return result;
    }

    result.sample_rate = audio->sample_rate;
    result.duration = static_cast<float>(audio->n) / static_cast<float>(audio->sample_rate);
    result.wav_data = wav_utils::pcm_float_to_wav(audio->samples, audio->n, audio->sample_rate);
    result.success = true;

    SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);
    return result;
}

SttResult SherpaEngine::recognize(const char* wav_data, size_t wav_len) {
    SttResult result;
    if (!wav_data || wav_len < 44) {
        result.success = false;
        result.error_code = "INVALID_AUDIO_FORMAT";
        result.error_message = "Audio data is empty or too short to be a valid WAV file.";
        return result;
    }

    std::string err;
    if (!ensure_stt_loaded(err)) {
        result.success = false;
        result.error_code = "MODEL_LOAD_FAILED";
        result.error_message = err;
        return result;
    }

    std::lock_guard<std::mutex> lock(stt_mutex_);
    const SherpaOnnxWave* wave = SherpaOnnxReadWaveFromBinaryData(wav_data, static_cast<int32_t>(wav_len));
    if (!wave) {
        result.success = false;
        result.error_code = "INVALID_AUDIO_FORMAT";
        result.error_message = "Failed to parse audio. Please provide a valid 16-bit PCM WAV file.";
        return result;
    }

    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(stt_);
    if (!stream) {
        SherpaOnnxFreeWave(wave);
        result.success = false;
        result.error_code = "INTERNAL_INFERENCE_ERROR";
        result.error_message = "Failed to create recognizer stream.";
        return result;
    }

    SherpaOnnxAcceptWaveformOffline(stream, wave->sample_rate, wave->samples, wave->num_samples);
    SherpaOnnxDecodeOfflineStream(stt_, stream);
    const SherpaOnnxOfflineRecognizerResult* res = SherpaOnnxGetOfflineStreamResult(stream);

    result.success = true;
    result.text = (res && res->text) ? res->text : "";
    result.sample_rate = wave->sample_rate;
    result.duration = static_cast<float>(wave->num_samples) / static_cast<float>(wave->sample_rate);

    if (res && res->count > 0 && res->tokens_arr) {
        for (int32_t i = 0; i < res->count; ++i) {
            if (res->tokens_arr[i]) {
                result.tokens.emplace_back(res->tokens_arr[i]);
            }
            if (res->timestamps) {
                result.timestamps.push_back(res->timestamps[i]);
            }
        }
    }

    if (res) SherpaOnnxDestroyOfflineRecognizerResult(res);
    SherpaOnnxDestroyOfflineStream(stream);
    SherpaOnnxFreeWave(wave);

    return result;
}

Json::Value SherpaEngine::get_models_status() {
    Json::Value root;

    Json::Value tts_info;
    tts_info["model_name"] = config_.tts.model_dir;
    tts_info["type"] = config_.tts.model_type;
    tts_info["status"] = is_tts_loaded() ? "loaded" : "standby";
    if (is_tts_loaded()) {
        tts_info["sample_rate"] = tts_sample_rate_;
        tts_info["num_speakers"] = tts_num_speakers_;
    }
    root["tts"] = tts_info;

    Json::Value stt_info;
    stt_info["model_name"] = config_.stt.model_dir;
    stt_info["type"] = config_.stt.model_type;
    stt_info["status"] = is_stt_loaded() ? "loaded" : "standby";
    stt_info["sample_rate"] = stt_sample_rate_;
    root["stt"] = stt_info;

    return root;
}

SherpaEngine::~SherpaEngine() {
    if (tts_) {
        SherpaOnnxDestroyOfflineTts(tts_);
        tts_ = nullptr;
    }
    if (stt_) {
        SherpaOnnxDestroyOfflineRecognizer(stt_);
        stt_ = nullptr;
    }
}
