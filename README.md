# 003_robot v3.2 (no base rotation)

Build:
  cmake --preset Debug
  cmake --build build/Debug

Flash (UART bootloader): BOOT0=1, TXD->PA10, RXD->PA9, then run tools/flash.ps1
prebuilt/robot_003.hex matches this source exactly.
NOTE: pin map NOT compatible with Robat5 (PA8/PB0 differ). Do not cross-flash.
