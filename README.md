<p>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset=".github/assets/lingcorex-lockup-h-dark.svg">
    <img src=".github/assets/lingcorex-lockup-h-light.svg" width="180" alt="瓴枢智云 LingCoreX">
  </picture>
</p>

# LingTTS：ESP32-S3 离线中文神经网络语音合成

[English](README.en.md) | 简体中文

**ESP32-S3 上的离线中文语音合成。** Offline Chinese text-to-speech for ESP32-S3.

LingTTS 是一款面向端侧的神经网络语音合成模型，在 ESP32-S3 级别的芯片上，即可实现高质量中文语音合成。完全离线运行，无需云端服务，让低成本设备也能自然开口。

在线试听：<https://edge.lingcorex.cn>（服务器上跑的是同一份 C 代码和模型，编译成了 PC 版）

当前版本 **0.2.0-rc2（社区预览版）**。社区版免费，示例代码 MIT；核心库和模型适用独立的社区许可，见[授权](#授权)。

## ESP32-S3 能离线运行中文神经网络 TTS 吗？

可以。LingTTS 接收中文文本，在 ESP32-S3 上本地生成语音，适合朗读故事、短文、说明文字和设备信息。合成过程不需要连接云端；ESP-IDF SDK 提供集成接口。

**先听效果：**[短句试听](samples/mandarin-short.wav) · [童话朗读](samples/mandarin-story.wav) · [在线输入任意文本](https://edge.lingcorex.cn/#listen)。WAV 样例由 LingTTS 女声 A 合成；在线试听运行 PC 版，板端速度和资源请以上板数据为准。

- **模型资源**：单音色模型 1.83 MB，模型加裁剪词典约 2.38 MB。
- **板端速度**：ESP32-S3 N16R8 上，hello 示例 RTF 为 0.42（PSRAM）或 0.48（内部 RAM）。
- **离线与授权**：生成语音不需要网络。示例代码采用 MIT 许可，核心库、模型和词典采用独立社区许可；商业部署需书面授权。

如果 LingTTS 对你的 ESP32 或嵌入式语音项目有参考价值，欢迎点 Star 收藏，方便之后找到，也帮助更多开发者发现这个项目。

## 上板试听（不装开发环境）

需要 ESP32-S3 N16R8 开发板、MAX98357A 功放和一个喇叭。

1. 接线（MAX98357A → 开发板）：BCLK → GPIO 15，LRC → GPIO 16，DIN → GPIO 7。
2. 从 [Releases](../../releases) 下载 `lingtts-demo-<版本>-n16r8-community.tar.gz` 并解压。
3. 烧录：

   ```bash
   python -m pip install esptool==4.12.0
   python flash.py --port COM5          # Linux / WSL：python3 flash.py --port /dev/ttyUSB0
   ```

   脚本先核对全部文件的校验值，再让你输入 YES 确认。
4. 手机连热点 `LingTTS-Demo`（密码 `lingtts-demo`），打开 <http://192.168.4.1>，输入一句中文，点播放。页面会显示这次合成的首段延迟、RTF 和内存。

## 在自己的工程里用

本仓库提供组件接入示例和文档；预编译组件、模型和词典通过版本化 SDK 包分发。克隆仓库后，可运行脚本下载当前预览版并校验 SHA-256：

```bash
# Linux / WSL
./scripts/get-sdk.sh

# Windows PowerShell
.\scripts\Get-LingTTSSdk.ps1
```

然后在脚本打印出的 SDK 目录中构建（已配置 ESP-IDF 5.5.6 环境）：

```bash
idf.py -C examples/hello -B build/hello build
```

刷写并监视串口时，把末尾的 `build` 换成 `-p <串口> flash monitor`。也可以直接从 [Releases](../../releases) 手动下载并核对 SHA-256。SDK 包中的组件和资源采用独立社区许可，细节见[授权](#授权)。

```c
#include "lingtts_engine.h"

static int on_pcm(const int16_t *pcm, size_t n, void *user)
{
    size_t written;
    i2s_channel_write(tx, pcm, n * 2, &written, portMAX_DELAY);
    return 0;                     /* 返回非 0 即停止合成 */
}

void app_main(void)
{
    lingtts_engine_cfg_t cfg;
    lingtts_engine_default_cfg(&cfg);    /* 选 PSRAM 或内部 RAM */

    lingtts_engine_t *e;
    lingtts_engine_create(&e, &cfg);
    lingtts_engine_speak(e, "夜晚的风轻轻吹过树林。", on_pcm, NULL);
}
```

分区、内存配置、播放、停止和错误码见 [集成指南](docs/集成指南.md) 和 [API 参考](docs/API参考.md)。I2S DMA 建议留 400 ms 以上缓冲，示例已配好。

## 语速调节（0.2.0-rc2）

热点演示网页可选择 0.5～2.0 倍语速，默认 1.0 倍。hello 示例可在 menuconfig → LingTTS hello → Speech speed percent 设置 50～200。

```c
cfg.speed = 1.2f;                         // 创建引擎前设置初始语速
// 创建引擎后，在两次朗读之间调用：
lingtts_engine_set_speed(e, 0.8f);
```

调速作用于模型预测的音素时长；输出和 I2S 始终为 16 kHz。设置影响后续朗读，请在调用 speak 的同一任务中设置。rc2 六档语速的板上短句试听已于 2026-11-13 通过；使用同一 rc2 引擎及播放代码的自动验收构建（音量 3%）已完成累计 30 分 35 秒音频、84 次播放，未见播放错误、异常重启或持续内存下降。详见 [演示验收记录](examples/web_demo/README.md)。下方规格中的历史实测数字来自 rc1。

## 规格

ESP32-S3 N16R8 · 240 MHz · ESP-IDF 5.5.6，板上实测。

| | |
|---|---|
| 输出 | 16 kHz · 16 bit · 单声道，PCM 通过回调分段送出 |
| 合成速度 RTF | 0.42 / 0.48（PSRAM / 无 PSRAM，hello 示例，含文本前端）；网页演示开着热点时 0.53–0.54 |
| 首段音频 | 0.27–0.32 s，从调用到拿到第一段 PCM，不等于喇叭出声时间 |
| 内存 | PSRAM 2.63 MB + 内部 RAM 约 1 KB；无 PSRAM 时用内部 RAM 151 KB。任务栈、DMA 和应用内存另计 |
| Flash | 约 2.6 MB：库 189 KB · 模型 1.83 MB · 词典 0.54 MB，可放进 4 MB Flash |
| CPU | 单核，另一个核留给 Wi-Fi、显示和业务逻辑 |
| 文本 | 中文、数字、日期、金额；英文只认字母和约 80 个常用词 |

详细数据见 [资源与性能](docs/资源与性能.md)。

## 当前局限

- 只有一个普通话基础音色，可根据需求定制。
- 英文只读字母和约 80 个常用词，带中文口音；整句英文不行。
- 支持整体语速调节（建议 0.5～2.0 倍）；不支持 SSML 和情感控制。
- 多音字、专名、型号编号可能读错，需要按你的文本适配。见 [文本支持与已知局限](docs/文本支持与已知局限.md)。
- 模型还在迭代，音质和长句停顿还有提升空间。

## 授权

| 内容 | 许可 |
|---|---|
| 本仓库的示例代码、公开头文件和组件接入文件 | [MIT](licenses/MIT.txt) |
| SDK 包里的预编译引擎库、模型、词典和体验固件 | [LingTTS 社区许可协议 1.0](licenses/LingTTS-Community-License-1.0.md) |

- **免费**：个人项目、学习教学，以及公司内部研发、测试和原型演示。不用注册，不联网激活，不限台数和合成时长。
- **商业部署需书面授权**：卖产品、收费服务、正式上线。早期项目可以谈免费或优惠。
- **工程支持按需**：适配你的板子、调文本读法、定制音色。

核心库和模型不是开源软件，整个 SDK 不能当作 MIT 使用。**请特别注意社区许可协议第 7 条（免责声明）和第 8 条（责任限制）。** 常见问题见 [社区使用与商业授权](docs/社区使用与商业授权.md)。

商业授权和工程合作：service@lingcorex.cn

## 反馈

读错的字词、跑不起来的板子、文档问题，请在 [Issues](../../issues) 提交，附上板型、SDK 版本、内存配置、输入文本和串口日志。不要贴设备私钥、授权文件或个人信息。

---

© 2026 瓴枢智云（上海）科技有限公司
