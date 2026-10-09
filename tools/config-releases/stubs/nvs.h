#pragma once
#include <cstddef>
typedef int nvs_handle_t;
typedef int esp_err_t;
#define ESP_OK 0
#define NVS_READONLY 0
inline esp_err_t nvs_open(const char *, int, nvs_handle_t *) { return -1; }
inline esp_err_t nvs_get_str(nvs_handle_t, const char *, char *, size_t *) { return -1; }
inline void nvs_close(nvs_handle_t) {}
