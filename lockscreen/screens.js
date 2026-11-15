// The one LockShare of the greeter. The greeter's views (one per screen)
// share one QML engine, and a library script is loaded once per engine,
// so every view that asks gets the same object. It is made here, not in
// a view, so it does not go away with the screen that asked first.
.pragma library

var _share = null

// url: where LockShare.qml is (made on the first call)
function share(url) {
    if (_share === null) {
        var component = Qt.createComponent(url)
        if (component.status !== 1 /* Component.Ready */) {
            console.warn("LockShare:", component.errorString())
            return null
        }
        _share = component.createObject(null)
    }
    return _share
}
