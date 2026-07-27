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
| `hsl_extract.cpp` | 无头(无 Vulkan/GLFW)提取器 `main`：加载 env→导出 glTF，可选合并 companion 包。默认**全量**模式(见下),`HSL_SCENEONLY` 可退回只导 runtime 绘制子集 |
| `hsl_bulk.cpp` | 场景图无关的**穷举**转储:直接遍历 `scene.zip` 每个 cooked 子目标,解码所有 RENDMESH/RENDTXTR(不依赖可达性,作兜底对照) |
| `qhe-parser-fixes.patch` | 对 Quest-Home-Editor 的解析修复(见下) + `hsl_extract`/`hsl_bulk` 的 CMake target |

## 解析修复(patch 内容)

1. **`hstf_parser.h` — 资产 id 是 JSON 字符串(根因·关键)**
   cooker 把 `packageOrRemoteId`/`ingestionId`/`targetId` 写成 JSON **字符串**(`"955515025"`)。
   `targetId` 原来用 `tinyjson::asInt()` 读 —— 而 `asInt()` 对字符串返回 **0**。tgt 变 0 后精确
   `(pkg,ing,tgt)` 查找必然 miss,`resolve()` 回退到「仅 ingestion-id」那一步,把**同一 level 里
   共享 ingestion 的所有实体全部塌缩到一个任意子目标**(calming 的 62 个 cagemeshes 实体全变成
   同一块 `root_xform_boulders`)。加 `hstfIdU64()` 同时接受字符串/数字两种形式,三处 id 读取统一改用它。
   这是「只提到 29 个网格」的真正根因。

2. **`scene_loader.h` — 全量 level 遍历(`loadAllLevels`)**
   libshell 的绘制遍历从 `firstWorldAssetId` 出发,fgpockets(卵石/植物/原木)与 cagemeshes
   (远景树线/山谷卡片)两个 sublevel 的 type-ref 会误解析到某个 mesh 子目标(而非该 level 的
   `.usda/template` HSTF),导致这两层一个实体都不产出。开 `loadAllLevels` 后,主遍历结束再把 manifest
   里每个未访问过的 `…/levels/*.usda/template_9k0v` 直接作为 root 加载 —— 每个实体都自带
   transform+mesh+material+collider,全环境**带忠实世界放置**进来。

3. **`scene_loader.h` — firstWorldAssetId 的 ingestion-id 兜底**
   现代 env(haven2025 / calming)root 的 `targetId=0`(未指定),但 manifest 里同一 ingestion 的
   target 非零 → 精确查找失败、场景在遍历前中止。加了「精确全 miss 后仅按 ingestion-id 兜底」一步。
   (注:修复 1 之后此兜底不再被 sublevel 实体误触发。)

4. **`rendtxtr_parser.h` — 天空盒 cubemap 识别**
   天空盒贴图是 **6 面 cubemap**,没有单张贴图的 block footprint 能匹配其 payload 长度
   (calming 天空盒 1536² = 6×6 ASTC × 6 面 = 8,388,768 B) → 旧逻辑回退到错误 formatCode 猜测,
   解出**横向错位条纹**。加 ×6 布局识别,取 face 0 的 mip0 作背景 → 天空盒净解。

## 构建

```bash
git clone https://github.com/xAstroBoy/Quest-Home-Editor.git
cd Quest-Home-Editor
git apply /path/to/tools/env-extract/qhe-parser-fixes.patch
cp /path/to/tools/env-extract/hsl_extract.cpp QuestHomeEditor/src/tools/hsl_extract.cpp
cp /path/to/tools/env-extract/hsl_bulk.cpp    QuestHomeEditor/src/tools/hsl_bulk.cpp

cmake -S QuestHomeEditor -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DHSR_BUILD_TESTS=OFF -DHSR_HAVE_PHYSX=OFF
cmake --build build --target hsl_extract -j"$(nproc)"
```

## 运行

```bash
# hsl_extract <env.apk> <outDir> [name] [companion.apk]   —— 默认全量(所有 sublevel)
./build/hsl_extract vista_calming.apk ./out calming

# 只导 runtime 绘制子集(旧行为,对照用):
HSL_SCENEONLY=1 ./build/hsl_extract vista_calming.apk ./out_scene calming
```

产物：`out/calming.gltf` + `out/calming.bin` + `out/textures/*.png`。

calming 实测(修复前后对照,同一 APK):

| 模式 | 网格 | 说明 |
|---|---|---|
| `HSL_SCENEONLY`(旧) | 29 | 只有 libshell 绘制的 MG-shell 子集(天空盒/瀑布/雾/树) |
| 默认全量(现) | **184**,0 跳过 | 追加 fgpockets(卵石/植物/原木)+ cagemeshes(树线/山谷/中景卡片) 全部带忠实放置+材质+光照贴图。约 31.6 万顶点 / 248 张贴图 |

> 注:29 只是 **runtime 可见子集**,不是全量;184 才是 scene.zip cooked asset store 的完整可绘制集合。
> 未绘制类别(碰撞 SEBD / navmesh / 骨骼动画 / 音频)仍以私有 sidecar 形式另行处理。

## 接入 daoshu

把产物放到 `daoshu/private/official/`(已 gitignore)，世界页会作为固定基底加载：

```
daoshu/index.html?basescale=90&basey=-3      # 也可点底栏「官方基底」开关
```

`officialBase` 与用户创造的物件(`movables`)彻底分层：基底不参与拖拽/持久化，
用户内容浮于其上 —— 类《我的世界》：底不变，可在其上自由创造。
