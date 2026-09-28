# HTTP API Contract — Sherpa Server

- **Host mặc định:** `http://127.0.0.1:5555`
- **Framework:** Drogon (C++20, Coroutine-centric)
- **Backend:** `sherpa-onnx` C API (Inference on ONNX Runtime)
- **Cơ chế nạp model:** Lazy load khi có request đầu tiên tới endpoint tương ứng, sau đó giữ luôn trên RAM (không unload).

---

## 1. Tổng quan các Endpoints

| Phương thức | Đường dẫn | Chức năng | Định dạng Request | Định dạng Response |
| :--- | :--- | :--- | :--- | :--- |
| `GET` | `/health` | Kiểm tra tình trạng server | Không | `application/json` |
| `GET` | `/api/v1/models` | Trạng thái các model (đã nạp vào RAM hay chưa) | Không | `application/json` |
| `POST` | `/api/v1/tts` | Tổng hợp giọng nói (Text-to-Speech) | `application/json` | `audio/wav` |
| `GET` | `/api/v1/tts` | Tổng hợp giọng nói qua URL (dùng trực tiếp cho thẻ `<audio>`) | Query parameters | `audio/wav` |
| `POST` | `/api/v1/stt` | Nhận diện giọng nói (Speech-to-Text) | `multipart/form-data` hoặc `audio/wav` | `application/json` |

---

## 2. Chi tiết Endpoints

### 2.1. System Health: `GET /health`
Kiểm tra server có đang hoạt động bình thường hay không.

#### Response
- **Status:** `200 OK`
- **Content-Type:** `application/json`
```json
{
  "status": "ok",
  "version": "1.0.0",
  "uptime_seconds": 120
}
```

---

### 2.2. Model Status: `GET /api/v1/models`
Xem trạng thái hiện tại của các model được cấu hình trong `models.ini`.

#### Response
- **Status:** `200 OK`
- **Content-Type:** `application/json`
```json
{
  "tts": {
    "model_name": "sherpa-onnx-supertonic-3-tts-int8-2026-05-11",
    "type": "supertonic",
    "status": "loaded", // "standby" (chưa load) hoặc "loaded" (đã ở trong RAM)
    "sample_rate": 44100,
    "num_speakers": 10
  },
  "stt": {
    "model_name": "sherpa-onnx-zipformer-vi-30M-int8-2026-02-09",
    "type": "transducer_zipformer",
    "status": "standby",
    "sample_rate": 16000
  }
}
```

---

### 2.3. Text-to-Speech (TTS): `POST /api/v1/tts`
Sinh file âm thanh từ văn bản đầu vào sử dụng model Supertonic 3.

#### Request Headers
- `Content-Type: application/json`

#### Request Body
```json
{
  "text": "Xin chào, đây là hệ thống tổng hợp giọng nói sherpa-server.",
  "sid": 0,
  "speed": 1.0,
  "silence_scale": 0.2,
  "num_steps": 5
}
```

#### Bảng tham số
| Trường | Kiểu dữ liệu | Bắt buộc | Mặc định | Giới hạn | Mô tả |
| :--- | :--- | :---: | :---: | :---: | :--- |
| `text` | `string` | **Có** | - | 1 - 2000 ký tự | Văn bản cần đọc. |
| `sid` | `integer` | Không | `0` | $\ge 0$ | Speaker ID / Voice style index. |
| `speed` | `float` | Không | `1.0` | `0.5` - `2.0` | Tốc độ phát âm (1.0 là bình thường, >1 nhanh hơn, <1 chậm hơn). |
| `silence_scale` | `float` | Không | `0.2` | `0.0` - `2.0` | Khoảng lặng giữa các câu ngắt nghỉ. |
| `num_steps` | `integer` | Không | `5` | `1` - `20` | Số bước khử nhiễu (flow-matching). Bước cao hơn cho âm thanh mượt hơn nhưng tốn CPU hơn. |

#### Response Thành Công
- **Status:** `200 OK`
- **Content-Type:** `audio/wav`
- **Headers bổ sung:**
  - `Content-Disposition: inline; filename="speech.wav"`
  - `X-Audio-Sample-Rate: 44100`
  - `X-Audio-Duration: 3.42` (giây)
- **Body:** Binary WAV audio (PCM 16-bit mono).

#### Ví dụ cURL
```bash
curl -X POST http://127.0.0.1:5555/api/v1/tts \
  -H "Content-Type: application/json" \
  -d '{"text": "Xin chào thế giới!", "speed": 1.0}' \
  --output output.wav
```

---

### 2.4. Text-to-Speech (TTS qua GET): `GET /api/v1/tts`
Hỗ trợ stream trực tiếp cho web frontend qua thẻ `<audio>` hoặc link tải nhanh mà không cần gửi POST JSON.

