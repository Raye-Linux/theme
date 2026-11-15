// Points the module at the repo's pictures, for every test.
.pragma library

function assets(palette) {
    palette.assets = Qt.resolvedUrl("../../assets/signin/")
    palette.wallpaperDir = Qt.resolvedUrl("../../wallpaper/Raye/contents/images/")
    palette.eye = Qt.resolvedUrl("../../assets/logo/raye-eye.png")
}
