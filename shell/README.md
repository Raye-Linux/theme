# Raye shell

The desktop shell of Raye Linux: taskbar, Start, quick settings, the
clock and its notifications, the window switcher, the on-screen displays
and the small apps around them. It builds into one binary package,
`raye-shell`, from this folder of the theme repo.

## Layout

Every part lives in its own folder and builds itself. This folder's
`CMakeLists.txt` finds Qt, KDE Frameworks and Plasma once, then picks the
parts up by folder, so adding a part never means editing a shared file.

| Folder | What happens to it |
|---|---|
| `<part>/CMakeLists.txt` | added with `add_subdirectory` |
| `<group>/<part>/CMakeLists.txt` | same, for group folders such as `kwin/` and `tabbox/` (a group folder has no `CMakeLists.txt` of its own) |
| `plasmoids/<id>/` with a `metadata.json` | installed as a Plasma applet package to `share/plasma/plasmoids/<id>` |
| `plasmoids/<id>/CMakeLists.txt` | a compiled applet: built with `add_subdirectory`, installs itself |
| `plasmoids/<id>/tests/CMakeLists.txt` | the applet's own tests, built only when testing, never installed |
| `po/<lang>/<domain>.po` | compiled to `.mo` and installed to `share/locale` |
| `tests/` | the checks every applet must pass (see below) |

A folder with nothing in it yet, or an applet folder with no
`metadata.json` yet, builds and installs nothing: parts can land one by
one. A `README.md`, `Messages.sh`, `tests/`, `dummydata/` or `*.qmlc` next
to an applet's QML is never installed.

QML modules built by a part go to `<build>/qml` (do not set your own
`OUTPUT_DIRECTORY`), so applets and tests find `org.raye.shell` before
it is installed. The `org.raye.ui` primitives of the theme come from
`../qml`.

## Strings

All text is in English and French. QML uses the domain forms only:
`i18nd("raye_shell_<part>", "Text")`, `i18ndc`, `i18ndp`, `i18ndcp`. Each
part has its own domain and its French file is
`po/fr/raye_shell_<part>.po`. `metadata.json` carries `Name[fr]` and
`Description[fr]`.

## Tests

`ctest` runs them, and the package build runs `ctest`.

- **plasmoidload** takes every applet of `plasmoids/` and checks:
  - `metadata.json`: an `org.raye.*` folder name, the same `Id` (a
    compiled applet has none: Plasma takes it from the plugin, so its
    target is named after the folder), `Plasma/Applet`, API version 6,
    name, description (both languages), icon, licence, version, authors,
    and a `contents/ui/main.qml` for applets made of QML;
  - `contents/config/main.xml` is a valid KConfigXT file;
  - every `i18n` call has a `raye_shell_*` domain, plain string literals,
    and a French string that is not empty, not fuzzy, and has the same
    `%1`, `%2`... as the English; no `qsTr()`;
  - the QML compiles (applets made of QML only);
  - then it runs `plasmawindowed <id>` against the installed Plasma, in
    English and in French, and fails on any QML error or warning that
    comes from the applet's own files, or when Plasma cannot load it.
- **plasmoidcheck** tests those checks on the applets in
  `tests/fixtures/`, each broken in one known way.
- **tree** builds a copy of this `CMakeLists.txt` with made-up parts of
  every kind and checks what it installs, then builds it empty.

The runs need `plasmawindowed` and `dbus-run-session` but no display:
they use the offscreen platform and a private D-Bus session that starts
no service, so nothing outlives a run. A run lasts 4 seconds per applet
and language; `RAYE_SHELL_LOAD_MS` changes that.

Build and test this folder alone:

    cmake -S shell -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure

The private Plasma QML modules some applets use have no stable API. The
package `Breaks` the next Plasma release, and this load test runs
against the Plasma installed at build time, so a Plasma upgrade only
lands with a shell rebuilt and tested against it.

## Looking at an applet

To see one applet under Xvfb, put the folder under
`<dir>/plasma/plasmoids/<id>` and run:

    XDG_DATA_DIRS=<dir>:<build>:/usr/share dbus-run-session plasmawindowed <id>

with `LANGUAGE=fr` for French. `<build>` holds the compiled translations.
