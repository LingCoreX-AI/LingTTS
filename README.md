# LingTTS

**Offline Chinese neural text-to-speech for ESP32-S3.**

在 ESP32-S3 上将中文文本转换为语音，无需云端 TTS。

> Community release in preparation / 社区版准备中。
> 当前页面是发布准备稿，尚不提供公开社区版下载。正式使用范围以发布包附带的许可为准。

## What it does

- Generates speech locally from UTF-8 Chinese text.
- Streams 16 kHz, 16-bit mono PCM for device playback.
- Provides a minimal integration example and a Wi-Fi hotspot web demo.
- Documents hardware requirements, measured performance and known limitations.

## 当前范围

目前支持 ESP32-S3 和一个普通话基础音色。英文主要用于少量字母、缩写和常用词，不适合英文整句朗读；不支持 SSML。多音字、专名及部分编号读法仍可能需要适配。

SDK 0.1.0 内部实测参考：N16R8、CPU 240 MHz、ESP-IDF 5.5.6，hello 示例 PSRAM 配置的合成 RTF 为 0.42，常见句子从调用合成到首段 PCM 为 0.27–0.32 秒。首段 PCM 延迟不等于扬声器实际出声延迟，网页演示和客户整机性能需分别验证。

## Release plan

- Quick-start firmware for a documented reference board.
- Community SDK with examples and integration documentation.
- Clear licensing terms for examples, the core engine and model assets.

## Licensing

Examples and integration code are planned to use MIT. The precompiled core engine and model assets will be distributed under a separate community license, with commercial licensing for deployments beyond its free allowance.

示例与接入代码计划采用 MIT；核心引擎和模型采用独立社区许可。免费范围、商业部署额度及再分发条件将在正式发布前明确。此说明不替代正式许可，也不赋予尚未发布资产的使用权。

## Feedback

After the community release, please include your board, SDK version, configuration, input text and relevant logs when reporting an issue. Do not post private keys, authorization files or personal data.

社区版发布后，欢迎提交板型、版本、输入文本和相关日志，帮助改进实际使用体验。
