/*
 * LingTTS 最小例子：创建引擎，几句文本依次合成并经 I2S（MAX98357A 一类功放）播放，最后打印耗时和内存。
 * 资源分区 lingtts_model / lingtts_fe 由 idf.py flash 一起烧录（见 CMakeLists.txt 和 partitions.csv）。
 */
#include <stdio.h>

#include "driver/i2s_std.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lingtts_engine.h"

#ifndef CONFIG_HELLO_FORCE_INTERNAL
#define CONFIG_HELLO_FORCE_INTERNAL 0
#endif

static const char *const texts[] = {
    "你好，这是LingTTS，一个运行在单片机上的离线语音合成引擎。",
    "今天是2026年10月22日，气温18到25摄氏度，空气质量良。",
    "请连接WiFi，然后在App里输入验证码3721。",
};

static i2s_chan_handle_t tx;
static int16_t buf[512];
static volatile int playing, underruns;   /* 出声之后 DMA 播空的次数：不为 0 就会听到噗噗声 */
static int64_t cb_us;                      /* 回调里（写 I2S、等 DMA）花的时间：从墙钟里减掉就是合成耗时 */
static int64_t speak_us, first_us;         /* speak 开始的时刻；首包延迟 = 第一次回调 - speak 开始 */

static bool IRAM_ATTR on_underrun(i2s_chan_handle_t h, i2s_event_data_t *ev, void *user)
{
    underruns += playing;
    return false;
}

static int on_pcm(const int16_t *pcm, size_t n, void *user)
{
    int64_t t0 = esp_timer_get_time();
    size_t *samples = user;
    *samples += n;
    if (!playing) {
        first_us = t0 - speak_us;
    }
    playing = 1;
    while (n > 0) {   /* 音量按比例缩小；写满 DMA 后阻塞，让出 CPU */
        size_t k = n < sizeof(buf) / sizeof(buf[0]) ? n : sizeof(buf) / sizeof(buf[0]), written;
        for (size_t i = 0; i < k; i++) {
            buf[i] = (int16_t)(pcm[i] * CONFIG_HELLO_VOLUME / 100);
        }
        i2s_channel_write(tx, buf, k * sizeof(buf[0]), &written, portMAX_DELAY);
        pcm += k;
        n -= k;
    }
    cb_us += esp_timer_get_time() - t0;
    return 0;
}

static void i2s_init(void)
{
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    cc.auto_clear = true;   /* 播完输出静音，不重复最后一段 */
    /* DMA 缓冲要比合成一块（默认 32 帧 = 320 ms 音频，约 120~170 ms）的时间长：回调阻塞写 I2S，
     * 合成下一块时只靠 DMA 里剩下的数据撑着。IDF 默认 6 × 240 帧 = 90 ms 不够，块间会断供出噗噗声 */
    cc.dma_frame_num = 480;   /* 每个描述符 30 ms */
    cc.dma_desc_num = (CONFIG_HELLO_DMA_MS + 29) / 30;
    ESP_ERROR_CHECK(i2s_new_channel(&cc, &tx, NULL));
    i2s_std_config_t sc = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(LINGTTS_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = CONFIG_HELLO_BCLK, .ws = CONFIG_HELLO_WS,
                     .dout = CONFIG_HELLO_DOUT, .din = I2S_GPIO_UNUSED},
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &sc));
    i2s_event_callbacks_t cb = {.on_send_q_ovf = on_underrun};
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(tx, &cb, NULL));
    ESP_ERROR_CHECK(i2s_channel_enable(tx));
}

void app_main(void)
{
    i2s_init();
    lingtts_engine_cfg_t cfg;
    lingtts_engine_default_cfg(&cfg);
    /* 有 4 MB 以上 PSRAM 就用 PSRAM 配置（快约 25%），否则用内部 RAM 分时复用 */
    cfg.mem = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) >= 4 * 1024 * 1024 && !CONFIG_HELLO_FORCE_INTERNAL
                  ? LINGTTS_MEM_PSRAM
                  : LINGTTS_MEM_INTERNAL;
    size_t free0 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int64_t t0 = esp_timer_get_time();
    lingtts_engine_t *e;
    int r = lingtts_engine_create(&e, &cfg);
    if (r != LINGTTS_OK) {
        printf("引擎创建失败：%d（内部 RAM 最大连续块 %u B）\n", r,
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        return;
    }
    lingtts_engine_mem_t m;
    lingtts_engine_get_mem(e, &m);
    printf("LingTTS %s，%s 配置，创建 %lld ms；合成器 %u B + 前端 %u B + 共享区 %u B，复制资源 %u B；"
           "内部 RAM 用了 %u B\n",
           lingtts_version(), cfg.mem == LINGTTS_MEM_PSRAM ? "PSRAM" : "INTERNAL",
           (esp_timer_get_time() - t0) / 1000, (unsigned)m.tts_arena, (unsigned)m.fe_arena, (unsigned)m.shared,
           (unsigned)m.copied, (unsigned)(free0 - heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
    char id[40];
    lingtts_t *tts = lingtts_engine_tts(e);
    if (lingtts_device_id(id, sizeof(id)) != LINGTTS_OK) {
        snprintf(id, sizeof(id), "（读取失败）");
    }
    if (lingtts_is_licensed(tts)) {
        printf("设备 ID %s，已授权\n", id);
    } else if (lingtts_eval_seconds_left() == LINGTTS_UNLIMITED_SECONDS) {
        printf("社区版：无需设备激活，不限合成时长；商业使用需另行授权\n");
    } else {
        printf("设备 ID %s，评估模式（本次开机还能合成 %u 秒）\n", id, (unsigned)lingtts_eval_seconds_left());
    }
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
        size_t samples = 0;
        underruns = 0;
        cb_us = 0;
        int64_t t1 = speak_us = esp_timer_get_time();
        r = lingtts_engine_speak(e, texts[i], on_pcm, &samples);
        playing = 0;
        int64_t wall = esp_timer_get_time() - t1;
        double audio = (double)samples / LINGTTS_SAMPLE_RATE, synth = (wall - cb_us) / 1e6;
        /* 墙钟时间含播放阻塞，约等于音频时长；减掉回调时间是合成耗时（含前端），RTF = 合成耗时 / 音频时长 */
        printf("[%u] 返回 %d，音频 %.2f s，墙钟 %.2f s，合成 %.2f s（RTF %.3f），首包 %lld ms，断供 %d 次：%s\n",
               (unsigned)i, r, audio, wall / 1e6, synth, audio > 0 ? synth / audio : 0, first_us / 1000, underruns,
               texts[i]);
    }
    printf("主任务栈剩余 %u B\n", (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
    lingtts_engine_destroy(e);
}
