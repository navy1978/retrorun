# retrorun-go2
libretro frontend for ODROID-GO Advance \
Use this for RG351P with rg351p-js2box available [here](https://github.com/christianhaitian/RG351P_virtual-gamepad).  For the RGB10, use the rgb10 branch.  For the RK2020, use the rk2020 branch.
# this branch adds the following features:
1) Tate mode (useful for naomi and atomiswave games)
2) It enables right analog for ParallN64 (for the moment is mapped as the DPAD but this should be changed)
3) Show FPS in the logs is -f flag is passed as parameter
4) Configuration of cores in now done via file, by default the file is in: "/storage/.config/distribution/configs/retrorun.cfg" but another file can be specified via "-c" parameter

Build
======
```
git clone https://github.com/navy1978/retrorun-go2
make
strip retrorun
```
