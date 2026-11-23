# Monitor robot_003 UART (115200 8N1)
param([string]$Port = "COM9", [int]$Seconds = 10)
$p = New-Object System.IO.Ports.SerialPort $Port,115200,"None",8,"One"
$p.ReadTimeout = 300
try { $p.Open() } catch { Write-Host "[ERR] open $Port failed" -ForegroundColor Red; exit 1 }
Start-Sleep -Milliseconds 150
$p.ReadExisting() | Out-Null
$n = 0
$t0 = Get-Date
while (((Get-Date) - $t0).TotalSeconds -lt $Seconds) {
  try { $s = $p.ReadLine(); if ($s) { $n++; Write-Output $s } } catch {}
}
$p.Close()
Write-Host "--- lines: $n ($Seconds s) ---"