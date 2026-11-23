# Flash robot_003 via UART bootloader (CH340)
param(
  [string]$Port = "COM9",
  [string]$Hex  = "D:\stm32xm\003_机器人\build\Debug\robot_003.hex",
  [int]$Baud = 115200
)
$cli = "C:\ST\STM32CubeCLT_1.22.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
if (-not (Test-Path -LiteralPath $Hex)) { Write-Host "[ERR] hex not found: $Hex" -ForegroundColor Red; exit 1 }
Write-Host "=== flash $Hex -> $Port ===" -ForegroundColor Cyan
Write-Host "Require: BOOT0=1, TXD->PA10, RXD->PA9, GND, then press RST" -ForegroundColor Yellow
$out = & $cli -c port=$Port br=$Baud -w $Hex -v -g 0x0 2>&1
if ($out -match "verified successfully") {
  Write-Host "[OK] flashed and running. Back to run mode: BOOT0=0, unplug TXD, RXD->PB10 or PA9, press RST" -ForegroundColor Green
  exit 0
} else {
  Write-Host "[FAIL] common causes:" -ForegroundColor Red
  Write-Host "  1) BOOT0 not at 1 / RST not pressed"
  Write-Host "  2) TXD not on PA10"
  Write-Host "  3) COM port busy (close serial tool)"
  Write-Host "  4) wrong COM number"
  $out | Select-Object -Last 6
  exit 1
}