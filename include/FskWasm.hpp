/*
* WebAssembly implementations of the fsk-core FFI surface.
*
* The native build gets these symbols from fsk-core (Rust, staticlib). That
* library cannot be compiled for wasm32-emscripten (rusqlite bundled, reqwest
* native-tls, libloading, sysinfo), so under Emscripten we provide the same
* extern "C" entry points here:
*
*   - implemented for real: fetch, free_string, sql, system info, crypto
*   - stubbed (clear error): ffi (dlopen), websocket, http server, vm_run
*/
#pragma once

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <sqlite3.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

#include "json.hpp"

using json = nlohmann::json;

namespace fsk_wasm {

[[noreturn]] void unsupported(const char* what) {
    throw std::runtime_error(std::string(what) +
                             " n'est pas disponible dans le build WebAssembly.");
}

// ---------------------------------------------------------------- JS helpers

EM_JS(char*, wasm_fetch, (const char* url), {
    try {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', UTF8ToString(url), false);
        xhr.send(null);
        if (xhr.status < 200 || xhr.status >= 300) return 0;
        var body = xhr.responseText;
        var out = _malloc(body.length + 1);
        stringToUTF8(body, out, body.length + 1);
        return out;
    } catch (e) {
        return 0;
    }
});

EM_JS(char*, wasm_system_info, (), {
    var nav = typeof navigator !== 'undefined' ? navigator : {};
    var loc = typeof location !== 'undefined' ? location : {};
    var info = {
        os: 'web',
        arch: nav.platform || 'unknown',
        cpu_cores: nav.hardwareConcurrency || 1,
        hostname: loc.hostname || 'localhost',
        user_agent: nav.userAgent || 'unknown'
    };
    var text = JSON.stringify(info);
    var out = _malloc(text.length + 1);
    stringToUTF8(text, out, text.length + 1);
    return out;
});

EM_JS(char*, wasm_sha256, (const char* input), {
    function rotr(v, n) { return (v >>> n) | (v << (32 - n)); }
    var K = new Uint32Array([
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2]);
    var data = new TextEncoder().encode(UTF8ToString(input));
    var len = data.length;
    var padded = new Uint8Array((((len + 9 + 63) >> 6) << 6));
    padded.set(data);
    padded[len] = 0x80;
    var view = new DataView(padded.buffer);
    view.setUint32(padded.length - 8, Math.floor(len / 536870912), false);
    view.setUint32(padded.length - 4, (len * 8) >>> 0, false);

    var H = new Uint32Array([0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                             0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19]);
    var w = new Uint32Array(64);
    for (var off = 0; off < padded.length; off += 64) {
        for (var t = 0; t < 16; t++) w[t] = view.getUint32(off + t * 4, false);
        for (t = 16; t < 64; t++) {
            var x = w[t - 15], y = w[t - 2];
            var s0 = rotr(x, 7) ^ rotr(x, 18) ^ (x >>> 3);
            var s1 = rotr(y, 17) ^ rotr(y, 19) ^ (y >>> 10);
            w[t] = (w[t - 16] + s0 + w[t - 7] + s1) | 0;
        }
        var a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
        for (t = 0; t < 64; t++) {
            var S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            var ch = (e & f) ^ (~e & g);
            var t1 = (h + S1 + ch + K[t] + w[t]) | 0;
            var S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            var maj = (a & b) ^ (a & c) ^ (b & c);
            var t2 = (S0 + maj) | 0;
            h = g; g = f; f = e; e = (d + t1) | 0;
            d = c; c = b; b = a; a = (t1 + t2) | 0;
        }
        H[0] = (H[0] + a) | 0; H[1] = (H[1] + b) | 0; H[2] = (H[2] + c) | 0; H[3] = (H[3] + d) | 0;
        H[4] = (H[4] + e) | 0; H[5] = (H[5] + f) | 0; H[6] = (H[6] + g) | 0; H[7] = (H[7] + h) | 0;
    }
    var out = [];
    for (var i = 0; i < 8; i++) out.push(('00000000' + (H[i] >>> 0).toString(16)).slice(-8));
    var hex = out.join('');
    var res = _malloc(hex.length + 1);
    stringToUTF8(hex, res, hex.length + 1);
    return res;
});

EM_JS(char*, wasm_md5, (const char* input), {
    function add32(a, b) { return (a + b) & 0xFFFFFFFF; }
    function cmn(q, a, b, x, s, t) {
        a = add32(add32(a, q), add32(x, t));
        return add32((a << s) | (a >>> (32 - s)), b);
    }
    function ff(a, b, c, d, x, s, t) { return cmn((b & c) | (~b & d), a, b, x, s, t); }
    function gg(a, b, c, d, x, s, t) { return cmn((b & d) | (c & ~d), a, b, x, s, t); }
    function hh(a, b, c, d, x, s, t) { return cmn(b ^ c ^ d, a, b, x, s, t); }
    function ii(a, b, c, d, x, s, t) { return cmn(c ^ (b | ~d), a, b, x, s, t); }

    function cycle(x, k) {
        var a = x[0], b = x[1], c = x[2], d = x[3];
        a=ff(a,b,c,d,k[0],7,-680876936);  d=ff(d,a,b,c,k[1],12,-389564586); c=ff(c,d,a,b,k[2],17,606105819);  b=ff(b,c,d,a,k[3],22,-1044525330);
        a=ff(a,b,c,d,k[4],7,-176418897);  d=ff(d,a,b,c,k[5],12,1200080426); c=ff(c,d,a,b,k[6],17,-1473231341);b=ff(b,c,d,a,k[7],22,-45705983);
        a=ff(a,b,c,d,k[8],7,1770035416);  d=ff(d,a,b,c,k[9],12,-1958414417);c=ff(c,d,a,b,k[10],17,-42063);  b=ff(b,c,d,a,k[11],22,-1990404162);
        a=ff(a,b,c,d,k[12],7,1804603682); d=ff(d,a,b,c,k[13],12,-40341101); c=ff(c,d,a,b,k[14],17,-1502002290);b=ff(b,c,d,a,k[15],22,1236535329);
        a=gg(a,b,c,d,k[1],5,-165796510);   d=gg(d,a,b,c,k[6],9,-1069501632);c=gg(c,d,a,b,k[11],14,643717713);b=gg(b,c,d,a,k[0],20,-373897302);
        a=gg(a,b,c,d,k[5],5,-701558691);   d=gg(d,a,b,c,k[10],9,38016083);  c=gg(c,d,a,b,k[15],14,-660478335);b=gg(b,c,d,a,k[4],20,-405537848);
        a=gg(a,b,c,d,k[9],5,568446438);    d=gg(d,a,b,c,k[14],9,-1019803690);c=gg(c,d,a,b,k[3],14,-187363961);b=gg(b,c,d,a,k[8],20,1163531501);
        a=gg(a,b,c,d,k[13],5,-1444681467); d=gg(d,a,b,c,k[2],9,-51403784);  c=gg(c,d,a,b,k[7],14,1735328473);b=gg(b,c,d,a,k[12],20,-1926607734);
        a=hh(a,b,c,d,k[5],4,-378558);      d=hh(d,a,b,c,k[8],11,-2022574463);c=hh(c,d,a,b,k[11],16,1839030562);b=hh(b,c,d,a,k[14],23,-35309556);
        a=hh(a,b,c,d,k[1],4,-1530992060);  d=hh(d,a,b,c,k[4],11,1272893353);c=hh(c,d,a,b,k[7],16,-155497632);b=hh(b,c,d,a,k[10],23,-1094730640);
        a=hh(a,b,c,d,k[13],4,681279174);   d=hh(d,a,b,c,k[0],11,-358537222);c=hh(c,d,a,b,k[3],16,-722521979);b=hh(b,c,d,a,k[6],23,76029189);
        a=hh(a,b,c,d,k[9],4,-640364487);   d=hh(d,a,b,c,k[12],11,-421815835);c=hh(c,d,a,b,k[15],16,530742520);b=hh(b,c,d,a,k[2],23,-995338651);
        a=ii(a,b,c,d,k[0],6,-198630844);   d=ii(d,a,b,c,k[7],10,1126891415); c=ii(c,d,a,b,k[14],15,-1416354905);b=ii(b,c,d,a,k[5],21,-57434055);
        a=ii(a,b,c,d,k[12],6,1700485571);  d=ii(d,a,b,c,k[3],10,-1894986606);c=ii(c,d,a,b,k[10],15,-1051523); b=ii(b,c,d,a,k[1],21,-2054922799);
        a=ii(a,b,c,d,k[8],6,1873313359);   d=ii(d,a,b,c,k[15],10,-30611744); c=ii(c,d,a,b,k[6],15,-1560198380);b=ii(b,c,d,a,k[13],21,1309151649);
        a=ii(a,b,c,d,k[4],6,-145523070);   d=ii(d,a,b,c,k[11],10,-1120210379);c=ii(c,d,a,b,k[2],15,718787259);b=ii(b,c,d,a,k[9],21,-343485551);
        x[0]=add32(a,x[0]); x[1]=add32(b,x[1]); x[2]=add32(c,x[2]); x[3]=add32(d,x[3]);
    }

    function wordsToMd5(bytes) {
        var n = bytes.length;
        var state = [1732584193, -271733879, -1732584194, 271733878];
        var i = 0;
        for (; i + 64 <= n; i += 64) {
            var k = new Array(16);
            for (var j = 0; j < 16; j++) {
                k[j] = bytes[i + j * 4] | (bytes[i + j * 4 + 1] << 8) |
                       (bytes[i + j * 4 + 2] << 16) | (bytes[i + j * 4 + 3] << 24);
            }
            cycle(state, k);
        }
        var tail = new Uint8Array(n - i + 72);
        tail.set(bytes.subarray(i));
        tail[n - i] = 0x80;
        var bits = n * 8;
        var t = tail.length;
        for (var b = 0; b < 8; b++) tail[t - 8 + b] = (bits >>> (8 * b)) & 0xFF;
        for (var off = 0; off < t; off += 64) {
            var kk = new Array(16);
            for (var m = 0; m < 16; m++) {
                kk[m] = tail[off + m * 4] | (tail[off + m * 4 + 1] << 8) |
                        (tail[off + m * 4 + 2] << 16) | (tail[off + m * 4 + 3] << 24);
            }
            cycle(state, kk);
        }
        var hex = '';
        for (var s = 0; s < 4; s++) {
            hex += ('00000000' + (state[s] >>> 0).toString(16)).slice(-8);
        }
        return hex;
    }

    var hex = wordsToMd5(new TextEncoder().encode(UTF8ToString(input)));
    var res = _malloc(hex.length + 1);
    stringToUTF8(hex, res, hex.length + 1);
    return res;
});

char* dup(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

// ---------------------------------------------------------------------- SQL

std::map<uint32_t, sqlite3*>& connections() {
    static std::map<uint32_t, sqlite3*> db;
    return db;
}

std::mutex& connectionsMutex() {
    static std::mutex m;
    return m;
}

uint32_t nextDbId() {
    static uint32_t id = 0;
    return id++;
}

}  // namespace fsk_wasm

// ------------------------------------------------------------------- exports

extern "C" {

char* fsk_fetch_blocking(const char* url) { return fsk_wasm::wasm_fetch(url); }

void fsk_free_string(char* s) { std::free(s); }

uint32_t fsk_sql_open(const char* path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }
    std::lock_guard<std::mutex> lock(fsk_wasm::connectionsMutex());
    uint32_t id = fsk_wasm::nextDbId();
    fsk_wasm::connections()[id] = db;
    return id;
}

char* fsk_sql_query(uint32_t db_id, const char* query) {
    std::lock_guard<std::mutex> lock(fsk_wasm::connectionsMutex());
    auto it = fsk_wasm::connections().find(db_id);
    if (it == fsk_wasm::connections().end()) {
        return fsk_wasm::dup("Error: Invalid DB ID");
    }

    sqlite3* db = it->second;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        return fsk_wasm::dup(std::string("Error: ") + sqlite3_errmsg(db));
    }

