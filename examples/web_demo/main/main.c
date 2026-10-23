#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "driver/i2s_std.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "lingtts_engine.h"

#define TEXT_BYTES 4096
#define PCM_QUEUE_BYTES (64 * 1024)
#define DMA_DESC 8
#define DMA_FRAMES 320
static const char *TAG = "demo";
static lingtts_engine_t *engine;
static i2s_chan_handle_t tx;
static StreamBufferHandle_t pcm_queue;
static StaticStreamBuffer_t pcm_control;
static SemaphoreHandle_t lock;
static TaskHandle_t synth_task, play_task;
static char input[TEXT_BYTES + 1];
static lingtts_engine_mem_t engine_mem;
static struct {
    unsigned id;
    bool busy, synth_done;
    int error;
    int64_t start_us, first_us, compute_us, end_us;
    uint64_t samples;
    size_t min_internal, min_psram;
} run;
// Only the synthesis task touches callback_us.
static int64_t callback_us;
static TickType_t last_yield;
extern const char page[] asm("_binary_index_html_start");

static void sample_memory_locked(void)
{
    size_t a = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t b = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (a < run.min_internal) run.min_internal = a;
    if (b < run.min_psram) run.min_psram = b;
}
static void *psalloc(size_t n)
{
    void *p = heap_caps_aligned_alloc(16, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(p ? ESP_OK : ESP_ERR_NO_MEM);
    return p;
}
static int pcm_cb(const int16_t *pcm, size_t n, void *unused)
{
    (void)unused;
    int64_t start = esp_timer_get_time();
    xSemaphoreTake(lock, portMAX_DELAY);
    if (!run.first_us && n) run.first_us = start - run.start_us;
    run.samples += n;
    sample_memory_locked();
    xSemaphoreGive(lock);
    const uint8_t *src = (const uint8_t *)pcm;
    size_t left = n * sizeof(*pcm);
    while (left) {
        size_t sent = xStreamBufferSend(pcm_queue, src, left, pdMS_TO_TICKS(50));
        src += sent;
        left -= sent;
        xSemaphoreTake(lock, portMAX_DELAY);
        bool failed = run.error != 0;
        xSemaphoreGive(lock);
        if (failed) { callback_us += esp_timer_get_time() - start; return 1; }
    }
    // Give idle/Wi-Fi tasks time without disabling watchdogs. Excluded from compute time.
    if (xTaskGetTickCount() - last_yield >= pdMS_TO_TICKS(20)) {
        vTaskDelay(1);
        last_yield = xTaskGetTickCount();
    }
    callback_us += esp_timer_get_time() - start;
    return 0;
}
static void synth_worker(void *unused)
{
    (void)unused;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        callback_us = 0;
        last_yield = xTaskGetTickCount();
        xTaskNotifyGive(play_task);
        int64_t begin = esp_timer_get_time();
        int result = lingtts_engine_speak(engine, input, pcm_cb, NULL);
        int64_t compute = esp_timer_get_time() - begin - callback_us;
        xSemaphoreTake(lock, portMAX_DELAY);
        run.compute_us = compute;
        // LINGTTS_STOPPED means pcm_cb returned 1 after an audio failure; run.error is already set.
        if (result && result != LINGTTS_STOPPED && !run.error) run.error = result;
        if (!run.samples && !run.error) run.error = -101;
        run.synth_done = true;
        sample_memory_locked();
        xSemaphoreGive(lock);
    }
}
static bool write_audio(const void *data, size_t len)
{
    size_t written = 0;
    esp_err_t err = i2s_channel_write(tx, data, len, &written, 2000);
    return err == ESP_OK && written == len;
}
static void play_worker(void *unused)
{
    (void)unused;
    int16_t buf[DMA_FRAMES];
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        bool enabled = false, failed = false;
        // Prefill 320 ms to absorb synthesis block jitter, unless a short utterance is complete.
        while (xStreamBufferBytesAvailable(pcm_queue) < 10240) {
            xSemaphoreTake(lock, portMAX_DELAY);
            bool done = run.synth_done;
            sample_memory_locked();
            xSemaphoreGive(lock);
            if (done) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        for (;;) {
            size_t n = xStreamBufferReceive(pcm_queue, buf, sizeof(buf), pdMS_TO_TICKS(20));
            if (n && !failed) {
                for (size_t i = 0; i < n / 2; i++) buf[i] = (int32_t)buf[i] * CONFIG_DEMO_VOLUME / 100;
                if (!enabled) {
                    failed = i2s_channel_enable(tx) != ESP_OK;
                    enabled = !failed;
                }
                if (!failed) failed = !write_audio(buf, n);
            }
            xSemaphoreTake(lock, portMAX_DELAY);
            if (failed) run.error = -100;
            bool done = run.synth_done;
            sample_memory_locked();
            xSemaphoreGive(lock);
            if (done && xStreamBufferBytesAvailable(pcm_queue) == 0) break;
        }
        if (enabled) {
            // Push one full DMA ring of silence, then allow a full ring to drain.
            // The completion timestamp is conservative (includes this tail), not acoustic latency.
            memset(buf, 0, sizeof(buf));
            for (int i = 0; i < DMA_DESC; i++) if (!write_audio(buf, sizeof(buf))) failed = true;
            vTaskDelay(pdMS_TO_TICKS(DMA_DESC * DMA_FRAMES * 1000 / LINGTTS_SAMPLE_RATE + 20));
            i2s_channel_disable(tx);
        }
        xSemaphoreTake(lock, portMAX_DELAY);
        if (failed) run.error = -100;
        run.end_us = esp_timer_get_time();
        sample_memory_locked();
        run.busy = false;
        ESP_LOGI(TAG, "job=%u error=%d first_pcm=%.1fms audio=%.2fs compute=%.2fs", run.id, run.error,
                 run.first_us / 1000., run.samples / 16000., run.compute_us / 1e6);
        xSemaphoreGive(lock);
    }
}
static esp_err_t home(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t status(httpd_req_t *req)
{
    cJSON *j = cJSON_CreateObject();
    if (!j) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    xSemaphoreTake(lock, portMAX_DELAY);
    cJSON_AddStringToObject(j, "state", run.busy ? "playing" : !run.id ? "idle" : run.error ? "error" : "done");
    cJSON_AddNumberToObject(j, "id", run.id);
    cJSON_AddNumberToObject(j, "error", run.error);
    cJSON_AddNumberToObject(j, "first_pcm_ms", run.first_us / 1000.);
    cJSON_AddNumberToObject(j, "audio_s", run.samples / 16000.);
    cJSON_AddNumberToObject(j, "compute_ms", run.compute_us / 1000.);
    cJSON_AddNumberToObject(j, "rtf", run.samples ? run.compute_us * .016 / run.samples : 0);
    cJSON_AddNumberToObject(j, "elapsed_ms", run.id ? ((run.busy ? esp_timer_get_time() : run.end_us) - run.start_us) / 1000. : 0);
    cJSON_AddNumberToObject(j, "min_internal", run.id ? run.min_internal : 0);
    cJSON_AddNumberToObject(j, "min_psram", run.id ? run.min_psram : 0);
    xSemaphoreGive(lock);
    cJSON_AddNumberToObject(j, "free_internal", heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(j, "free_psram", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(j, "internal_total", heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(j, "psram_total", heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(j, "tts_arena_used", engine_mem.tts_arena);
    cJSON_AddNumberToObject(j, "demo_psram_reserved", engine_mem.copied + engine_mem.tts_arena + engine_mem.fe_arena + PCM_QUEUE_BYTES + 1);
    cJSON_AddBoolToObject(j, "licensed", lingtts_is_licensed(lingtts_engine_tts(engine)));
    uint32_t left = lingtts_eval_seconds_left();
    cJSON_AddBoolToObject(j, "unlimited", left == LINGTTS_UNLIMITED_SECONDS);
    if (left == LINGTTS_UNLIMITED_SECONDS) cJSON_AddNullToObject(j, "eval_left_s");
    else cJSON_AddNumberToObject(j, "eval_left_s", left);
    char *s = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    if (!s) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t ret = httpd_resp_sendstr(req, s);
    free(s);
    return ret;
}
// Strict UTF-8 validation; reject embedded NULs and whitespace-only submissions.
static bool valid_text(const unsigned char *s, size_t n)
{
    bool content = false;
    for (size_t i = 0; i < n;) {
        uint32_t c = s[i++];
        int more = 0;
        uint32_t minimum = 0;
        if (!c) return false;
        if (c >= 0xc2 && c <= 0xdf) { c &= 31; more = 1; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { c &= 15; more = 2; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { c &= 7; more = 3; minimum = 0x10000; }
        else if (c >= 0x80) return false;
        if (i + more > n) return false;
        for (int k = 0; k < more; k++) {
            unsigned char b = s[i++];
            if ((b & 0xc0) != 0x80) return false;
            c = (c << 6) | (b & 63);
        }
        if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
        if (c > 32 && c != 0x3000 && c != 0xa0) content = true;
    }
    return content;
}
static esp_err_t speak(httpd_req_t *req)
{
    if (req->content_len == 0 || req->content_len > TEXT_BYTES)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Text must be 1..4096 UTF-8 bytes");
    xSemaphoreTake(lock, portMAX_DELAY);
    bool busy = run.busy;
    xSemaphoreGive(lock);
    if (busy) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "Playback in progress");
    }
    // HTTP server serializes handlers; no producer is active while busy=false.
    size_t n = 0;
    while (n < req->content_len) {
        int r = httpd_req_recv(req, input + n, req->content_len - n);
        if (r <= 0) return httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Incomplete request");
        n += r;
    }
    input[n] = 0;
    if (!valid_text((const unsigned char *)input, n))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid or empty UTF-8 text");
    xStreamBufferReset(pcm_queue);
    xSemaphoreTake(lock, portMAX_DELAY);
    unsigned id = run.id + 1;
    memset(&run, 0, sizeof(run));
    run.id = id;
    run.busy = true;
    run.min_internal = run.min_psram = SIZE_MAX;
    run.start_us = esp_timer_get_time();
    sample_memory_locked();
    xSemaphoreGive(lock);
    xTaskNotifyGive(synth_task);
    httpd_resp_set_status(req, "202 Accepted");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"accepted\":true}");
}
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init()); // Never auto-erase existing board NVS.
    lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(lock ? ESP_OK : ESP_ERR_NO_MEM);
    // Model and dictionary come from the lingtts_model / lingtts_fe partitions and are copied to PSRAM;
    // Community build needs no device activation; legacy license partition is not auto-loaded.
    lingtts_engine_cfg_t cfg;
    lingtts_engine_default_cfg(&cfg);
    cfg.mem = LINGTTS_MEM_PSRAM;
    int r = lingtts_engine_create(&engine, &cfg);
    if (r != LINGTTS_OK) {
        ESP_LOGE(TAG, "lingtts_engine_create failed: %d (-6 = license invalid for this device)", r);
        ESP_ERROR_CHECK(ESP_FAIL);
    }
    lingtts_engine_get_mem(engine, &engine_mem);
    char id[40];
    if (lingtts_device_id(id, sizeof(id)) != LINGTTS_OK) snprintf(id, sizeof(id), "?");
    ESP_LOGI(TAG, "LingTTS %s, device %s, %s", lingtts_version(), id,
             lingtts_is_licensed(lingtts_engine_tts(engine)) ? "device certificate verified" : "community (no activation, unlimited)");
    pcm_queue = xStreamBufferCreateStatic(PCM_QUEUE_BYTES + 1, 1, psalloc(PCM_QUEUE_BYTES + 1), &pcm_control);
    ESP_ERROR_CHECK(pcm_queue ? ESP_OK : ESP_ERR_NO_MEM);
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    cc.dma_desc_num = DMA_DESC;
    cc.dma_frame_num = DMA_FRAMES;
    cc.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&cc, &tx, NULL));
    i2s_std_config_t sc = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(LINGTTS_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = CONFIG_DEMO_BCLK, .ws = CONFIG_DEMO_WS,
                     .dout = CONFIG_DEMO_DOUT, .din = I2S_GPIO_UNUSED},
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &sc));
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(play_worker, "audio", 4096, NULL, 6, &play_task, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(synth_worker, "tts", 16384, NULL, 3, &synth_task, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_ap() ? ESP_OK : ESP_FAIL);
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    const char *ssid = CONFIG_DEMO_AP_SSID, *password = CONFIG_DEMO_AP_PASSWORD;
    ESP_ERROR_CHECK(strlen(ssid) > 0 && strlen(ssid) <= 32 && strlen(password) >= 8 && strlen(password) <= 63 ? ESP_OK : ESP_ERR_INVALID_ARG);
    wifi_config_t wifi = {0};
    memcpy(wifi.ap.ssid, ssid, strlen(ssid));
    memcpy(wifi.ap.password, password, strlen(password));
    wifi.ap.ssid_len = strlen(ssid);
    wifi.ap.channel = 6;
    wifi.ap.max_connection = 3;
    wifi.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi));
    ESP_ERROR_CHECK(esp_wifi_start());
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.stack_size = 8192;
    hc.lru_purge_enable = true;
    httpd_handle_t server;
    ESP_ERROR_CHECK(httpd_start(&server, &hc));
    httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = home},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status},
        {.uri = "/api/speak", .method = HTTP_POST, .handler = speak},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) ESP_ERROR_CHECK(httpd_register_uri_handler(server, &routes[i]));
    ESP_LOGI(TAG, "Ready: connect to %s, open http://192.168.4.1", ssid);
}
