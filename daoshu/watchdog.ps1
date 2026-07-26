# daoshu watchdog - wake-to-world
$SER = "2G0YC5ZG8L08Z7"
$URL = "http://localhost:8899/index.html"
$DIR = "c:\Users\zhouyoukang\dao-workspace\daoshu"
$prev = "Unknown"
function Log($m){ Add-Content -Path "$DIR\watchdog.log" -Value ("{0} {1}" -f (Get-Date -Format o), $m) }
Log "watchdog started SER=[$SER]"
while ($true) {
  try {
    $listen = netstat -ano | Select-String ":8899 .*LISTENING"
    if (-not $listen) {
      Start-Process -WindowStyle Hidden python -ArgumentList "$DIR\serve.py" -WorkingDirectory $DIR
      Log "serve.py restarted"
    }
    cmd /c "adb -s $SER reverse --list > $DIR\rev.tmp 2>&1" | Out-Null
    $rev = (Get-Content "$DIR\rev.tmp" -ErrorAction SilentlyContinue) -join ""
    if ($rev -notmatch "8899") { cmd /c "adb -s $SER reverse tcp:8899 tcp:8899" | Out-Null; Log "reverse re-established" }
    cmd /c "adb -s $SER shell dumpsys display 2>nul | findstr mState= > $DIR\state.tmp" | Out-Null
    $state = ((Get-Content "$DIR\state.tmp" -ErrorAction SilentlyContinue) | Select-Object -First 1) -join ""
    $cur = if ($state -match "mState=ON") { "ON" } elseif ($state -match "mState=") { "OFF" } else { "Unknown" }
    if ($cur -ne $prev) { Log "state $prev -> $cur" }
    if ($cur -eq "ON" -and $prev -eq "OFF") {
      Start-Sleep -Seconds 2
      cmd /c "adb -s $SER shell am start -a android.intent.action.VIEW -d $URL com.oculus.browser" | Out-Null
      Log "wake detected -> world container launched"
    }
    if ($cur -ne "Unknown") { $prev = $cur }
  } catch { Log ("error " + $_.Exception.Message) }
  Start-Sleep -Seconds 8
}
