# Building Guide

This guide will help you build the project on your local machine. The process will require you to provide a decompressed ROM of the US version of Goemon's Great Adventure.

These steps cover: decompressing the ROM, running the recompiler and finally building the project.

## 1. Clone the GGA64Recomp Repository
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

Now that you have the required files, you must build [N64Recomp](https://github.com/Mr-Wiseguy/N64Recomp) and run it to generate the C code to be compiled. The building instructions can be found [here](https://github.com/Mr-Wiseguy/N64Recomp?tab=readme-ov-file#building). That will build the executables: `N64Recomp` and `RSPRecomp` which you should copy to the root of the GGA64Recomp repository.

After that, go back to the repository root, and run the following commands:
```bash
./N64Recomp lib/gga/config/usa/gga.recomp.toml
./RSPRecomp aspMain.toml
```

Both read the decompressed ROM produced in step 3, so nothing else needs supplying.

## 5. Building the Project

Finally, you can build the project! :rocket:

On Windows, you can open the repository folder with Visual Studio, and you'll be able to `[build / run / debug]` the project from there.

If you prefer the command line or you're on a Unix platform you can build the project using CMake:

```bash
cmake -S . -B build-cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -G Ninja -DCMAKE_BUILD_TYPE=Release # or Debug if you want to debug
cmake --build build-cmake --target GGA64Recompiled -j$(nproc) --config Release # or Debug
```

## 6. Success

Voilà! You should now have a `GGA64Recompiled` executable in the build directory! If you used Visual Studio this will be `out/build/x64-[Configuration]` and if you used the provided CMake commands then this will be `build-cmake`. You will need to run the executable out of the root folder of this project or copy the assets folder to the build folder to run it.

> [!IMPORTANT]  
> In the game itself, you should be using a standard ROM, not the decompressed one.

## Continuous integration

`.github/workflows/validate.yml` runs the same steps as above. Everything the recompiler reads is committed — the symbol files live in the `lib/gga` submodule, so there is no separate symbols repository — with one exception: the ROM, which cannot be in a public repository.

Each job therefore checks out a private repository, `GGA64RecompSecrets`, first. That needs a `SECRETS_TOKEN` repository secret: a fine-grained personal access token with `Contents: Read` on that repository and nothing else. It is passed to `actions/checkout` rather than embedded in a clone URL, with `persist-credentials: false` so it is not left in `.git/config` for a later step to archive.

The private repository's contents are copied over the checkout recursively, so it mirrors this repository's layout, and one file is all it needs to contain:

```
lib/gga/config/usa/baserom.decompressed.z64
```

Both `gga.recomp.toml` and `aspMain.toml` read it, the former because its paths resolve relative to the config file rather than the working directory.
