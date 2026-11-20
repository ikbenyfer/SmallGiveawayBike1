#!/bin/bash
# Zet de romfs (plaatjes als .raw, lettertype, geluid, stemmen, NL-kaart) en het icoon (256x256 jpg) klaar
# voor de Switch-.nro in switch-build/. Gebruikt door CI.
set -e
R=switch-build/romfs
mkdir -p $R
python3 tools/linux_assets.py $R
cp Resources/sfx_*.wav Resources/vo_*.mp3 Resources/music.mp3 Resources/nl_hb.pak Resources/nl_hb_map.txt $R/
python3 -c "from PIL import Image; Image.open('Resources/Icon-152.png').convert('RGB').resize((256,256), Image.LANCZOS).save('switch-build/icon.jpg', quality=92)"
du -sh $R
