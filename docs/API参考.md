# LingTTS API 参考（0.2.0-rc1）

应用只需要 `#include "lingtts_engine.h"`。它包含了 `lingtts.h`，常量、错误码和下面列出的几个工具函数在那里定义。

`lingtts.h` 和 `lingtts_frontend.h` 里还有一层底层接口（自己管理内存、直接输入音素序列等），
供有特殊调度需求的客户使用；底层接口不在本文档范围内，需要时请联系我们。

## 常量

| 名字 | 值 | 说明 |
|---|---|---|
| `LINGTTS_SAMPLE_RATE` | 16000 | 输出采样率（Hz），单声道 int16 |
| `LINGTTS_VERSION` | `"0.2.0-rc1"` | 头文件版本；库的版本用 `lingtts_version()` |
| `LINGTTS_ENGINE_NO_COPY` | `1u` | `cfg.flags`：PSRAM 配置下不把资源复制进 PSRAM |

错误码见 `集成指南.md` 第 6 节。

## 类型

```c
typedef enum { LINGTTS_MEM_PSRAM = 0, LINGTTS_MEM_INTERNAL = 1 } lingtts_mem_t;

typedef struct {
    const void *model;     size_t model_size;      /* NULL = 分区 "lingtts_model" */
    const void *frontend;  size_t frontend_size;   /* NULL = 分区 "lingtts_fe" */
    const void *license;   size_t license_size;    /* 社区版 NULL = 不读取授权分区；非 NULL 时验证显式凭证 */
    lingtts_mem_t mem;     /* 默认 LINGTTS_MEM_PSRAM */
    uint32_t flags;        /* 默认 0 */
    float speed;           /* 语速，默认 1.0；大于 1 更快，例如 1.2 */
    uint16_t block;        /* 每块帧数，默认 32；16 时首包快约 30 ms，总耗时多约 6% */
    uint16_t max_tokens;   /* 每段最多音素数，默认 128；一般不用改 */
} lingtts_engine_cfg_t;

typedef struct {
    size_t tts_arena;      /* 合成器常驻内存 */
    size_t fe_arena;       /* 前端常驻内存 */
    size_t shared;         /* 合成器和前端分时复用的共享区（只有 INTERNAL 配置有） */
    size_t copied;         /* 复制进 PSRAM 的资源 */
} lingtts_engine_mem_t;

typedef int (*lingtts_pcm_cb)(const int16_t *pcm, size_t n, void *user);
```

自己传资源地址时，地址要 16 字节对齐，并且在 `lingtts_engine_destroy` 之前一直有效（引擎不复制）。

## 函数

### `void lingtts_engine_default_cfg(lingtts_engine_cfg_t *cfg)`

填默认值：资源全部从分区找、PSRAM 配置、语速 1.0、块 32 帧、每段 128 个音素。先调用它，再改需要的字段。

### `int lingtts_engine_create(lingtts_engine_t **out, const lingtts_engine_cfg_t *cfg)`

找资源、校验显式提供的设备凭证、分配内存、初始化。社区版不自动加载授权分区。`cfg` 为 NULL 时用默认配置。耗时约 0.9 s（PSRAM 配置里大部分是复制资源）。

返回 `LINGTTS_OK`；`LINGTTS_ERR_MODEL`（资源找不到或格式不对）；`LINGTTS_ERR_ARENA`（内存不够）；
`LINGTTS_ERR_LICENSE`（有授权文件但无效）；`LINGTTS_ERR_ARG`。失败时 `*out` 为 NULL，已分配的内存都已释放。

内存全部在这里一次分配好，之后 `speak` 不再分配内存。

### `int lingtts_engine_speak(lingtts_engine_t *e, const char *utf8, lingtts_pcm_cb cb, void *user)`

合成一段 UTF-8 文本，阻塞到全部合成完才返回。文本长度不限，内部按句末标点切句，过长的句子在逗号等处再切。

每产生一段 PCM 调用一次 `cb(pcm, n, user)`，一般 n = 160（10 ms）。`pcm` 只在回调期间有效，需要保留就复制。
回调返回非 0 表示停止。回调在调用 `speak` 的任务里执行，可以在回调里阻塞（例如写 I2S），合成会相应暂停。

返回：
- `LINGTTS_OK`：全部合成完。只有标点、空白等读不出来的文本也返回 OK，但不会回调。
- `LINGTTS_STOPPED`：回调返回了非 0，或别的任务调用了 `lingtts_engine_stop`。
- `LINGTTS_ERR_LICENSE`：内部限时评估构建的额度已用完（社区包默认不限制）（已经回调出去的音频不受影响）。
- 其他负数：错误。

### `void lingtts_engine_stop(lingtts_engine_t *e)`

可以从任意任务调用。正在进行的 `speak` 在下一段 PCM 之前返回 `LINGTTS_STOPPED`，之后不再回调。
只影响当前这次 `speak`；没有 `speak` 在进行时调用不起作用（不会让下一次 `speak` 立刻停止）。

### `int lingtts_engine_set_voice(lingtts_engine_t *e, const void *ltv, size_t size)`

切换音色：`ltv` 是音色包（`.ltv` 文件内容，16 字节对齐，使用期间一直有效），NULL 换回基础音色。
切换只需要几十微秒，合成速度不变。失败时保持原来的音色。只能在调用 `speak` 的任务里、`speak` 之外调用。

返回 `LINGTTS_ERR_VOICE`（格式、CRC、签名错误或和模型不配套）；`LINGTTS_ERR_LICENSE`（有授权时，
音色包没有签名或不在授权的音色列表里）。

### `void lingtts_engine_get_mem(const lingtts_engine_t *e, lingtts_engine_mem_t *mem)`

引擎占用的内存，字段含义见上面的类型说明。

### `lingtts_t *lingtts_engine_tts(lingtts_engine_t *e)`

取引擎内部的合成器对象，用来调用 `lingtts_is_licensed`。

### `void lingtts_engine_destroy(lingtts_engine_t *e)`

释放引擎的全部内存和分区映射。不能在 `speak` 进行中调用。

## 工具函数（`lingtts.h`）

| 函数 | 说明 |
|---|---|
| `const char *lingtts_version(void)` | 库版本，例如 `"0.2.0-rc1"` |
| `int lingtts_device_id(char *buf, size_t size)` | 本机设备 ID，用来申请授权。`size` 至少 35。返回 `LINGTTS_OK` 或 `LINGTTS_ERR_ARG` |
| `int lingtts_is_licensed(const lingtts_t *tts)` | 1 = 已加载有效设备凭证，0 = 未加载；不表示商业使用权。参数用 `lingtts_engine_tts(e)` |
| `uint32_t lingtts_eval_seconds_left(void)` | 社区版返回 `LINGTTS_UNLIMITED_SECONDS`（UINT32_MAX）；内部限时构建返回剩余秒数 |

## 完整例子

见 `examples/hello/main/main.c`（约 150 行，含 I2S 初始化、断供统计、内存和速度打印）。
