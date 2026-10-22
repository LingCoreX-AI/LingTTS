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

## Community policy / 社区政策

个人非商业项目、教学研究和企业内部研发评估免费；不要求注册、联网激活或逐台登记，不设测试设备数和合成时长门槛。销售产品、收费服务和正式商业部署需要书面商业授权；早期项目可申请免费或优惠安排。

免费使用不附带专门适配或响应时限。示例与应用固件可按其适用许可分享，核心资源不是 MIT。

当前正在准备 0.2.0-rc1，本仓库仍为私有准备仓库。正式许可主体、上游资源权利和板端验收尚待完成，暂不公开分发核心库、模型或固件。

详细说明见 [社区使用与商业授权](docs/community-policy.md)；正式发布前以 [发布检查](docs/release-checklist.md) 核对。

## Release plan

- Quick-start firmware for a documented reference board.
- Community SDK with examples and integration documentation.
- Clear licensing terms for examples, the core engine and model assets.

## Licensing

Examples and integration code are planned to use MIT. The precompiled core engine and model assets will be distributed under a separate community license, with separate written authorization for product distribution and production commercial use.

示例与接入代码计划采用 MIT；核心引擎和模型采用独立社区许可。个人非商业项目与企业内部研发评估免费，无需注册或联网激活，不设设备数和合成时长门槛。产品销售、收费服务及正式商业部署需另行书面授权。具体再分发条件以正式许可为准。此说明不替代正式许可，也不赋予尚未发布资产的使用权。

## Feedback

After the community release, please include your board, SDK version, configuration, input text and relevant logs when reporting an issue. Do not post private keys, authorization files or personal data.

社区版发布后，欢迎提交板型、版本、输入文本和相关日志，帮助改进实际使用体验。
