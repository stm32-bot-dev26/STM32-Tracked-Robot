# Package project into a clean zip (no .idea/.vscode/build)
param(
  [string]$Project = "D:\stm32xm\003_机器人",
  [string]$OutZip  = "D:\机器人\03_无旋转版\固件_003_机器人_无旋转.zip"
)
$stage = Join-Path $env:TEMP ("pk_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $stage | Out-Null
foreach ($d in @("App","Core","Drivers","cmake","tools")) { $s = Join-Path $Project $d; if (Test-Path -LiteralPath $s) { Copy-Item -LiteralPath $s -Destination $stage -Recurse -Force } }
foreach ($f in @("CMakeLists.txt","CMakePresets.json","startup_stm32f103xb.s","STM32F103XX_FLASH.ld")) { $s = Join-Path $Project $f; if (Test-Path -LiteralPath $s) { Copy-Item -LiteralPath $s -Destination $stage -Force } }
Get-ChildItem -LiteralPath $Project -Filter "*.ioc" | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $stage -Force }
$pre = Join-Path $stage "prebuilt"
New-Item -ItemType Directory -Force -Path $pre | Out-Null
foreach ($e in @("elf","hex","bin")) { $s = Join-Path $Project ("build\Debug\robot_003." + $e); if (Test-Path -LiteralPath $s) { Copy-Item -LiteralPath $s -Destination $pre -Force } }
$readme = @"
# 003_robot v3.2 (no base rotation)

Build:
  cmake --preset Debug
  cmake --build build/Debug

Flash (UART bootloader): BOOT0=1, TXD->PA10, RXD->PA9, then run tools/flash.ps1
prebuilt/robot_003.hex matches this source exactly.
NOTE: pin map NOT compatible with Robat5 (PA8/PB0 differ). Do not cross-flash.
"@
$readme | Set-Content -LiteralPath (Join-Path $stage "README.md") -Encoding UTF8
Remove-Item -LiteralPath $OutZip -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $OutZip -CompressionLevel Optimal
Remove-Item -LiteralPath $stage -Recurse -Force
Write-Output ("ok " + [math]::Round((Get-Item -LiteralPath $OutZip).Length/1MB,2) + " MB")