# LingTTS: Offline Chinese Neural Text-to-Speech for ESP32-S3

[简体中文](README.md) | English

LingTTS is a neural text-to-speech model for offline Mandarin speech synthesis on ESP32-S3-class devices. It generates speech locally from Chinese text, including stories, short passages and device information. The ESP-IDF SDK provides an integration interface.

[Try the online demo](https://edge.lingcorex.cn) — the demo runs the same C code and model compiled for a PC server; use a board to evaluate device performance.

## Can ESP32-S3 run neural TTS offline?

Yes. LingTTS runs the synthesis locally on ESP32-S3 without a cloud connection. The current community preview is 0.2.0-rc1. It supports Mandarin Chinese; English support is limited to letters and around 80 common words, not full English sentences.

## ESP32-S3 performance and memory

The measurements below are for ESP32-S3 N16R8 at 240 MHz with ESP-IDF 5.5.6. Results depend on the application and memory configuration. RTF is synthesis time divided by generated audio duration; lower is faster.

| Metric | Measured result |
| --- | --- |
| Audio output | 16 kHz, 16-bit mono PCM, delivered in chunks |
| RTF | 0.42 with PSRAM / 0.48 using internal RAM, hello example including text front end; 0.53–0.54 in the Wi-Fi demo |
| First PCM | 0.27–0.32 seconds from invocation, not speaker playback latency |
| Memory | 2.63 MB PSRAM plus about 1 KB internal RAM, or 151 KB internal RAM without PSRAM; task stack, DMA and application memory are extra |
| Flash | About 2.6 MB for library, model and dictionary; fits within 4 MB Flash subject to application and partition needs |

See the [performance details](docs/资源与性能.md) and [integration guide](docs/集成指南.md) (Chinese).

## Try it on a board

The demo requires an ESP32-S3 N16R8 board, MAX98357A amplifier and speaker. Connect BCLK to GPIO 15, LRC to GPIO 16, and DIN to GPIO 7. Download the community demo package from [Releases](../../releases), extract it and run:

```bash
python -m pip install esptool==4.12.0
python flash.py --port COM5  # Linux / WSL: python3 flash.py --port /dev/ttyUSB0
```

Connect a phone to the `LingTTS-Demo` Wi-Fi hotspot (password `lingtts-demo`), open http://192.168.4.1, enter Chinese text and play it. For ESP-IDF integration, download the community SDK package from Releases and see the [Chinese README](README.md#在自己的工程里用).

## Limitations

The current release offers a Mandarin voice. English is limited as described above; SSML, emotion and speed controls are not supported. Polyphonic characters, names and model numbers may require text adaptation. Other chips have not yet been adapted.

## License

Examples, public headers and integration files use the [MIT license](licenses/MIT.txt). The precompiled engine, model, dictionary and demo firmware use the separate [LingTTS Community License 1.0](licenses/LingTTS-Community-License-1.0.md). The core and model are not open-source software. Personal projects, education, and internal company R&D, testing and prototypes are free; commercial deployment requires written authorization. Contact service@lingcorex.cn.

Report board, documentation or pronunciation issues via [GitHub Issues](../../issues), including the board, SDK version, memory configuration and relevant logs. Do not include secrets or personal information.
