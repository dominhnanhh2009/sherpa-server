#include "controllers.h"
#include "sherpa_engine.h"
#include <drogon/MultiPart.h>
#include <chrono>
#include <fstream>
#include <sstream>

using namespace drogon;

namespace {

auto g_server_start_time = std::chrono::steady_clock::now();

HttpResponsePtr make_error_response(HttpStatusCode status, const std::string& code, const std::string& message) {
    Json::Value err;
    err["error"] = true;
    err["code"] = code;
    err["message"] = message;
    auto resp = HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(status);
    return resp;
}

const char* DEMO_HTML = R"rawhtml(<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Sherpa Server — TTS & STT Demo</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #0f172a;
      --card-bg: #1e293b;
      --card-border: #334155;
      --primary: #3b82f6;
      --primary-hover: #2563eb;
      --accent: #10b981;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --danger: #ef4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Inter', sans-serif;
      background: var(--bg);
      color: var(--text);
      line-height: 1.6;
      padding: 24px;
      min-height: 100vh;
    }
    .container { max-width: 1000px; margin: 0 auto; }
    header {
      margin-bottom: 28px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      border-bottom: 1px solid var(--card-border);
      padding-bottom: 20px;
      flex-wrap: wrap;
      gap: 12px;
    }
    h1 { font-size: 1.8rem; font-weight: 700; color: #fff; display: flex; align-items: center; gap: 10px; }
    .badge {
      display: inline-block;
      padding: 4px 10px;
      border-radius: 9999px;
      font-size: 0.75rem;
      font-weight: 600;
      text-transform: uppercase;
    }
    .badge-loaded { background: #065f46; color: #34d399; border: 1px solid #059669; }
    .badge-standby { background: #78350f; color: #fde68a; border: 1px solid #d97706; }
    .status-bar {
      display: flex;
      gap: 16px;
      background: var(--card-bg);
      padding: 12px 18px;
      border-radius: 8px;
      border: 1px solid var(--card-border);
      font-size: 0.85rem;
      align-items: center;
      flex-wrap: wrap;
    }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 24px; margin-top: 24px; }
    @media (max-width: 768px) { .grid { grid-template-columns: 1fr; } }
    .card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 22px;
      display: flex;
      flex-direction: column;
      gap: 16px;
    }
    .card-title {
      font-size: 1.25rem;
      font-weight: 600;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    label { font-size: 0.85rem; font-weight: 500; color: var(--text-muted); display: block; margin-bottom: 6px; }
    textarea, input[type="text"], select {
      width: 100%;
      background: #0f172a;
      border: 1px solid var(--card-border);
      border-radius: 8px;
      color: var(--text);
      padding: 10px 12px;
      font-size: 0.95rem;
      outline: none;
      transition: border-color 0.2s;
    }
    textarea:focus, select:focus { border-color: var(--primary); }
    textarea { resize: vertical; min-height: 100px; }
    .controls-row { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
    .slider-container { display: flex; flex-direction: column; }
    .slider-header { display: flex; justify-content: space-between; font-size: 0.85rem; color: var(--text-muted); margin-bottom: 4px; }
    input[type="range"] { accent-color: var(--primary); }
    button {
      background: var(--primary);
      color: white;
      border: none;
      padding: 12px 20px;
      border-radius: 8px;
      font-size: 0.95rem;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.2s, opacity 0.2s;
      display: flex;
      justify-content: center;
      align-items: center;
      gap: 8px;
    }
    button:hover:not(:disabled) { background: var(--primary-hover); }
    button:disabled { opacity: 0.6; cursor: not-allowed; }
    .result-box {
      background: #0f172a;
      border: 1px solid var(--card-border);
      border-radius: 8px;
      padding: 14px;
      margin-top: 8px;
      font-size: 0.9rem;
      min-height: 50px;
      word-break: break-word;
    }
    audio { width: 100%; margin-top: 8px; outline: none; border-radius: 8px; }
    .sample-prompts { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 4px; }
    .sample-tag {
      font-size: 0.75rem;
      background: #334155;
      padding: 4px 8px;
      border-radius: 4px;
      cursor: pointer;
      color: var(--text-muted);
    }
    .sample-tag:hover { color: #fff; background: #475569; }
    .file-dropzone {
      border: 2px dashed var(--card-border);
      border-radius: 8px;
      padding: 24px;
      text-align: center;
      cursor: pointer;
      transition: border-color 0.2s;
      background: #0f172a;
    }
    .file-dropzone:hover { border-color: var(--primary); }
    .tokens-tag {
      display: inline-block;
      background: #334155;
      color: #93c5fd;
      padding: 2px 6px;
      border-radius: 4px;
      font-size: 0.75rem;
      margin: 2px;
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div>
        <h1>⚡ Sherpa Server</h1>
        <div style="font-size:0.85rem; color:var(--text-muted);">High-performance C++20 TTS & STT Server (Drogon + sherpa-onnx)</div>
      </div>
      <div class="status-bar" id="statusBar">
        <span>Server: <strong style="color:var(--accent);">Online</strong></span>
        <span>TTS: <span class="badge badge-standby" id="ttsBadge">Standby</span></span>
        <span>STT: <span class="badge badge-standby" id="sttBadge">Standby</span></span>
      </div>
    </header>

    <div class="grid">
      <!-- TTS CARD -->
      <div class="card">
        <div class="card-title">
          <span>🔊 Text-to-Speech (TTS)</span>
          <span style="font-size:0.8rem; color:var(--text-muted);">Supertonic 3</span>
        </div>
        <div>
          <label>Văn bản cần đọc (Tiếng Việt / Đa ngôn ngữ):</label>
          <textarea id="ttsText" placeholder="Nhập văn bản vào đây...">Xin chào các bạn, đây là dịch vụ tổng hợp giọng nói sherpa-server chạy trên C++20 cực nhanh.</textarea>
          <div class="sample-prompts">
            <span class="sample-tag" onclick="setTts('Xin chào thế giới!')">Chào ngắn</span>
            <span class="sample-tag" onclick="setTts('Dự án sherpa-server triển khai trên nền tảng Drogon C++20 và Sherpa-onnx.')">Dự án</span>
            <span class="sample-tag" onclick="setTts('Hôm nay thời tiết rất đẹp, chúc bạn một ngày làm việc hiệu quả và tràn đầy năng lượng!')">Chúc buổi sáng</span>
          </div>
        </div>
        <div class="controls-row">
          <div>
            <label>Giọng đọc (Voice ID):</label>
            <select id="ttsSid">
              <option value="0">Giọng 0 (Mặc định)</option>
              <option value="1">Giọng 1</option>
              <option value="2">Giọng 2</option>
              <option value="3">Giọng 3</option>
              <option value="4">Giọng 4</option>
              <option value="5">Giọng 5</option>
            </select>
          </div>
          <div class="slider-container">
            <div class="slider-header">
              <span>Tốc độ:</span>
              <span id="speedVal">1.0x</span>
            </div>
            <input type="range" id="ttsSpeed" min="0.6" max="1.8" step="0.1" value="1.0" oninput="document.getElementById('speedVal').innerText = this.value + 'x'">
          </div>
        </div>
        <div class="controls-row">
          <div class="slider-container">
            <div class="slider-header">
              <span>Số bước flow (Steps):</span>
              <span id="stepsVal">5</span>
            </div>
            <input type="range" id="ttsSteps" min="1" max="10" step="1" value="5" oninput="document.getElementById('stepsVal').innerText = this.value">
          </div>
          <div class="slider-container">
            <div class="slider-header">
              <span>Khoảng lặng (Silence):</span>
              <span id="silenceVal">0.2</span>
            </div>
            <input type="range" id="ttsSilence" min="0.0" max="1.0" step="0.05" value="0.2" oninput="document.getElementById('silenceVal').innerText = this.value">
          </div>
        </div>
        <button id="ttsBtn" onclick="runTts()"><span>Phát âm (Synthesize)</span></button>
        <div id="ttsOutput" style="display:none;">
          <audio id="ttsAudio" controls></audio>
          <div id="ttsStats" style="font-size:0.8rem; color:var(--text-muted); margin-top:4px;"></div>
        </div>
      </div>

      <!-- STT CARD -->
      <div class="card">
        <div class="card-title">
          <span>🎙️ Speech-to-Text (STT)</span>
          <span style="font-size:0.8rem; color:var(--text-muted);">Zipformer 30M (Tiếng Việt)</span>
        </div>
        <div>
          <label>File âm thanh WAV (Mono 16kHz):</label>
          <div class="file-dropzone" onclick="document.getElementById('sttFile').click()">
            <input type="file" id="sttFile" accept=".wav" style="display:none" onchange="onFileSelected(this)">
            <span id="dropText">📁 Nhấn để chọn file WAV hoặc kéo thả vào đây</span>
          </div>
        </div>
        <button id="sttBtn" onclick="runStt()" disabled><span>Nhận diện (Transcribe)</span></button>
        <div>
          <label>Kết quả phiên âm:</label>
          <div class="result-box" id="sttResult">Chưa có kết quả.</div>
          <div id="sttTokens" style="margin-top:8px;"></div>
        </div>
      </div>
    </div>
  </div>

  <script>
    function setTts(t) { document.getElementById('ttsText').value = t; }

    async function checkModels() {
      try {
        const res = await fetch('/api/v1/models');
        const data = await res.json();
        const ttsB = document.getElementById('ttsBadge');
        if (data.tts && data.tts.status === 'loaded') {
          ttsB.innerText = 'Loaded in RAM';
          ttsB.className = 'badge badge-loaded';
        } else {
          ttsB.innerText = 'Standby';
          ttsB.className = 'badge badge-standby';
        }
        const sttB = document.getElementById('sttBadge');
        if (data.stt && data.stt.status === 'loaded') {
          sttB.innerText = 'Loaded in RAM';
          sttB.className = 'badge badge-loaded';
        } else {
          sttB.innerText = 'Standby';
          sttB.className = 'badge badge-standby';
        }
      } catch (e) {
        console.error(e);
      }
    }

    async function runTts() {
      const text = document.getElementById('ttsText').value.trim();
      if (!text) return alert('Vui lòng nhập văn bản');
      const btn = document.getElementById('ttsBtn');
      btn.disabled = true;
      btn.innerText = 'Đang xử lý...';
      const sid = parseInt(document.getElementById('ttsSid').value);
      const speed = parseFloat(document.getElementById('ttsSpeed').value);
      const num_steps = parseInt(document.getElementById('ttsSteps').value);
      const silence_scale = parseFloat(document.getElementById('ttsSilence').value);

      try {
        const t0 = performance.now();
        const res = await fetch('/api/v1/tts', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ text, sid, speed, num_steps, silence_scale })
        });
        if (!res.ok) {
          const err = await res.json();
          alert('Lỗi: ' + (err.message || 'Không thể tổng hợp giọng nói'));
          return;
        }
        const blob = await res.blob();
        const url = URL.createObjectURL(blob);
        const audio = document.getElementById('ttsAudio');
        audio.src = url;
        audio.play();
        document.getElementById('ttsOutput').style.display = 'block';
        const dur = (performance.now() - t0).toFixed(0);
        const serverDur = res.headers.get('X-Audio-Duration') || '';
        document.getElementById('ttsStats').innerText = `Thời gian xử lý: ${dur}ms | Độ dài âm thanh: ${parseFloat(serverDur).toFixed(2)}s`;
        checkModels();
      } catch (e) {
        alert('Lỗi kết nối server: ' + e.message);
      } finally {
        btn.disabled = false;
        btn.innerText = 'Phát âm (Synthesize)';
      }
    }

    let selectedFile = null;
    function onFileSelected(input) {
      if (input.files && input.files[0]) {
        selectedFile = input.files[0];
        document.getElementById('dropText').innerText = '🎵 Đã chọn: ' + selectedFile.name + ' (' + (selectedFile.size/1024).toFixed(1) + ' KB)';
        document.getElementById('sttBtn').disabled = false;
      }
    }

    async function runStt() {
      if (!selectedFile) return;
      const btn = document.getElementById('sttBtn');
      btn.disabled = true;
      btn.innerText = 'Đang nhận diện...';
      const resultBox = document.getElementById('sttResult');
      resultBox.innerText = 'Đang gửi âm thanh tới model Zipformer...';
      document.getElementById('sttTokens').innerHTML = '';

      try {
        const formData = new FormData();
        formData.append('file', selectedFile);
        const res = await fetch('/api/v1/stt', {
          method: 'POST',
          body: formData
        });
        const data = await res.json();
        if (!res.ok || data.error) {
          resultBox.innerHTML = '<span style="color:var(--danger)">Lỗi: ' + (data.message || 'Không thể nhận diện') + '</span>';
          return;
        }
        resultBox.innerText = data.text || '(Không nhận diện được từ nào)';
        if (data.tokens && data.tokens.length > 0) {
          let tokensHtml = '<div style="font-size:0.8rem; color:var(--text-muted); margin-bottom:4px;">Tokens:</div>';
          data.tokens.forEach(tok => {
            tokensHtml += `<span class="tokens-tag">${tok}</span>`;
          });
          document.getElementById('sttTokens').innerHTML = tokensHtml;
        }
        checkModels();
      } catch (e) {
        resultBox.innerText = 'Lỗi kết nối: ' + e.message;
      } finally {
        btn.disabled = false;
        btn.innerText = 'Nhận diện (Transcribe)';
      }
    }

    checkModels();
    setInterval(checkModels, 5000);
  </script>
</body>
</html>
)rawhtml";

Task<HttpResponsePtr> handle_health(HttpRequestPtr req) {
    auto now = std::chrono::steady_clock::now();
    int64_t uptime = std::chrono::duration_cast<std::chrono::seconds>(now - g_server_start_time).count();

    Json::Value res;
    res["status"] = "ok";
    res["version"] = "1.0.0";
    res["uptime_seconds"] = static_cast<Json::Int64>(uptime);

    auto resp = HttpResponse::newHttpJsonResponse(res);
    co_return resp;
}

Task<HttpResponsePtr> handle_models(HttpRequestPtr req) {
    Json::Value res = SherpaEngine::instance().get_models_status();
    auto resp = HttpResponse::newHttpJsonResponse(res);
    co_return resp;
}

Task<HttpResponsePtr> handle_tts_post(HttpRequestPtr req) {
    auto json = req->getJsonObject();
    if (!json) {
        co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "Request body must be valid JSON.");
    }

    if (!json->isMember("text") || !(*json)["text"].isString()) {
        co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "Field 'text' is required and must be a string.");
    }

    std::string text = (*json)["text"].asString();
    if (text.empty() || text.length() > 2000) {
        co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "Field 'text' must not be empty and cannot exceed 2000 characters.");
    }

    int32_t sid = json->get("sid", 0).asInt();
    if (sid < 0) {
        co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "Field 'sid' must be non-negative.");
    }

    float speed = json->get("speed", 1.0f).asFloat();
    if (speed < 0.2f || speed > 3.0f) {
        co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "Field 'speed' must be between 0.2 and 3.0.");
    }

    float silence_scale = json->get("silence_scale", 0.2f).asFloat();
    if (silence_scale < 0.0f || silence_scale > 2.0f) {
        co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "Field 'silence_scale' must be between 0.0 and 2.0.");
    }

    int32_t num_steps = json->get("num_steps", 5).asInt();
    if (num_steps < 1 || num_steps > 30) {
        co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "Field 'num_steps' must be between 1 and 30.");
    }

    TtsResult result = SherpaEngine::instance().synthesize(text, sid, speed, silence_scale, num_steps);
    if (!result.success) {
        HttpStatusCode sc = (result.error_code == "MODEL_LOAD_FAILED") ? k503ServiceUnavailable : k500InternalServerError;
        co_return make_error_response(sc, result.error_code, result.error_message);
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_CUSTOM);
    resp->addHeader("Content-Type", "audio/wav");
    resp->addHeader("Content-Disposition", "inline; filename=\"speech.wav\"");
    resp->addHeader("X-Audio-Sample-Rate", std::to_string(result.sample_rate));
    resp->addHeader("X-Audio-Duration", std::to_string(result.duration));
    resp->setBody(std::string(reinterpret_cast<const char*>(result.wav_data.data()), result.wav_data.size()));

    co_return resp;
}

