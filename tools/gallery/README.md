# raye-gallery

Every Qt control in every state it can be shown in, drawn by the widget
style in use, in Raye Night or Raye Day, at any display scale. It is the
board the Raye style (`style/`) and the `org.raye.ui` QML pieces (`qml/`)
are checked on, side by side with the WinUI 3 Gallery. It is a tool for
working on the theme; no package installs it.

- **Qt Widgets** pages: buttons, fields, check boxes and radios, combo
  boxes, sliders, progress, scroll bars, lists, trees, tables, menus,
  tool bars, tabs, frames, dialogs, tooltips.
- **Qt Quick** pages: the same controls through the desktop Qt Quick
  style (what Kirigami apps such as Raye Settings use), plus Kirigami's
  messages, and Raye's own QML pieces with every colour token.
- **Reference** pages: every colour of the scheme in use, the type ramp.

The bar at the top switches Night and Day live and changes the scale
(the gallery starts again at the new scale). It also says which widget
style and icon theme draw the page.

## Build and run

    cmake -S tools/gallery -B ~/.cache/raye-gallery-build -G Ninja
    cmake --build ~/.cache/raye-gallery-build
    ~/.cache/raye-gallery-build/raye-gallery --scheme day --scale 1.5

The widget style is Raye when it is installed, else Breeze (`--style` to
pick one). The colour schemes and QML pieces come from this repo.
`raye-gallery --help` lists the options.

## Pictures of every page

    tools/shoot.sh                                    # 18 pages, night and day, 100/125/150 %
    tools/shoot.sh --pages w-menus --sizes 1920x1080  # one page, a bigger screen

`shoot.sh` starts its own Xvfb, builds the gallery if needed and writes
`<page>-<scheme>-<scale>-<size>.png` into `${TMPDIR:-/tmp}/raye-gallery-shots`.
Each capture puts the pointer on one control, gives another the keyboard
focus and opens the page's menu or dialog, so hover, focus and popups
show in the pictures. The pictures are never committed.

## Tests

`ctest` runs the option parsing, the colour scheme and style choices, a
check that every page builds without a QML warning and has its hover and
focus samples, and `shoot.sh` itself on two pages (skipped without Xvfb).