    json rows = json::array();
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        json row = json::object();
        int cols = sqlite3_column_count(stmt);
        for (int i = 0; i < cols; i++) {
            const char* name = sqlite3_column_name(stmt, i);
            switch (sqlite3_column_type(stmt, i)) {
                case SQLITE_INTEGER:
                    row[name] = (double)sqlite3_column_int64(stmt, i);
                    break;
                case SQLITE_FLOAT:
                    row[name] = sqlite3_column_double(stmt, i);
                    break;
                case SQLITE_BLOB: {
                    const unsigned char* blob =
                        static_cast<const unsigned char*>(sqlite3_column_blob(stmt, i));
                    int n = sqlite3_column_bytes(stmt, i);
                    std::string hex;
                    static const char* digits = "0123456789abcdef";
                    for (int b = 0; b < n; b++) {
                        hex += digits[blob[b] >> 4];
                        hex += digits[blob[b] & 0xF];
                    }
                    row[name] = hex;
                    break;
                }
                case SQLITE_NULL:
                    row[name] = nullptr;
                    break;
                default: {
                    const unsigned char* txt = sqlite3_column_text(stmt, i);
                    row[name] = txt ? reinterpret_cast<const char*>(txt) : "";
                    break;
                }
            }
        }
        rows.push_back(row);
    }
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return fsk_wasm::dup(std::string("Error: ") + sqlite3_errmsg(db));
    }
    return fsk_wasm::dup(rows.dump());
}