#### Query Parameters
- `text` (*bắt buộc*): URL-encoded string.
- `sid` (*tùy chọn*, mặc định `0`): Speaker ID.
- `speed` (*tùy chọn*, mặc định `1.0`): Tốc độ phát.
- `silence_scale` (*tùy chọn*, mặc định `0.2`).
- `num_steps` (*tùy chọn*, mặc định `5`).

#### Ví dụ sử dụng trong HTML / cURL
```html
<audio controls src="http://127.0.0.1:5555/api/v1/tts?text=Xin+ch%C3%A0o&speed=1.0"></audio>
```
```bash
curl "http://127.0.0.1:5555/api/v1/tts?text=Xin+chao" --output output.wav
```

---

### 2.5. Speech-to-Text (STT): `POST /api/v1/stt`
Chuyển đổi âm thanh tiếng Việt thành văn bản sử dụng model Zipformer 30M.

Hỗ trợ 2 hình thức gửi audio:
1. **Multipart Form-Data** (khuyên dùng khi upload từ form frontend hoặc curl file).
2. **Raw Binary WAV** (`Content-Type: audio/wav`).

#### Cách 1: `multipart/form-data`
- **Form fields:**
  - `file` (*bắt buộc*): File âm thanh `.wav` (PCM 16-bit, khuyến nghị 16kHz).
  - `decoding_method` (*tùy chọn*): `"greedy_search"` (mặc định) hoặc `"modified_beam_search"`.

##### Ví dụ cURL:
```bash
curl -X POST http://127.0.0.1:5555/api/v1/stt \
  -F "file=@models/sherpa-onnx-zipformer-vi-30M-int8-2026-02-09/test_wavs/0.wav"
```

#### Cách 2: Raw Binary `Content-Type: audio/wav`
- Gửi trực tiếp toàn bộ dữ liệu byte của file WAV trong HTTP request body.
```bash
curl -X POST http://127.0.0.1:5555/api/v1/stt \
  -H "Content-Type: audio/wav" \
  --data-binary "@models/sherpa-onnx-zipformer-vi-30M-int8-2026-02-09/test_wavs/0.wav"
```

#### Response Thành Công
- **Status:** `200 OK`
- **Content-Type:** `application/json`
```json
{
  "text": "xin chào các bạn đã đến với kênh",
  "duration": 3.74,
  "sample_rate": 16000,
  "tokens": ["xin", "chào", "các", "bạn", "đã", "đến", "với", "kênh"],
  "timestamps": [0.42, 0.78, 1.15, 1.38, 1.62, 1.89, 2.14, 2.55]
}
```

---

## 3. Định dạng lỗi chuẩn (Standard Error Format)

Tất cả các lỗi (4xx, 5xx) trả về JSON với cấu trúc đồng nhất:

```json
{
  "error": true,
  "code": "INVALID_ARGUMENT",
  "message": "Trường 'text' không được để trống hoặc vượt quá 2000 ký tự."
}
```

### Bảng mã lỗi phổ biến
| HTTP Status | Error Code | Mô tả |
| :---: | :--- | :--- |
| `400` | `INVALID_ARGUMENT` | Thiếu tham số hoặc tham số vượt khoảng cho phép. |
| `400` | `INVALID_AUDIO_FORMAT` | File audio không đọc được hoặc không phải định dạng WAV hợp lệ. |
| `404` | `ENDPOINT_NOT_FOUND` | URL không tồn tại. |
| `415` | `UNSUPPORTED_MEDIA_TYPE` | `Content-Type` không được hỗ trợ. |
| `500` | `INTERNAL_INFERENCE_ERROR` | Lỗi xảy ra trong quá trình tính toán của ONNX / Sherpa engine. |
| `503` | `MODEL_LOAD_FAILED` | Không tìm thấy file model hoặc lỗi khi nạp model vào bộ nhớ RAM. |

---

## 4. Tương ứng cấu hình `models.ini`

Cấu trúc file `models.ini` mà server sẽ đọc khi khởi động:

```ini
[server]
host = 127.0.0.1
port = 5555
threads = 4

[tts]
enabled = true
model_type = supertonic
model_dir = models/sherpa-onnx-supertonic-3-tts-int8-2026-05-11
duration_predictor = duration_predictor.int8.onnx
text_encoder = text_encoder.int8.onnx
vector_estimator = vector_estimator.int8.onnx
vocoder = vocoder.int8.onnx
tts_json = tts.json
unicode_indexer = unicode_indexer.bin
voice_style = voice.bin
num_threads = 2
debug = 0

[stt]
enabled = true
model_type = zipformer
model_dir = models/sherpa-onnx-zipformer-vi-30M-int8-2026-02-09
encoder = encoder.int8.onnx
decoder = decoder.onnx
joiner = joiner.int8.onnx
tokens = tokens.txt
bpe_vocab = bpe.model
decoding_method = greedy_search
sample_rate = 16000
feature_dim = 80
num_threads = 2
debug = 0
```
