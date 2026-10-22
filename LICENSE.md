# LingTTS SDK 许可范围

这是社区候选包（0.2.0-rc1），供内部准备与验收，尚未公开发布。
核心许可仍为草案：拟授权主体为瓴枢智云（上海）科技有限公司，联系 service@lingcorex.cn；模型/音色/词典来源权利和正式许可尚待核准。不得将本候选包当作已核准的对外授权。

| 路径/内容 | 适用许可 |
|---|---|
| `examples/`、`components/lingtts/include/`、组件接入 CMake 文件 | `licenses/MIT.txt`，仅适用于这些明确列出的接入文件 |
| `components/lingtts/lib/`、`assets/` 内的 LingTTS 自有部分 | `licenses/LingTTS-Community-DRAFT.md`，拟定社区许可；不是 MIT/GPL |
| 文档 | 随 SDK 使用和再分发时可复制，保留来源、版本及限制说明；不因此获得核心实现权利 |
| 第三方部分 | 原许可独立有效，见 `NOTICE` 及 `components/lingtts/licenses/`；本许可不缩减其已有权利 |

核心引擎没有公开源码，整个 SDK 不应宣传为完全开源。MIT 示例允许商业使用，但示例所依赖的核心库和模型须另行满足社区或商业许可。

未经发布准备核查，不应将压缩包、模型或固件上传到公开下载站点。
