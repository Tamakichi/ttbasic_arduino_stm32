set dev=COM5
stm32flash.exe -b 115200 -f -v -w %1 %dev%
