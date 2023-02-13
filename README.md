# Upspring the Spring RTS model editor

Upspring is a model editor for .3do and .s3o files, it's able to convert from .3do to .s3o, it can handle .3ds as well.

## Features

- Model editing and texturing via it's GUI.
- LUA API via the "--run" option as well as lua plugins.

## CMD usage

- Open with:

    ```bash
    Upspring ~/.spring/games/TA.sdd/objects3d/cdevastator.3do
    ```

- Run lua script with arguments, everything after "--" is passed as "args" to lua.

    ```bash
    Upspring --run runscripts/convert.lua -- ~/.spring/games/TA.sdd/objects3d/cdevastator.3do
    ```


## Authors

- [@jochumdev](https://github.com/jochumdev) - current Maintainer
- [@abma](https://github.com/abma)
- [@SpliFF](https://github.com/SpliFF/)
- kloot
- Jelmer Cnossen

## License

GPLv2

We use parts from [spring](https://github.com/spring/spring) which is GPL v2 licensed.

And the following libraries:

| Name                                                 | License       |
|------------------------------------------------------|---------------|
| [7zip](https://7-zip.org/sdk.html)                   | public domain |
| [CLI11](https://github.com/CLIUtils/CLI11)           | BSD 3-Clause  |
| [fltk2](vendor/fltk2/COPYING)                        | LGPL-2.1      |
| [lib3ds](https://github.com/techannihilation/lib3ds) | LGPL-2.1      |
| [minizip-ng](https://github.com/zlib-ng/minizip-ng)  | zlib license  |
| [spdlog](https://github.com/gabime/spdlog)           | MIT           |
