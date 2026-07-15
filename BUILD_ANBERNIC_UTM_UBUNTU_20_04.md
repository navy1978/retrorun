# Building RetroRun for Anbernic on an Apple Silicon Mac

This guide creates a fast ARM64 development environment on an Apple Silicon
Mac (M1, M2, M3 or newer), using UTM and Ubuntu Server 20.04. It covers the
native GO2/DRM build used by supported Anbernic devices and the alternative
SDL2 build.

The important difference from the older x86 instructions is that the Linux VM
and the target are both ARM64. ARM64 programs therefore execute natively in
the VM; QEMU CPU emulation is not required for an ARM64 chroot.

> Ubuntu 20.04 left standard support in May 2025 and now receives extended
> updates through Ubuntu Pro/ESM. It is retained here because it is useful for
> compatibility with older handheld distributions. Do not expose this VM as a
> public server.

## 1. What was verified

The current native GO2 Makefile links these libraries:

| RetroRun requirement | Ubuntu 20.04 ARM64 source | Status |
| --- | --- | --- |
| C/C++ compiler, libc, `dl`, pthreads | `build-essential` | Available |
| DRM | `libdrm-dev` | Available for ARM64 |
| GBM | `libgbm-dev` | Available for ARM64 |
| EGL and OpenGL ES 2 | `libegl1-mesa-dev`, `libgles2-mesa-dev` | Available for ARM64 |
| Linux input/evdev | `libevdev-dev`, kernel headers | Available for ARM64 |
| PNG | `libpng-dev` | Available for ARM64 |
| ALSA | `libasound2-dev` | Available for ARM64 |
| OpenAL | `libopenal-dev` | Available for ARM64 |
| Rockchip RGA | pinned `linux-rga` source | Must be built from source |
| SDL2 alternative backend | `libsdl2-dev` | Available for ARM64 |
| GO2 support | sources under `src/go2` | Already included in RetroRun |

