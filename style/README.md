# Raye widget style and window decoration

Two plugins live here:

- `kstyle/` - the **Raye** Qt widget style (`raye6.so`), installed in
  `qt6/plugins/styles/`. Applications pick it up as `Raye`, the name
  Plasma writes in `kdeglobals` under `[KDE] widgetStyle`.
- `kdecoration/` - the **Raye** window decoration (`org.raye.decoration`),
  installed in `qt6/plugins/org.kde.kdecoration3/`, with its KCM
  `kcm_rayedecoration`.

`libraye-common/` holds the shadow renderer both of them use, and
`po/fr/` the French catalogs for the two config modules.

## Where the code comes from

The code starts from KDE Breeze; `UPSTREAM` names the exact version it
was taken from. `cmake/import_upstream.py` does the import: it copies
the three folders out of a Breeze source tree and renames every plugin
id, config file, class namespace and translation domain, so the result
installs **next to** Breeze instead of replacing it. Nothing is shared
between the two: Raye reads `rayerc`, Breeze keeps `breezerc`, and a
user can switch back to Breeze at any time.

The style and the decoration share `rayerc`, the way the two Breeze
plugins share `breezerc`: its `[Common]` group holds the shadow
settings, which the decoration's settings page writes and the style
reads to draw the shadows of menus and tooltips. `[Style]` belongs to
the style, `[Windeco]` and the window exceptions to the decoration.
A group never lives in two files; `cmake/check_names.py` checks it.

The one thing that keeps its Breeze name is the `_breeze_*` family of
widget properties: applications set those on their own widgets to ask
the style for something (a frame, no separator). They are part of the
public behaviour of any Breeze-derived style, so renaming them would
silently change how other people's applications look.

`cmake/check_names.py` is a test that walks the tree and fails if any
Breeze name is left in code. Comments may say where a file comes from - 
only the names the system answers to have to be Raye's.

## Moving to a newer Breeze

Import into an **empty** folder and diff it against this one, then carry
the Raye changes over by hand:

    apt-get source breeze
    python3 cmake/import_upstream.py breeze-<version> /tmp/raye-import <source package version>
    diff -ru /tmp/raye-import style

The importer refuses to write into a folder that already has the
imported trees in it, so it can never quietly undo work.

## Building

    cmake -S . -B build -G Ninja
    cmake --build build
    ctest --test-dir build --output-on-failure

`LICENSES/` holds the licence texts the sources refer to. `LGPL-3.0-only.txt`
is not in the Breeze tarball but `kstyle/rayestyle.cpp` names it, so it
is kept here by hand; the importer leaves it alone.
