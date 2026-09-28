sherpa-onnx original is kind of suck because it is lack server for devs who familar with spam http services instead of linking using C++ ABI
luckily, sherpa-onnx prebuilt already included a simple sdk (.lib/.dll + headers).
this project will implement a thin layer of http server using C api header + its .dll (C api, not C++)

tech stack:
 - c++20, msys2 ucrt64 g++ compiler (cmake, ninja)
 - drogon, co_routine centric (hạn chế dùng callback nhất có thể trừ khi drogon bắt buộc)
 - dựng trên nền của sherpa-onnx-v1.13.8-win-x64-shared-MT-Release (có thể tự tải xuống bằng cmake)
 - html/css/js nhẹ để demo server

ở bản đầu tiên:
 - có endpoint api cho tts (supertonic), stt (zipformer) cơ bản (vẫn chỉnh được các tham số ngoại trừ các file model). chạy được ngay cho model sherpa-onnx-supertonic-3-tts-int8-2026-05-11 và model sherpa-onnx-zipformer-vi-30M-int8-2026-02-09
 - workflow: server đọc file `models.ini` lúc khởi động, lazy load, nếu được gọi thì infer rồi trả audio, xong thì giữ trong ram chứ ko unload

server:
 - 0.0.0.0:5555