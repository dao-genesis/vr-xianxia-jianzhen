# daoshu watchdog · 点亮即世界
# 监测 Quest 3 唤醒: 唤醒瞬间自动把世界容器拉到前台; 并保底 serve.py/adb reverse 常在
$SER = "2G0YC5ZG8L08Z7"
$URL = "http://localhost:8899/index.html"
$DIR = "c:\Users\zhouyoukang\dao-workspace\daoshu"
$prev = "Unknown"
function Log($m){ Add-Content -Path "$DIR\watchdog.log" -Value ("{0} {1}" -f (Get-Date -Format o), $m) }
Log "watchdog started"
while ($true) {
  try {
    Log "tick"
    # 1) serve.py 保底
    $listen = netstat -ano | Select-String ":8899 .*LISTENING"
    if (-not $listen) {
      Start-Process -WindowStyle Hidden python -ArgumentList "$DIR\serve.py" -WorkingDirectory $DIR
      Log "serve.py restarted"
    }
    # 2) adb reverse 保底 (幂等重建, 兼容不支持 --list 的 adb 版本)
    & adb -s $SER reverse tcp:8899 tcp:8899 2>$null | Out-Null
    # 2b) 清除会禁用沉浸 VR 的残留调试配置 (--force-webxr-runtime=no-runtime)
    $cl = (& adb -s $SER shell "cat /data/local/tmp/chrome-command-line 2>/dev/null" 2>$null) -join ""
    if ($cl -match "no-runtime") {
      & adb -s $SER shell "mv /data/local/tmp/chrome-command-line /data/local/tmp/chrome-command-line.bak" 2>$null
      Log "removed stale chrome-command-line (webxr no-runtime)"
    }
    # 3) 唤醒检测
    $state = (& adb -s $SER shell "dumpsys display | grep mState= | head -1" 2>$null) -join ""
    $cur = if ($state -match "mState=ON") { "ON" } elseif ($state -match "mState=") { "OFF" } else { "Unknown" }
    if ($cur -ne $prev) { Log "state $prev -> $cur" }
    if ($cur -eq "ON" -and $prev -eq "OFF") {
      Start-Sleep -Seconds 2
      & adb -s $SER shell am start -a android.intent.action.VIEW -d $URL com.oculus.browser 2>$null
      Log "wake detected -> world container launched"
    }
    if ($cur -ne "Unknown") { $prev = $cur }
  } catch { Log ("error " + $_.Exception.Message) }
  Start-Sleep -Seconds 8
}