char* fsk_system_get_info() { return fsk_wasm::wasm_system_info(); }

char* fsk_crypto_sha256(const char* input) { return fsk_wasm::wasm_sha256(input); }

char* fsk_crypto_md5(const char* input) { return fsk_wasm::wasm_md5(input); }

// --- not portable to a browser ---

void fsk_start_server(uint16_t, void*, void*) { fsk_wasm::unsupported("FSK.startServer"); }
void fsk_http_respond(uint64_t, uint16_t, const char*) { fsk_wasm::unsupported("request.send"); }
void fsk_ws_listen(uint16_t, void (*)(uint32_t, const char*, void*), void*) {
    fsk_wasm::unsupported("WS.listen");
}
void fsk_ws_send(uint32_t, const char*) { fsk_wasm::unsupported("WS.send"); }
void fsk_vm_run(const uint8_t*, size_t, const char*) { fsk_wasm::unsupported("VM"); }

uint64_t fsk_ffi_open(const char*) { fsk_wasm::unsupported("FFI (chargement de .so)"); }
int32_t fsk_ffi_call_void_string(uint64_t, const char*, const char*) { fsk_wasm::unsupported("FFI"); }
int32_t fsk_ffi_call_int_int(uint64_t, const char*, int32_t) { fsk_wasm::unsupported("FFI"); }
void fsk_ffi_call_void(uint64_t, const char*) { fsk_wasm::unsupported("FFI"); }
bool fsk_ffi_call_bool(uint64_t, const char*) { fsk_wasm::unsupported("FFI"); }
void fsk_ffi_call_void_int_int_string(uint64_t, const char*, int32_t, int32_t, const char*) { fsk_wasm::unsupported("FFI"); }
void fsk_ffi_call_void_int_int(uint64_t, const char*, int32_t, int32_t) { fsk_wasm::unsupported("FFI"); }
void fsk_ffi_call_void_4int(uint64_t, const char*, int32_t, int32_t, int32_t, int32_t) { fsk_wasm::unsupported("FFI"); }
void fsk_ffi_call_void_5int(uint64_t, const char*, int32_t, int32_t, int32_t, int32_t, int32_t) { fsk_wasm::unsupported("FFI"); }
void fsk_ffi_call_void_6int(uint64_t, const char*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t) { fsk_wasm::unsupported("FFI"); }
void fsk_ffi_call_void_8int(uint64_t, const char*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t) { fsk_wasm::unsupported("FFI"); }
void fsk_ffi_call_void_2int_float_int(uint64_t, const char*, int32_t, int32_t, float, int32_t) { fsk_wasm::unsupported("FFI"); }
void fsk_ffi_call_void_string_4int(uint64_t, const char*, const char*, int32_t, int32_t, int32_t, int32_t) { fsk_wasm::unsupported("FFI"); }

}  // extern "C"

#endif  // __EMSCRIPTEN__
