# Upspring the Spring RTS model editor

Upspring is a model editor for .3do and .s3o files, it's able to convert from .3do to .s3o, it can handle .3ds as well.

## Features

- Model editing and texturing via it's GUI.
- Lot's of helpers which make you the life in the GUI easier.
- LUA API via the "--run" option as well as lua plugins.

## Roadmap

See [TODO.md](TODO.md)

## Install

- Linux

  Build it on your own, it's easy to so with cmake.

  ```bash
  git clone --recursive https://github.com/techannihilation/upspring.git
  cd upspring
  cmake --preset gcc-release && cmake --build --preset gcc-release
  ./out/build/gcc-release/src/Upspring
  ```

  I need to check which dependecies you need.

  On Arch Linux it crashes hard without install [Courier](https://github.com/wine-mirror/wine/raw/refs/heads/master/fonts/courier.ttf).

- Windows

  Download from the [releases](https://github.com/techannihilation/upspring/releases) page.

## CMD usage

- Open with:

  ```bash
  Upspring ~/.spring/games/TA.sdd/objects3d/cdevastator.3do
  ```

- Run lua script with arguments, everything after "--" is passed as "args" to lua.

  ```bash
  Upspring --run runscripts/convert.lua -- ~/.spring/games/TA.sdd/objects3d/cdevastator.3do
  ```

The given lua script here needs [lfs](https://lunarmodules.github.io/luafilesystem/) you can install it with [luarocks](https://github.com/luarocks/luarocks/wiki/Download).

## Authors

- [@jochumdev](https://github.com/jochumdev) - current Maintainer
- [@abma](https://github.com/abma)
- [@SpliFF](https://github.com/SpliFF/)
- kloot
- Jelmer Cnossen

## License

- GPL-2.0
- We use parts from [spring](https://github.com/spring/spring) which is GPL-2.0 licensed.
- And the following libraries:
  | Name | License |
  |------------------------------------------------------|---------------|
  | [7zip](https://7-zip.org/sdk.html) | public domain |
  | [CLI11](https://github.com/CLIUtils/CLI11) | BSD 3-Clause |
  | [fltk2](vendor/fltk2/COPYING) | LGPL-2 |
  | [lib3ds](https://github.com/techannihilation/lib3ds) | LGPL-2 |
  | [minizip-ng](https://github.com/zlib-ng/minizip-ng) | zlib license |
  | [simdutf](https://github.com/simdutf/simdutf) | Apache 2.0 |
  | [spdlog](https://github.com/gabime/spdlog) | MIT |
  | [TXPK](https://github.com/Seng3694/TXPK) | MIT |
  | [OpenIL](https://openil.sourceforge.net/) | GPL-2 |
