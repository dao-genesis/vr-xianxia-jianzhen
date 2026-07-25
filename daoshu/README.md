# 道枢主页 · 世界容器 v0.1

「主页即世界」的 WebXR 世界容器 MVP：Quest 3 浏览器（或任意 WebXR 浏览器）打开即进入可自由布局的 3D 世界。

## 功能
- 拖拽摆放物件：仙鹤（glTF）、灵石、影院幕布（视频/程序化纹理兜底）、灯
- 桌面模式：左键拖物体、右键转视角；VR 模式：手柄扳机/手势捏合抓取
- 布局持久化：world.json → localStorage，刷新不丢
- three.js 全量 vendor 本地化（npmmirror 源），无墙外 CDN 依赖
- 遥测信标 `/ping?stage=…`（配合 serve.py 日志服务器）
- XR 支持每 5s 复查：头显戴上后 VR 按钮自动激活

## 运行
```
python serve.py            # 8899 端口·带信标日志
adb reverse tcp:8899 tcp:8899
adb shell am start -a android.intent.action.VIEW -d "http://localhost:8899/index.html" com.oculus.browser
```
或经 GitHub Pages: https://zhouyoukang.github.io/vr-xianxia-jianzhen/daoshu/index.html
