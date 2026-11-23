#!/bin/bash
# Zet het Meta Quest-project (VR) klaar in quest-build/. Gebruikt door CI.
# Gradle-skelet: het SDL-androidsjabloon (zonder SDL zelf); OpenXR-loader + headers: de Khronos-AAR van Maven Central.
set -e
[ -d sdl ] || git clone --depth 1 -b release-2.30.9 https://github.com/libsdl-org/SDL.git sdl
P=quest-build
rm -rf $P
cp -r sdl/android-project $P
sed -i "s#mavenCentral()#mavenCentral(); maven { url 'https://maven-central.storage-download.googleapis.com/maven2/' }#g" $P/build.gradle
rm -rf $P/app/jni $P/app/src/main/java
J=$P/app/jni
mkdir -p $J/src $J/openxr/include $J/openxr/lib/arm64-v8a
cp -r Game Render Quest $J/src/
cp Quest/jni/Android.mk Quest/jni/Application.mk $J/
cp Quest/build.gradle $P/app/build.gradle
cp Quest/AndroidManifest.xml $P/app/src/main/AndroidManifest.xml
mkdir -p $P/app/src/main/java/nl/yf/smallgiveawaybike1/vr
cp Quest/QuestActivity.java $P/app/src/main/java/nl/yf/smallgiveawaybike1/vr/

# OpenXR-loader voor Android (Khronos)
# Maven Central geeft soms 429 (te veel verzoeken): ook de Google-mirror, en een paar keer opnieuw met pauze
ok=0
for try in 1 2 3 4; do
  for host in https://maven-central.storage-download.googleapis.com/maven2 https://repo1.maven.org/maven2 https://repo.maven.apache.org/maven2; do
    for v in 1.1.47 1.1.46 1.1.43 1.1.41; do
      u=$host/org/khronos/openxr/openxr_loader_for_android/$v/openxr_loader_for_android-$v.aar
      if curl -fsSL --retry 2 --retry-delay 5 -o openxr.aar "$u"; then echo "OpenXR-loader $v van $host"; ok=1; break 3; fi
    done
  done
  echo "poging $try mislukt, even wachten"; sleep 20
done
[ $ok = 1 ] || { echo "OpenXR-loader niet gevonden"; exit 1; }
rm -rf oxr && mkdir oxr && (cd oxr && unzip -q ../openxr.aar)
so=$(find oxr -path '*arm64-v8a*' -name libopenxr_loader.so | head -1)
hdr=$(dirname "$(find oxr -name openxr.h | head -1)")
echo "loader: $so  headers: $hdr"
cp "$so" $J/openxr/lib/arm64-v8a/
cp -r "$hdr" $J/openxr/include/

rm -rf $P/app/src/main/res/mipmap-*
for d in mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192; do
  mkdir -p $P/app/src/main/res/mipmap-${d%%:*}
  python3 -c "import sys; from PIL import Image; Image.open('Resources/Icon-152.png').convert('RGBA').resize((int(sys.argv[1]),)*2, Image.LANCZOS).save(sys.argv[2])" ${d##*:} $P/app/src/main/res/mipmap-${d%%:*}/ic_launcher.png
done
A=$P/app/src/main/assets
mkdir -p $A
python3 tools/linux_assets.py $A
cp Resources/sfx_*.wav Resources/vo_*.mp3 Resources/music.mp3 Resources/nl_hb.pak Resources/nl_hb_map.txt $A/
ls $J/openxr/include/openxr
