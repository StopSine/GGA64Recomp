# Goemon's Great Adventure: Recompiled

This project uses [N64: Recompiled](https://github.com/Mr-Wiseguy/N64Recomp) to **statically recompile** "Goemon's Great Adventure" into a native port, with [RT64](https://github.com/rt64/rt64) as the rendering engine.

It is a fork of [Goemon64Recomp](https://github.com/klorfmorf/Goemon64Recomp), which targets Mystical Ninja Starring Goemon. That game is no longer built here — this repository is Goemon's Great Adventure only. If you want Mystical Ninja, use the upstream project.

### **This repository and its releases do not contain game assets. The original game is required to build or run this project.**

## Status

This is a work in progress, not a finished port. Goemon's Great Adventure has no decompilation, so it is recompiled directly from disassembly, and progress is tracked by what has been made to work rather than by a feature list.

**Working**
* Boots and plays, with controls and audio
* Saving, via the Controller Pak backed by the save file
* Widescreen in-level, on the world map, and for the screen fades, with the vertical overscan padding removed
* Aspect ratio can be changed while playing and takes effect immediately
* 4:3 is preserved and unaffected by the widescreen patches

**Known gaps**
* The logo screens and the main menu are still pillarboxed in widescreen. Their scissors are already full width, so this is unstretched 2D texture rectangles rather than clipping.
* Entities activate, and enemies respawn, based on distance rather than what is on screen, so in widescreen you can see inactive enemies near the edges and respawns can happen in view. Widening the game's own field of view does not fix this; the activation logic never consults the projection.
* Rumble is unavailable. The Controller Pak and the Rumble Pak occupy the same slot and saving takes priority.

## System Requirements
A GPU supporting Direct3D 12.0 (Shader Model 6), Vulkan 1.2, or Metal Argument Buffers Tier 2 support is required to run this project. The oldest GPUs that should be supported for each vendor are:
* GeForce GT 630
* Radeon HD 7750 (the one from 2012, not to be confused with the RX 7000 series) and newer
* Intel HD 510 (Skylake)
* A Mac with Apple Silicon or an Intel 7th Gen CPU with MacOS 13.0+

On x86-64 PCs, a CPU supporting the SSE4.1 instruction set is also required (Intel Core 2 Penryn series or AMD Bulldozer and newer). ARM64 builds will work on any ARM64 CPU.

If you have issues with crashes on startup, make sure your graphics drivers are fully up to date.

## Features

These come from the recompilation framework and RT64, and apply here except where the status section above says otherwise.

#### Plug and Play
Provide your copy of the North American version of the game in the main menu and start playing. Assets are loaded from the copy you provide, so there is no separate extraction step.

#### Fully Intact N64 Effects
RT64 renders the game's graphical effects as they were on the N64 rather than approximating them, including framebuffer effects, decals such as shadows, accurate lighting and shading.

#### High Framerate Support
Play at any framerate, defaulting to your monitor's refresh rate. You can also play at the game's original framerate if you prefer. **Changing framerate has no effect on gameplay.**

**Note**: External framerate limiters (such as the NVIDIA Control Panel) are known to potentially cause problems, so if you notice any stuttering then turn them off and use the manual framerate slider in the in-game graphics menu instead.

#### Widescreen Support
Any aspect ratio is supported. Widescreen needs per-game patches to look correct, so see the status section for where that currently stands.

#### Easy-to-Use Menus
Gameplay, graphics, input and audio settings are all configurable from the in-game config menu, usable with mouse, controller or keyboard.

#### Low Input Lag and Instant Load Times
Running natively removes most of the input latency and load times of the original hardware.

#### Linux and Steam Deck Support
A Linux binary as well as a Flatpak can be built for most up-to-date distros, including the Steam Deck. To play on Steam Deck, extract the Linux build onto your deck. Then, in desktop mode, right click the GGA64Recompiled executable file and select "Add to Steam". From there you can return to Gaming mode and configure the controls as needed.

## FAQ

#### What is static recompilation?
Static recompilation is the process of automatically translating an application from one platform to another. For more details, check out the full description of how this project's recompilation works here: [N64: Recompiled](https://github.com/Mr-Wiseguy/N64Recomp).

#### Is this based on a decompilation?
No. Static recompilation bypasses the need for decompiled source code, which is what makes this game portable at all: unlike Mystical Ninja, Goemon's Great Adventure has no decompilation project. Every modification here is made by patching recompiled functions by name, working from disassembly and symbol names rather than source.

#### How do I set up gyro aiming on Steam Deck?
This project provides mouse aiming as a way to allow using gyro on Steam Deck, as the Steam Deck's gyro sensors cannot be read directly. First, launch the game in Gaming Mode, press the Steam button and go to "Controller Settings". Choose "Controller Settings" again in the menu that follows, and then set "Gyro Behavior" to "As Mouse".

![Controller Settings menu](docs/deck_gyro_1.jpg)

You'll probably also want to change the default behavior so that you don't need to be touching the right stick to allow gyro input. To do so, click on the Gear icon to the right of "Gyro Behavior" and ensure that "Gyro Activation Buttons" is set to "None Selected (Gyro Always On)." If this isn't the case, then select that option and then press "Select None" in the following menu.

#### Where is the savefile stored?
- Windows: `%LOCALAPPDATA%\GGA64Recomp\saves`
- Linux: `~/.config/GGA64Recomp/saves`
- macOS: `~/Library/Application Support/GGA64Recomp/saves`

#### How do I choose a different ROM?
**You don't.** This project is only a port of Goemon's Great Adventure, and it will only accept one specific ROM: the US version of the N64 release. ROMs in formats other than .z64 will be automatically converted, as long as it is the correct ROM. **It is not an emulator and it cannot run any arbitrary ROM.**

#### Can you run this project as a portable application?
Yes, if you place a file named `portable.txt` in the same folder as the executable then this project will run in portable mode. In portable mode, the save files, config files, and mods are placed in the same folder as the executable.

## Known Issues
* In widescreen, the flat background of most levels can appear and disappear at the left and right edges of the screen. The background is drawn as tall images tiled side by side, and the game draws only the ones that cover a 4:3 view. That choice is made inside the routine that draws it, rather than from any setting or field that can be widened, so it is unfixed for now. Terrain and platform pop-in, which had a separate cause, is fixed. Use the original aspect ratio to avoid it.
* Overlays such as MSI Afterburner and other software such as Wallpaper Engine can cause performance issues with this project that prevent the game from rendering correctly. Disabling such software is recommended.

## Building
Instructions on how to build this project can be found in the [BUILDING.md](BUILDING.md) file. Building requires a copy of the game, and generating the disassembly it recompiles from is a separate step handled by the [`lib/gga`](https://github.com/StopSine/gga) submodule.

## Libraries Used and Projects Referenced
* [Goemon64Recomp](https://github.com/klorfmorf/Goemon64Recomp) for the Mystical Ninja recompilation this is forked from
* Also used from above is the icon/background graphic designed by [Jingleboy of Goemon International](https://goemoninternational.com)
* [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp) for the base upon which that project was built
* [RT64](https://github.com/rt64/rt64) for the project's rendering engine
* [RmlUi](https://github.com/mikke89/RmlUi) for building the menus and launcher
* [lunasvg](https://github.com/sammycage/lunasvg) for SVG rendering, used by RmlUi
* [FreeType](https://freetype.org/) for font rendering, used by RmlUi
* [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue) for semaphores and fast, lock-free MPMC queues
* [Gamepad Motion Helpers](https://github.com/JibbSmart/GamepadMotionHelpers) for sensor fusion and calibration algorithms
* [Ares emulator](https://github.com/ares-emulator/ares) for RSP vector instruction reference implementations, used in RSP recompilation
