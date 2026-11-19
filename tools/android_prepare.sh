#!/bin/bash
# Zet het Android-project klaar in android-build/ (SDL2-sjabloon + game + assets). Gebruikt door CI.
set -e
git clone --depth 1 -b release-2.30.9 https://github.com/libsdl-org/SDL.git sdl
git clone --depth 1 -b release-2.8.0 https://github.com/libsdl-org/SDL_mixer.git sdl_mixer
P=android-build
cp -r sdl/android-project $P
# reserve voor Maven Central (die geeft soms tijdelijk "Could not find")
sed -i "s#mavenCentral()#mavenCentral()
        maven { url 'https://maven-central.storage-download.googleapis.com/maven2/' }#g" $P/build.gradle
grep -n "maven" $P/build.gradle
J=$P/app/jni
rm -rf $J/src $J/SDL $J/SDL_mixer $J/CMakeLists.txt
ln -s "$PWD/sdl" $J/SDL
ln -s "$PWD/sdl_mixer" $J/SDL_mixer
mkdir -p $J/src
cp -r Game Render Win $J/src/
cp Android/jni/Android.mk Android/jni/Application.mk $J/
cp Android/jni/src/Android.mk $J/src/
cp Android/build.gradle $P/app/build.gradle
cp Android/AndroidManifest.xml $P/app/src/main/AndroidManifest.xml
mkdir -p $P/app/src/main/java/nl/yf/smallgiveawaybike1
cp Android/SGBActivity.java $P/app/src/main/java/nl/yf/smallgiveawaybike1/
rm -rf $P/app/src/main/res/mipmap-*
for d in mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192; do
  mkdir -p $P/app/src/main/res/mipmap-${d%%:*}
  python3 -c "import sys; from PIL import Image; Image.open('Resources/Icon-152.png').convert('RGBA').resize((int(sys.argv[1]),)*2, Image.LANCZOS).save(sys.argv[2])" ${d##*:} $P/app/src/main/res/mipmap-${d%%:*}/ic_launcher.png
done
A=$P/app/src/main/assets
mkdir -p $A
python3 tools/linux_assets.py $A
cp Resources/sfx_*.wav Resources/music.mp3 Resources/nl_hb.pak Resources/nl_hb_map.txt $A/
ls $A | head -50
