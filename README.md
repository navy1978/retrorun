# retrorun-go2 for RG351 M/P/V/MP 
libretro frontend for ODROID-GO Advance \
Use this for RG351* with rg351p-js2box available [here](https://github.com/christianhaitian/RG351P_virtual-gamepad).

# This branch adds the following features:
1) Tate mode (useful for naomi and atomiswave games)
2) It enables right analog for ParallN64 (with C buttons management)
3) Show FPS in the logs is -f flag is passed as parameter
4) Show FPS on screen with button combination SELECT +Y
5) Configuration of cores is now done via file, by default the file is in: "/storage/.config/distribution/configs/retrorun.cfg" but another file can be specified via "-c" parameter
6) Manages different aspect ratio (as parameters)
7) Manages GPIO/USB joypad

Build
======
```
git clone https://github.com/navy1978/retrorun-go2
make
strip retrorun
```
