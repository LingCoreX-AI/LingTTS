# LingTTS: Offline Chinese Neural Text-to-Speech for ESP32-S3

[简体中文](README.md) | English

LingTTS is a neural text-to-speech model for offline Mandarin speech synthesis on ESP32-S3-class devices. It generates speech locally from Chinese text, including stories, short passages and device information. The ESP-IDF SDK provides an integration interface.

**Listen first:** [short sample](samples/mandarin-short.wav) · [short story](samples/mandarin-story.wav) · [type and synthesize your own text](https://edge.lingcorex.cn/#listen). The WAV samples are generated with LingTTS Voice A. The online demo runs a PC build; use board measurements below for device performance.

## Can ESP32-S3 run neural TTS offline?

Yes. LingTTS runs the synthesis locally on ESP32-S3 without a cloud connection. The current community preview is 0.2.0-rc2. It supports Mandarin Chinese; English support is limited to letters and around 80 common words, not full English sentences.

One voice model is about 1.83 MB; the model plus trimmed dictionary is about 2.38 MB. If LingTTS is useful for your ESP32 or embedded speech project, consider starring the repository so you can find it again and help other developers discover it.

## ESP32-S3 performance and memory

The measurements below are for ESP32-S3 N16R8 at 240 MHz with ESP-IDF 5.5.6. Results depend on the application and memory configuration. RTF is synthesis time divided by generated audio duration; lower is faster.

| Metric | Measured result |
| --- | --- |
| Audio output | 16 kHz, 16-bit mono PCM, delivered in chunks |
| RTF | 0.42 with PSRAM / 0.48 using internal RAM, hello example including text front end; 0.53–0.54 in the Wi-Fi demo |
| First PCM | 0.27–0.32 seconds from invocation, not speaker playback latency |
| Memory | 2.63 MB PSRAM plus about 1 KB internal RAM, or 151 KB internal RAM without PSRAM; task stack, DMA and application memory are extra |
| Flash | About 2.6 MB for library, one voice model and dictionary; fits within 4 MB Flash subject to application and partition needs |

See the [performance details](docs/资源与性能.md) and [integration guide](docs/集成指南.md) (Chinese).

## Voices

Two Mandarin female voices are available: **Voice A** is included in the SDK and ready-to-flash demo; **Voice B** is distributed as `lingtts-voice-b-1.0.tar.gz` in this version's [Releases](../../releases).

Voice B is a complete model. Replace the model resource as described in the [integration guide](docs/集成指南.md). The demo uses Voice A by default; keeping both models requires an extra model partition, and switching between them requires recreating the engine.

## Try it on a board

The demo requires an ESP32-S3 N16R8 board, MAX98357A amplifier and speaker. Connect BCLK to GPIO 15, LRC to GPIO 16, and DIN to GPIO 7. Download the community demo package from [Releases](../../releases), extract it and run:

```bash
python -m pip install esptool==4.12.0
python flash.py --port COM5  # Linux / WSL: python3 flash.py --port /dev/ttyUSB0
```

Connect a phone to the `LingTTS-Demo` Wi-Fi hotspot (password `lingtts-demo`), open http://192.168.4.1, enter Chinese text, select a speech speed and play it. Defaults are 1.0× speed and 50% playback volume.

For ESP-IDF integration, clone this repository and fetch the versioned binary SDK package with checksum verification:

```bash
# Linux / WSL
./scripts/get-sdk.sh

# Windows PowerShell
.\scripts\Get-LingTTSSdk.ps1
```

Then build from the SDK directory printed by the script, with ESP-IDF 5.5.6 configured:

```bash
idf.py -C examples/hello -B build/hello build
```

The examples and integration files are in this repository. The precompiled component, model and dictionary are distributed in the separately licensed SDK package. See the [Chinese integration guide](https://github.com/LingCoreX-AI/LingTTS/blob/main/docs/%E9%9B%86%E6%88%90%E6%8C%87%E5%8D%97.md).

## Limitations

- Full English sentences, SSML and emotion controls are not supported.
- Polyphonic characters, names and model numbers may be mispronounced.

## License

Examples, public headers and integration files use the [MIT license](licenses/MIT.txt). The precompiled engine, model, dictionary and demo firmware use the separate [LingTTS Community License 1.0](licenses/LingTTS-Community-License-1.0.md). The core and model are not open-source software. Personal projects, education, and internal company R&D, testing and prototypes are free; commercial deployment requires written authorization. Contact service@lingcorex.cn.

Report board, documentation or pronunciation issues via [GitHub Issues](../../issues), including the board, SDK version, memory configuration and relevant logs. Do not include secrets or personal information.

## Speech speed (0.2.0-rc2)

The hotspot demo offers 0.5–2.0× speech speed (default 1.0×). In the hello example, menuconfig → LingTTS hello → Speech speed percent accepts 50–200.

Set `cfg.speed = 1.2f` before engine creation, or call `lingtts_engine_set_speed(e, 0.8f)` between utterances in the same task that calls `speak`. Speed scales predicted phoneme durations; PCM and I2S stay at 16 kHz. Board listening passed for all six speeds on 2026-11-13 (six short utterances, all error codes zero). An instrumented validation build using the same rc2 engine and HTTP/synthesis/I2S paths, at 3% volume, completed 84 utterances and 30 min 35 s of audio over 32 min 23 s, with no playback errors or unexpected reboot and no sustained decrease in sampled free memory. This was separate from the exact released app binary. Earlier specification measurements describe rc1; see the [demo validation record](examples/web_demo/README.md).
