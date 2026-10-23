# LingTTS hello：引擎 API 最小例子

开机后依次合成三句文本，经 I2S 播放，串口打印内存和每句耗时。全部代码在 `main/main.c`，约 100 行。

## 硬件

- ESP32-S3，Flash ≥ 4 MB。有 4 MB 以上 PSRAM 时自动用 `LINGTTS_MEM_PSRAM`，否则用 `LINGTTS_MEM_INTERNAL`。
  `sdkconfig.defaults` 按八线 PSRAM（N16R8 一类）配置，并打开了 `SPIRAM_IGNORE_NOTFOUND`，所以没有 PSRAM 的板子也能直接启动。
  四线 PSRAM 的板子在 menuconfig 里改 PSRAM 模式。
- MAX98357A 一类 I2S 功放：默认 BCLK GPIO15、LRCK GPIO16、DIN GPIO7，音量 30%。
  在 `idf.py -C examples/hello menuconfig` → LingTTS hello 里修改；也可以强制使用 INTERNAL 配置。

## 构建和烧录

在 SDK 包的根目录（GitHub Releases 里的 `lingtts-sdk-*.tar.gz` 解压后）：

```bash
. ~/esp/esp-idf-v5.5.6/export.sh
idf.py -C examples/hello -B build/hello build
idf.py -C examples/hello -B build/hello -p /dev/ttyUSB0 flash monitor
```

`flash` 会同时把模型和词典写进 `lingtts_model`、`lingtts_fe` 两个分区（`lingtts_flash_assets()`，见组件的 `project_include.cmake`）。
资源默认来自 SDK 包的 `assets/`，
也可以用 `-D LINGTTS_ASSET_DIR=/绝对路径` 指定。

分区表按 4 MB Flash 排：模型分区 2 MB，词典分区 1.06 MB，能放下裁剪词典（0.54 MB）。
完整词典（1.19 MB）需要 8 MB Flash，并把 `lingtts_fe` 分区改大。

## 社区使用

0.2.0 社区版不需要 license.bin、不联网激活、不限制合成时长。旧授权分区不会自动加载。
个人非商业与企业内部研发评估免费；销售产品、正式商业部署需另行取得商业授权。
示例 MIT 与核心资源许可分开，见 SDK 根目录 LICENSE.md。

## 用到的 API

```c
lingtts_engine_cfg_t cfg;
lingtts_engine_default_cfg(&cfg);          // 资源从分区找；PSRAM 配置；语速 1.0
cfg.mem = LINGTTS_MEM_INTERNAL;            // 没有 PSRAM 时
lingtts_engine_t *e;
lingtts_engine_create(&e, &cfg);
lingtts_engine_speak(e, "你好。", on_pcm, user);   // 阻塞；on_pcm 收到 16 kHz 单声道 int16
lingtts_engine_destroy(e);
```

回调里直接写 I2S（DMA 写满就阻塞），所以合成快于播放时会自动等待。
**DMA 缓冲必须比合成一块的时间长**（menuconfig 里的 I2S DMA 缓冲，默认 400 ms）：回调阻塞期间合成器不动，
合成下一块（默认 32 帧 = 320 ms 音频，板上约 120~170 ms）时只靠 DMA 里剩下的数据撑着。
IDF 默认的 6 × 240 帧 = 90 ms 不够，块间会断供，听起来是轻微的噗噗声。串口每句打印的“断供”次数应为 0。
需要在合成的同时做别的事，就把 `speak` 放在单独的任务里，从其他任务调用 `lingtts_engine_stop` 打断；
也可以像 `examples/web_demo` 那样，回调只把 PCM 放进环形缓冲，由单独的播放任务写 I2S。
这个例子是墙钟计时，包含播放等待；纯合成的 RTF 见 `docs/资源与性能.md`。

## 验证状态

0.2.0-rc1 只改了授权逻辑，合成路径与下面的测试相同。

2026-10-22 在 ESP32-S3（N16R8，八线 PSRAM 8 MB）上验证：

| 配置 | 创建 | 合成器 + 前端 + 共享区 | 内部 RAM | 三句（17.0 s 音频） |
|---|---|---|---|---|
| PSRAM | 0.96 s（含复制 2.3 MB 资源） | 127600 + 123784 + 0 B | 约 1 KB | 断供 0 次，样本数与 PC 相同 |
| INTERNAL | 0.85 s | 11412 + 21976 + 116176 B | 150748 B | 断供 0 次，样本数与 PC 相同 |

- 设备 ID 从 eFuse 读到 `U-` 开头的 32 位十六进制；本机授权通过（验签约 60 ms，算在创建时间里），
  别的设备的授权 `lingtts_engine_create` 返回 -6（这一项是带开发授权的构建；社区版默认不读取授权分区）。
- 板上的合成器 arena 比 PC 大约 27 KB，是 esp-nn 的 S3 内核要的 scratch。
- PC 上同样的引擎代码由 `tools/pc_tests.sh` 验证（两种内存配置的输出都和底层 API 逐字节一致）。