Task<HttpResponsePtr> handle_tts_get(HttpRequestPtr req) {
    const auto& params = req->getParameters();
    auto it_text = params.find("text");
    if (it_text == params.end() || it_text->second.empty()) {
        co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "Query parameter 'text' is required.");
    }

    std::string text = it_text->second;
    if (text.length() > 2000) {
        co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "Parameter 'text' cannot exceed 2000 characters.");
    }

    int32_t sid = 0;
    auto it_sid = params.find("sid");
    if (it_sid != params.end()) {
        try { sid = std::stoi(it_sid->second); } catch (...) {}
    }

    float speed = 1.0f;
    auto it_speed = params.find("speed");
    if (it_speed != params.end()) {
        try { speed = std::stof(it_speed->second); } catch (...) {}
    }

    float silence_scale = 0.2f;
    auto it_silence = params.find("silence_scale");
    if (it_silence != params.end()) {
        try { silence_scale = std::stof(it_silence->second); } catch (...) {}
    }

    int32_t num_steps = 5;
    auto it_steps = params.find("num_steps");
    if (it_steps != params.end()) {
        try { num_steps = std::stoi(it_steps->second); } catch (...) {}
    }

    TtsResult result = SherpaEngine::instance().synthesize(text, sid, speed, silence_scale, num_steps);
    if (!result.success) {
        HttpStatusCode sc = (result.error_code == "MODEL_LOAD_FAILED") ? k503ServiceUnavailable : k500InternalServerError;
        co_return make_error_response(sc, result.error_code, result.error_message);
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_CUSTOM);
    resp->addHeader("Content-Type", "audio/wav");
    resp->addHeader("Content-Disposition", "inline; filename=\"speech.wav\"");
    resp->addHeader("X-Audio-Sample-Rate", std::to_string(result.sample_rate));
    resp->addHeader("X-Audio-Duration", std::to_string(result.duration));
    resp->setBody(std::string(reinterpret_cast<const char*>(result.wav_data.data()), result.wav_data.size()));

    co_return resp;
}

