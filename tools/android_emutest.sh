#!/bin/bash
# Android-starttest in de emulator (CI): starten, op PLAY en slot 1 tikken, screenshots + logcat in out/
set -x
mkdir -p out
adb install -r test.apk
adb logcat -c
adb shell am start -n nl.yf.smallgiveawaybike1/.SGBActivity
sleep 40
adb shell screencap -p /sdcard/s.png && adb pull /sdcard/s.png out/01_start.png
SZ=$(adb shell wm size | tail -1 | sed 's/.*: //' | tr -cd '0-9x'); W=${SZ%x*}; H=${SZ#*x}
if [ "$W" -lt "$H" ]; then T=$W; W=$H; H=$T; fi
echo "scherm ${W}x${H}"
# UI-coordinaten: hoogte 768, breedte UW = 768*W/H; menu's in een 1024-paneel in het midden
UW=$(( 768 * W / H ))
tapui() { adb shell input tap $(( ($1 + (UW - 1024) / 2) * W / UW )) $(( $2 * H / 768 )); }
tapui 97 712; sleep 4        # PLAY (hoofdmenu, 1e item)
adb shell screencap -p /sdcard/s.png && adb pull /sdcard/s.png out/02_slots.png
tapui 790 162; sleep 4       # slot 1: PLAY-knop
adb shell screencap -p /sdcard/s.png && adb pull /sdcard/s.png out/03_after_slot.png
sleep 25
adb shell screencap -p /sdcard/s.png && adb pull /sdcard/s.png out/04_game.png
adb shell input keyevent 4; sleep 3   # terug-knop = pauze
adb shell screencap -p /sdcard/s.png && adb pull /sdcard/s.png out/05_back.png
adb logcat -d > out/logcat.txt
grep -E " SGB |AndroidRuntime|FATAL" out/logcat.txt | head -80
adb shell pidof nl.yf.smallgiveawaybike1 && echo "APP DRAAIT NOG" || echo "APP DRAAIT NIET MEER"
