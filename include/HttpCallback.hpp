#pragma once
#include <cstdint>

extern "C" {
    void fsk_on_http_request(uint64_t req_id, const char* method, const char* path, const char* body, void* context);
    void fsk_on_ws_message(uint32_t ws_id, const char* message, void* context);
}
