<p>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset=".github/assets/lingcorex-lockup-h-dark.svg">
    <img src=".github/assets/lingcorex-lockup-h-light.svg" width="180" alt="瓴枢智云 LingCoreX">
  </picture>
</p>

# LingTTS

**ESP32-S3 上的离线中文语音合成。** Offline Chinese text-to-speech for ESP32-S3.

LingTTS 是一个 ESP-IDF 组件。传入一段中文，芯片本地合成 16 kHz 语音，边合成边从 I2S 播出。不联网，不调用云端服务，同样的输入每次输出完全一致。

在线试听：<https://edge.lingcorex.cn>（服务器上跑的是同一份 C 代码和模型，编译成了 PC 版）

当前版本 **0.2.0-rc1（社区预览版）**。社区版免费，示例代码 MIT；核心库和模型适用独立的社区许可，见[授权](#授权)。

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

从 [Releases](../../releases) 下载 SDK 包 `lingtts-sdk-<版本>-community.tar.gz`。包里有预编译组件 `components/lingtts`、模型和词典 `assets/`、两个示例和文档。本仓库的 `examples/` 与包里的示例相同，构建时要在解压后的 SDK 包里进行。

```bash
. ~/esp/esp-idf-v5.5.6/export.sh
cd lingtts-sdk-0.2.0-rc1-community
idf.py -C examples/hello -B build/hello -p /dev/ttyUSB0 flash monitor
```

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
    lingtts_engine_speak(e, "您本次消费23.50元。", on_pcm, NULL);
}
```

分区、内存配置、播放、停止和错误码见 [集成指南](docs/集成指南.md) 和 [API 参考](docs/API参考.md)。I2S DMA 建议留 400 ms 以上缓冲，示例已配好。

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
- 不支持 SSML、情感和语速控制。
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