Canonical's Focal archive contains ARM64 packages. The relevant archive pages
include [libdrm-dev](https://launchpad.net/ubuntu/focal/%2Bpackage/libdrm-dev),
[libgbm-dev](https://launchpad.net/ubuntu/focal/%2Bpackage/libgbm-dev),
[libegl1-mesa-dev](https://launchpad.net/ubuntu/focal/%2Bpackage/libegl1-mesa-dev),
[libopenal-dev](https://launchpad.net/ubuntu/focal/%2Bpackage/libopenal-dev) and
[libsdl2-dev](https://launchpad.net/ubuntu/focal/%2Bpackage/libsdl2-dev).

The current `build/gmake/Makefile` compiles the files under `src/go2` directly
and does **not** link `libgo2.so`. The separate `libgo2` build in the historical
instructions is consequently not needed. Do not run `premake4 gmake` in the
RetroRun repository: `premake4.lua` is historical and would replace the
maintained generated Makefile with an obsolete one.

Packages from the old setup such as FFmpeg, Boost, Magics++, SDL2_ttf,
Mercurial, CMake and libcurl are not dependencies of the current RetroRun
build. They can be installed for other projects, but are deliberately omitted
here.

Availability alone does not guarantee runtime compatibility. The executable,
the core and all shared libraries must use the same ARM ABI as the target
distribution. Section 10 describes the checks to perform on the handheld.

## 2. Download UTM and Ubuntu 20.04 ARM64

Download UTM from one of its official locations:

- [UTM website](https://mac.getutm.app/)
- [UTM GitHub releases](https://github.com/utmapp/UTM/releases)

Download the official Ubuntu 20.04.5 ARM64 server installer:

- [ubuntu-20.04.5-live-server-arm64.iso](https://cdimage.ubuntu.com/ubuntu/releases/20.04/release/ubuntu-20.04.5-live-server-arm64.iso)
- [SHA256SUMS](https://cdimage.ubuntu.com/ubuntu/releases/20.04/release/SHA256SUMS)
- [Ubuntu ARM64 image directory](https://cdimage.ubuntu.com/ubuntu/releases/20.04/release/)

The ARM64 server ISO stopped at 20.04.5; this is expected. The main Ubuntu
20.04.6 download page contains AMD64 images and must not be used on the M3 for
this setup.

Verify the downloaded ISO on macOS:

```sh
cd ~/Downloads
curl -LO https://cdimage.ubuntu.com/ubuntu/releases/20.04/release/SHA256SUMS
shasum -a 256 ubuntu-20.04.5-live-server-arm64.iso
grep ubuntu-20.04.5-live-server-arm64.iso SHA256SUMS
```

The two hashes must be identical.

## 3. Create the fast ARM64 VM in UTM

1. Open UTM and select **Create a New Virtual Machine**.
2. Select **Virtualize**, not **Emulate**.
3. Select **Linux**.
4. Enable **Use Apple Virtualization** when offered.
5. Select `ubuntu-20.04.5-live-server-arm64.iso` as the boot image.
6. Assign approximately 6 CPU cores, 8 GB RAM and at least 40 GB storage.
7. Enable a shared directory or plan to use Git/SSH for source transfer.
8. Keep the default shared/NAT network.
9. Start the VM and perform the normal Ubuntu Server installation.
10. Install OpenSSH during Ubuntu setup if remote access from macOS is useful.

Do not enable x86 emulation and do not select an AMD64 ISO. UTM documents that
virtualization is used when the guest architecture matches the host:
[UTM architecture documentation](https://docs.getutm.app/settings-qemu/system/).
Apple likewise specifies ARM64 Linux images for Apple Silicon:
[Apple Linux virtualization documentation](https://developer.apple.com/documentation/virtualization/creating-and-running-a-linux-virtual-machine).

After installation, eject the virtual ISO and reboot.

## 4. Confirm that the VM is ARM64

Run inside Ubuntu:

```sh
uname -m
dpkg --print-architecture
getconf LONG_BIT
```

The expected output is:

```text
aarch64
arm64
64
```

If `uname -m` reports `x86_64`, stop here: the wrong ISO or VM mode was used.

Update the VM and enable the Universe repository:

```sh
sudo apt update
sudo apt install -y software-properties-common
sudo add-apt-repository -y universe
sudo apt update
```

Because Focal is now in ESM, Ubuntu may offer Ubuntu Pro. A Pro subscription is
not required merely to compile RetroRun, but it is recommended if the VM needs
continued security maintenance. Canonical documents the Focal support status
[here](https://ubuntu.com/security/esm).

## 5. Install the actual RetroRun build dependencies

Install the common tools and native GO2/DRM dependencies:

```sh
sudo apt install -y \
    build-essential \
    git \
    wget \
    ca-certificates \
    pkg-config \
    meson \
    ninja-build \
    libdrm-dev \
    libgbm-dev \
    libegl1-mesa-dev \
    libgles2-mesa-dev \
    libevdev-dev \
    libpng-dev \
    libasound2-dev \
    libopenal-dev
```

Install SDL2 as well if the alternative SDL2/KMSDRM backend will be built:

```sh
sudo apt install -y libsdl2-dev
```

Ubuntu 20.04 normally supplies GCC 9. Its C++20 mode is named `-std=c++2a`;
the RetroRun Linux SDL2 Makefile deliberately uses that spelling for Focal
compatibility. Newer GCC and Clang releases accept it as well.

Confirm that APT installed ARM64 libraries:

```sh
dpkg-query -W -f='${Package} ${Architecture} ${Version}\n' \
    libdrm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev \
    libevdev-dev libpng-dev libasound2-dev libopenal-dev
```

Every architecture column should say `arm64` or `all`.

The old `/usr/include/drm` symbolic link should normally be unnecessary because
the current build uses the pkg-config/include layout. If compilation explicitly
fails with a missing `drm/*.h` header, create the compatibility link only then:

```sh
sudo ln -s /usr/include/libdrm /usr/include/drm
```

## 6. Build the compatible Rockchip RGA library

RGA is hardware-specific and is not supplied by Ubuntu as the library expected
by the historical GO2 backend. Build the known-compatible revision used by the
older Anbernic toolchain instructions:

```sh
cd ~
git clone https://github.com/351ELEC/linux-rga.git
cd linux-rga
git checkout 1fc02d56d97041c86f01bc1284b7971c6098c5fb
meson setup build --prefix=/usr
ninja -C build
```

Install the library and headers:

```sh
find build -name 'librga.so*' -print
sudo ninja -C build install
sudo install -d /usr/local/include/rga
sudo install -m 0644 \
    drmrga.h \
    rga.h \
    RgaApi.h \
    RockchipRgaMacro.h \
    /usr/local/include/rga/
sudo ldconfig
```

Depending on the revision/build generator, the compiled library can be under
`build/librga.so`, `build/lib/librga.so` or another Meson-controlled path.
Using the `install` target avoids assuming one of these layouts. If this old
revision has no working install target, use the path printed by `find`; for
example:

```sh
sudo cp -av build/lib/librga.so* /usr/lib/aarch64-linux-gnu/
sudo ldconfig
```

Do not run that fallback command unless `find` actually reported
`build/lib/librga.so`.

Verify it:

```sh
find /usr/lib /usr/local/lib -name 'librga.so*' -print 2>/dev/null
ldconfig -p | grep librga
```

Run `file` on the path returned by `find`; it must report an ARM AArch64 shared
object. Building this userspace library does not require RGA hardware to be
present in the VM; using it does.
The pinned revision matters because newer variants can be incompatible with
the RGA kernel/userspace interface on these older devices.

## 7. Get and build RetroRun

Clone the repository inside the VM:

```sh
cd ~
git clone https://github.com/navy1978/retrorun.git
cd retrorun
git switch SDL2
```

If the repository is shared from macOS, enter that directory instead. Building
on the VM's virtual disk is generally faster than compiling directly on a
shared filesystem.

Build the native GO2/DRM version:

```sh
make clean
make go2 config=release
```

The resulting executable is `retrorun`.

Verify the result:

```sh
file ./retrorun
readelf -h ./retrorun | grep -E 'Class|Machine'
ldd ./retrorun
```

Expected architecture information:

```text
ELF 64-bit LSB ... ARM aarch64
Class:   ELF64
Machine: AArch64
```

`ldd` must not contain `not found`.

## 8. Build the alternative SDL2 version

With `libsdl2-dev` installed:

```sh
make sdl2 config=release
```

The output is `retrorun-sdl2`. Verify it in the same way:

```sh
file ./retrorun-sdl2
ldd ./retrorun-sdl2
```

Ubuntu's SDL2 development package is sufficient to compile the program.
Running it directly on a console-only handheld additionally requires the SDL2
library shipped by that handheld to provide the KMSDRM video driver and the
necessary rotation/device patches. Check this on the handheld with:

```sh
strings /usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0 | grep -i kmsdrm
```

The exact SDL2 path can differ by distribution. A successful Ubuntu VM build
therefore proves CPU/header compatibility, not KMSDRM runtime compatibility.

## 9. Optional: reproduce the old Debian Buster ARM64 chroot

Building directly in Ubuntu 20.04 uses its glibc (2.31). If the handheld has an
older userspace, retain Ubuntu 20.04 as the fast VM but compile inside a Debian
Buster ARM64 chroot, as in the historical workflow. Because the VM and chroot
are both ARM64, use `debootstrap`, not `qemu-debootstrap`.

Install the host tools:

```sh
sudo apt install -y debootstrap debian-archive-keyring
sudo mkdir -p /srv/chroot/debian-buster-arm64
```

Buster has moved to the Debian archive. Create the chroot:

```sh
sudo debootstrap \
    --arch=arm64 \
    --keyring=/usr/share/keyrings/debian-archive-keyring.gpg \
    buster \
    /srv/chroot/debian-buster-arm64 \
    http://archive.debian.org/debian/
```

Configure archived APT metadata and DNS:

```sh
echo 'Acquire::Check-Valid-Until "false";' | \
    sudo tee /srv/chroot/debian-buster-arm64/etc/apt/apt.conf.d/99archive
echo 'deb http://archive.debian.org/debian buster main contrib non-free' | \
    sudo tee /srv/chroot/debian-buster-arm64/etc/apt/sources.list
sudo cp -L /etc/resolv.conf /srv/chroot/debian-buster-arm64/etc/resolv.conf
```

Mount the virtual filesystems and the source directory:

```sh
sudo mkdir -p /srv/chroot/debian-buster-arm64/work/retrorun
sudo mount --bind /dev /srv/chroot/debian-buster-arm64/dev
sudo mount -t proc proc /srv/chroot/debian-buster-arm64/proc
sudo mount -t sysfs sys /srv/chroot/debian-buster-arm64/sys
sudo mount --bind "$HOME/retrorun" \
    /srv/chroot/debian-buster-arm64/work/retrorun
```

Enter the chroot:

```sh
sudo chroot /srv/chroot/debian-buster-arm64 /bin/bash
```

Inside it, confirm and install dependencies:

```sh
uname -m
dpkg --print-architecture
apt update
apt install -y \
    build-essential git ca-certificates pkg-config meson ninja-build \
    libdrm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev \
    libevdev-dev libpng-dev libasound2-dev libopenal-dev libsdl2-dev
```

Build and install the same pinned RGA revision using section 6, then build
RetroRun from `/work/retrorun` using section 7 or 8.

Exit and unmount when finished:

```sh
exit
sudo umount /srv/chroot/debian-buster-arm64/work/retrorun
sudo umount /srv/chroot/debian-buster-arm64/proc
sudo umount /srv/chroot/debian-buster-arm64/sys
sudo umount /srv/chroot/debian-buster-arm64/dev
```

Debian documents that native bootstrap is appropriate when the target and host
architectures match, while QEMU is used for foreign architectures:
[Debian ARM64 bootstrap](https://wiki.debian.org/Arm64Port) and
[Debian cross-debootstrap](https://wiki.debian.org/EmDebian/CrossDebootstrap).

## 10. Check the actual handheld before deployment

Architecture names alone are insufficient. Boot the target device and run:

```sh
uname -m
getconf LONG_BIT
ldd --version | head -n 1
file /usr/bin/retrorun 2>/dev/null || true
file /path/to/a/core_libretro.so
```

Use the results as follows:

- `aarch64` and `64`: use the ARM64 build from this guide.
- an ARM64 frontend and ARM64 core must be used together;
- a 32-bit core needs a 32-bit frontend and userspace, not this ARM64 build;
- the build's required glibc version must not be newer than the device glibc;
- prefer the `librga.so` already supplied by the firmware if it is compatible;
- never replace firmware DRM, GBM, EGL or RGA libraries without a backup.

Check the executable's required symbol versions before copying it:

```sh
readelf --version-info ./retrorun | grep GLIBC_ | sort -Vu
```

If the newest required `GLIBC_x.y` is greater than the device's glibc, build in
the older Buster chroot or, preferably, in the exact AmberELEC/ArkOS build
sysroot.

Copy the executable to a temporary path first:

```sh
scp ./retrorun user@HANDHELD_IP:/tmp/retrorun-test
```

Then, on the handheld:

```sh
chmod +x /tmp/retrorun-test
ldd /tmp/retrorun-test
```

Do not install it permanently until every library resolves and the existing
RetroRun binary has been backed up.

## 11. ARM32 (`armhf`) devices or cores

The procedure above intentionally targets ARM64. On an Apple Silicon VM,
ARM32 is a foreign userspace and may require QEMU user-mode emulation:

```sh
sudo apt install -y qemu-user-static binfmt-support debootstrap
sudo qemu-debootstrap \
    --arch=armhf \
    buster \
    /srv/chroot/debian-buster-armhf \
    http://archive.debian.org/debian/
```

This will be slower than the ARM64 chroot, but it avoids the much slower
combination of a fully emulated x86 VM plus ARM emulation. Only create this
chroot after section 10 proves that a 32-bit frontend is actually needed.

## 12. Troubleshooting

### `librga.so: cannot open shared object file`

```sh
sudo ldconfig
ldconfig -p | grep librga
```

Check that the library is under `/usr/lib/aarch64-linux-gnu` and is AArch64.

### `rga/RgaApi.h: No such file or directory`

```sh
ls -l /usr/local/include/rga/RgaApi.h
```

Repeat the header installation in section 6 if it is absent.

### `drm/drm_fourcc.h: No such file or directory`

Install `libdrm-dev`. If the old source still assumes `/usr/include/drm`, use
the compatibility symbolic link shown in section 5.

### The executable builds but does not run in the VM

This is expected for the native GO2 backend. UTM does not emulate the
handheld's Rockchip DRM display, RGA accelerator, input devices or panel. Use
the VM for compilation and static/link checks; perform the real runtime test on
the Anbernic device.

### The executable reports `GLIBC_x.y not found` on the handheld

The build environment is newer than the target userspace. Use the Buster
chroot in section 9 or the exact distribution build sysroot.

### `SDL_GetCurrentVideoDriver` does not report `KMSDRM`

The SDL2 library on the target was not built with the required KMSDRM backend,
or another driver was selected. The Ubuntu package inside the VM cannot fix
the target firmware's SDL2 build.
