# Building Guide

This guide will help you build the project on your local machine. The process will require you to provide a decompressed ROM of the US version of Goemon's Great Adventure.

These steps cover: decompressing the ROM, running the recompiler and finally building the project.

## 1. Clone the Goemon64Recomp Repository
This project makes use of submodules so you will need to clone the repository with the `--recurse-submodules` flag.

```bash
git clone --recurse-submodules
# if you forgot to clone with --recurse-submodules
# cd /path/to/cloned/repo && git submodule update --init --recursive
```

## 2. Install Dependencies

### Linux
For Linux the instructions for Ubuntu are provided, but you can find the equivalent packages for your preferred distro.

```bash
# For Ubuntu, simply run:
sudo apt-get install cmake ninja-build libsdl2-dev libgtk-3-dev lld llvm clang
```

### Windows
You will need to install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/).
In the setup process you'll need to select the following options and tools for installation:
- Desktop development with C++
- C++ Clang Compiler for Windows
- C++ CMake tools for Windows

The other tool necessary will be `make` which can be installe via [Chocolatey](https://chocolatey.org/):
```bash
choco install make
```

## 3. Decompressing the target ROM
You will need a decompressed copy of the NTSC-U Goemon's Great Adventure ROM before running the recompiler. Unlike Mystical Ninja, this game has no decompilation, so the project is recompiled straight from the disassembly produced by the [`lib/gga`](https://github.com/StopSine/gga) submodule.

Place your retail US ROM at `lib/gga/config/usa/baserom.z64`, then from `lib/gga` run:

```bash
make
```

That is all that is needed to build, because the symbol files the recompiler reads (`config/usa/gga.elf.syms.toml` and `gga.elf.datasyms.toml`) are committed.

Regenerating those symbols is only necessary if the disassembly itself changes; `make setup` splits the ROM and `make elf` links the ELF that N64Recomp's `--dump-context` reads. See [`lib/gga/README.md`](lib/gga/README.md) for that process and `lib/gga/docs/overlay_loader.md` for why the overlays need this treatment.

## 4. Generating the C code

Now that you have the required files, you must build [N64Recomp](https://github.com/Mr-Wiseguy/N64Recomp) and run it to generate the C code to be compiled. The building instructions can be found [here](https://github.com/Mr-Wiseguy/N64Recomp?tab=readme-ov-file#building). That will build the executables: `N64Recomp` and `RSPRecomp` which you should copy to the root of the Goemon64Recomp repository.

After that, go back to the repository root, and run the following commands:
```bash
./N64Recomp lib/gga/config/usa/gga.recomp.toml
./RSPRecomp aspMain.toml
```

> [!NOTE]
> `aspMain.toml` still extracts the RSP audio microcode from a Mystical Ninja ROM, which is where it was originally taken from, and expects that ROM at `mnsg.z64` in the repository root. Whether Goemon's Great Adventure ships the same microcode has not been verified. See the comment at the top of that file.

## 5. Building the Project

Finally, you can build the project! :rocket:

On Windows, you can open the repository folder with Visual Studio, and you'll be able to `[build / run / debug]` the project from there.

If you prefer the command line or you're on a Unix platform you can build the project using CMake:

```bash
cmake -S . -B build-cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -G Ninja -DCMAKE_BUILD_TYPE=Release # or Debug if you want to debug
cmake --build build-cmake --target Goemon64Recompiled -j$(nproc) --config Release # or Debug
```

## 6. Success

Voilà! You should now have a `Goemon64Recompiled` executable in the build directory! If you used Visual Studio this will be `out/build/x64-[Configuration]` and if you used the provided CMake commands then this will be `build-cmake`. You will need to run the executable out of the root folder of this project or copy the assets folder to the build folder to run it.

> [!IMPORTANT]  
> In the game itself, you should be using a standard ROM, not the decompressed one.
