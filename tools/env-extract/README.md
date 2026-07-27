# env-extract · 官方虚拟格 → glTF 无头提取器

把 Meta Horizon Home 官方环境包(Quest `.apk`)里的场景反向提取成标准 **glTF 2.0 + PNG**，
供 daoshu 世界作为固定基底(`officialBase`)加载。基于开源逆向项目
[xAstroBoy/Quest-Home-Editor](https://github.com/xAstroBoy/Quest-Home-Editor)(从 Meta `libshell.so`
逆向出 RENDMESH / RENDTXTR / HSTF / MATL / ASMH / scene 解析 + glTF 导出)。

> ⚠️ **版权**：官方环境资源是 Meta 专有资产。本工具仅供在**你自己的设备**上做个人研究/使用。
> 提取产物(`.apk`、`.gltf`、`.bin`、贴图)**一律不入库、不分发**——见仓库根 `.gitignore`。

## 组成

| 文件 | 作用 |
|---|---|
| `hsl_extract.cpp` | 无头(无 Vulkan/GLFW)提取器 `main`：加载 env→导出 glTF，可选合并 companion 包 |
| `qhe-parser-fixes.patch` | 对 Quest-Home-Editor 的两处解析修复(见下) + `hsl_extract` 的 CMake target |

## 两处解析修复(patch 内容)

1. **`scene_loader.h` — firstWorldAssetId 的 ingestion-id 兜底**
   现代 env(haven2025 / calming)的 `shellconfig.jsonc` 里 root 的 `targetId=0`(未指定)，
   但 manifest 里同一 ingestion 的 target 是真实非零值 → 精确 `(pkg,ing,tgt)` 查找失败、
   场景在遍历前就中止。加了「精确查找全 miss 后，仅按 ingestion-id(大唯一哈希)兜底」的一步，
   不影响任何原有可用查找。

2. **`rendtxtr_parser.h` — 天空盒 cubemap 识别**
   天空盒贴图是 **6 面 cubemap**(6 张面拼接)，没有单张贴图的 block footprint 能匹配其 payload 长度
   (calming 天空盒 1536² = 6×6 ASTC × 6 面 = 8,388,768 B) → 旧逻辑回退到错误的 formatCode 猜测，
   解出来是**横向错位条纹**。加了 ×6 布局识别，解码器取 face 0 的 mip0 作背景 → 天空盒净解。

## 构建

```bash
git clone https://github.com/xAstroBoy/Quest-Home-Editor.git
cd Quest-Home-Editor
git apply /path/to/tools/env-extract/qhe-parser-fixes.patch
cp /path/to/tools/env-extract/hsl_extract.cpp QuestHomeEditor/src/tools/hsl_extract.cpp

cmake -S QuestHomeEditor -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DHSR_BUILD_TESTS=OFF -DHSR_HAVE_PHYSX=OFF
cmake --build build --target hsl_extract -j"$(nproc)"
```

## 运行

```bash
# hsl_extract <env.apk> <outDir> [name] [companion.apk]
./build/hsl_extract vista_calming.apk ./out calming
```

产物：`out/calming.gltf` + `out/calming.bin` + `out/textures/*.png`。
(calming 实测：29 网格 / 材质 / 贴图 —— 天空盒、瀑布群、大气雾、树。)

## 接入 daoshu

把产物放到 `daoshu/private/official/`(已 gitignore)，世界页会作为固定基底加载：

```
daoshu/index.html?basescale=90&basey=-3      # 也可点底栏「官方基底」开关
```

`officialBase` 与用户创造的物件(`movables`)彻底分层：基底不参与拖拽/持久化，
用户内容浮于其上 —— 类《我的世界》：底不变，可在其上自由创造。
