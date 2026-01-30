#!/bin/sh
jb="/var/jb"
ARCH="arm64"

if [ $(uname -n) = iPhone ]; then
    cc -lc -lc++ \
       *.c \
       format/*.c \
       -I../include \
       -I"$jb/usr/include" \
       -I"$theos_sdk/usr/include" \
       -L"$jb/usr/lib" \
       -L/usr/lib \
       -I/$jb/usr/include/ncursesw \
       -I./ncursesw \
       -I./xun-kernel-include \
       -Wl,-undefined,dynamic_lookup \
       -o "pv" && ldid -M -Hsha256 -Sens.plist pv && mv pv build/
else
    echo "[Error]: this not iPhone"
    exit 1
fi