Task<HttpResponsePtr> handle_stt_post(HttpRequestPtr req) {
    const char* audio_data = nullptr;
    size_t audio_len = 0;

    std::string content_type = req->getHeader("content-type");
    if (content_type.find("multipart/form-data") != std::string::npos) {
        MultiPartParser parser;
        if (parser.parse(req) == 0 && !parser.getFiles().empty()) {
            const auto& file = parser.getFiles().front();
            audio_data = file.fileData();
            audio_len = file.fileLength();
        } else {
            co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "Multipart form-data must include an audio file in field 'file'.");
        }
    } else {
        // Raw audio bytes in request body
        const auto& body = req->getBody();
        if (!body.empty()) {
            audio_data = body.data();
            audio_len = body.size();
        } else {
            co_return make_error_response(k400BadRequest, "INVALID_ARGUMENT", "No audio data received. Send multipart/form-data or binary audio body.");
        }
    }

    SttResult result = SherpaEngine::instance().recognize(audio_data, audio_len);
    if (!result.success) {
        HttpStatusCode sc = k500InternalServerError;
        if (result.error_code == "INVALID_AUDIO_FORMAT") sc = k400BadRequest;
        else if (result.error_code == "MODEL_LOAD_FAILED") sc = k503ServiceUnavailable;
        co_return make_error_response(sc, result.error_code, result.error_message);
    }

    Json::Value res;
    res["text"] = result.text;
    res["duration"] = result.duration;
    res["sample_rate"] = result.sample_rate;

    Json::Value tokens_arr(Json::arrayValue);
    for (const auto& tok : result.tokens) {
        tokens_arr.append(tok);
    }
    res["tokens"] = tokens_arr;

    Json::Value timestamps_arr(Json::arrayValue);
    for (float ts : result.timestamps) {
        timestamps_arr.append(ts);
    }
    res["timestamps"] = timestamps_arr;

    auto resp = HttpResponse::newHttpJsonResponse(res);
    co_return resp;
}

Task<HttpResponsePtr> handle_root(HttpRequestPtr req) {
    // Check if static/index.html exists on disk, otherwise serve embedded HTML
    std::ifstream f("static/index.html");
    std::string html_content;
    if (f.is_open()) {
        std::stringstream ss;
        ss << f.rdbuf();
        html_content = ss.str();
    } else {
        html_content = DEMO_HTML;
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setBody(std::move(html_content));
    co_return resp;
}

} // namespace

namespace controllers {

void register_routes(drogon::HttpAppFramework& app) {
    app.registerHandler("/", &handle_root, {Get});
    app.registerHandler("/health", &handle_health, {Get});
    app.registerHandler("/api/v1/health", &handle_health, {Get});
    app.registerHandler("/api/v1/models", &handle_models, {Get});
    app.registerHandler("/api/v1/tts", &handle_tts_post, {Post});
    app.registerHandler("/api/v1/tts", &handle_tts_get, {Get});
    app.registerHandler("/api/v1/stt", &handle_stt_post, {Post});
}

} // namespace controllers
