# Raye Linux theme

How Raye Linux looks: the night colours with a violet accent, the Inter
font, the wallpaper, the Raye eye as the Start button, the taskbar, the
sign-in and lock screens, the boot screen and the sounds.

- `art/` draws the eye and renders every picture (`python3 art/render.py`,
  needs Pillow and Inter); the results are committed under `assets/`
- `lookandfeel/`, `desktoptheme/`, `colors/`, `icons/` the Plasma look
- `shell/` the taskbar, Start menu and quick settings
- `sddm/`, `signin/`, `lockscreen/` sign-in and lock screens
- `plymouth/`, `grub/` the boot screens
- `style/` the window style

## Build and test

    dpkg-buildpackage -b -us -uc     # builds every part and runs all the tests
    python3 -m pytest tests/ art/    # only the Python tests, nothing to build

## License

GPL-3.0-or-later (`LICENSE`). Some files carry GPL-2.0-or-later or MIT,
as `debian/copyright` lists.

## Links

- Website: https://rayelinux.com
- Discord: https://discord.gg/TrxhUkabyB (questions, bug reports, news)
