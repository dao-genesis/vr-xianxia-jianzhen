# 道枢主页 · 世界容器 v0.1

「主页即世界」的 WebXR 世界容器 MVP：Quest 3 浏览器（或任意 WebXR 浏览器）打开即进入可自由布局的 3D 世界。

## 功能
- 拖拽摆放物件：仙鹤（glTF）、灵石、影院幕布（视频/程序化纹理兜底）、灯
- 桌面模式：左键拖物体、右键转视角；VR 模式：手柄扳机/手势捏合抓取
- 布局持久化：world.json → localStorage，刷新不丢
- three.js 全量 vendor 本地化（npmmirror 源），无墙外 CDN 依赖
- 遥测信标 `/ping?stage=…`（配合 serve.py 日志服务器）
- XR 支持每 5s 复查：头显戴上后 VR 按钮自动激活

## 应用之门（daoshu-gate APK）
`daoshu-gate/` 是原生 Android 壳：开机自启（BOOT_COMPLETED）+ 前台服务在头显 127.0.0.1:8901 开「应用之门」HTTP 桥（/apps 枚举全部已装应用、/launch 拉起、/view 打开 URL），并将浏览器带入世界页。世界页底栏新增「应用之门」面板：列出头显内全部应用，点击直接拉起，或「+入世界」把应用做成世界内的门物件（VR 里扣扳机即启动）。构建：`./build.sh`（仅需 Android SDK build-tools，无 Gradle）。安装：`adb install daoshu-gate.apk ## 泼溅底景## 泼溅底景 adb shell appops set org.daoshu.gate SYSTEM_ALERT_WINDOW allow`。

## 泼溅底景
底栏「泼溅底景」按钮（或 URL 加 `?splat=1`）：加载 WorldLabs 3D 高斯泼溅场景（云端仙宫 500k·Spark 渲染），照片级真实底景替换程序化天空/地面，开关状态持久化。

## 点亮即世界(主页化自启)
`watchdog.ps1` 注册为 Windows 计划任务(daoshu-watchdog·登录自启): 监测 Quest 3 唤醒(display OFF→ON), 唤醒瞬间自动把世界容器拉到前台; 同时保底 serve.py 与 adb reverse 常在。戴上头显即在世界里, 无需手动进入任何应用。
```
schtasks /Create /F /TN daoshu-watchdog /SC ONLOGON /TR "powershell -WindowStyle Hidden -ExecutionPolicy Bypass -File <path>\\watchdog.ps1"
```

## 运行
```
python serve.py            # 8899 端口·带信标日志
adb reverse tcp:8899 tcp:8899
adb shell am start -a android.intent.action.VIEW -d "http://localhost:8899/index.html" com.oculus.browser
```
或经 GitHub Pages: https://zhouyoukang.github.io/vr-xianxia-jianzhen/daoshu/index.html
