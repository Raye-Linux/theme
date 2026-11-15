// SPDX-License-Identifier: GPL-3.0-or-later
// Raye's animated wallpaper: a video (mp4, webm) or a GIF, silent and
// looping. It rests -- the last frame stays -- while a maximized or
// full-screen window covers this screen, on battery, and when the
// desktop is hidden. After 30 s of rest the file is let go and the still
// picture shown instead, so the decoder's memory goes back. Videos go through Qt Multimedia's FFmpeg backend, which
// decodes on the graphics card (VA-API) when the machine has a driver.
import QtQuick
import QtMultimedia
import org.kde.plasma.plasmoid
import org.kde.plasma.plasma5support as P5Support
import org.kde.taskmanager as TaskManager
import "rest.js" as Rest

WallpaperItem {
    id: root

    readonly property url source: root.configuration.Source
    readonly property bool gif: Rest.isGif(source)
    readonly property int maxFps: root.configuration.MaxFps
    readonly property int fill: root.configuration.FillMode

    // what the tests look at
    readonly property bool moving: gif ? stepper.running : player.playbackState === MediaPlayer.PlayingState
    readonly property int frameInterval: stepper.interval
    readonly property url playerSource: player.source
    readonly property url gifSource: gifView.source
    readonly property bool stillShown: still.visible

    property int releaseAfter: 30000
    property bool released: false
    // a grab that answers after the desktop came back must not let go
    property int releaseTicket: 0
    // the tests answer it themselves, later
    property var grabFrame: done => video.grabToImage(r => done(r.url))

    property bool covered: false
    property bool fullScreen: false
    readonly property bool onBattery: power.data["Battery"] !== undefined
                                      && power.data["Battery"]["Has Battery"] === true
                                      && power.data["AC Adapter"] !== undefined
                                      && power.data["AC Adapter"]["Plugged in"] === false
    readonly property bool resting: Rest.shouldRest({
        visible: root.visible && Window.window !== null && Window.window.visible,
        fullScreen: root.fullScreen,
        covered: root.covered,
        pauseCovered: root.configuration.PauseCovered,
        onBattery: root.onBattery,
        pauseOnBattery: root.configuration.PauseOnBattery
    })

    P5Support.DataSource {
        id: power
        engine: "powermanagement"
        connectedSources: root.configuration.PauseOnBattery ? ["Battery", "AC Adapter"] : []
    }

    TaskManager.VirtualDesktopInfo { id: desktops }
    TaskManager.ActivityInfo { id: activities }
    TaskManager.TasksModel {
        id: tasks
        groupMode: TaskManager.TasksModel.GroupDisabled
        filterByScreen: true
        // the attached Screen says the first screen on every screen here;
        // the containment the wallpaper sits in knows which one it is
        screenGeometry: root.parent && root.parent.screenGeometry !== undefined
                        ? root.parent.screenGeometry
                        : Qt.rect(Screen.virtualX, Screen.virtualY, Screen.width, Screen.height)
        filterByVirtualDesktop: true
        virtualDesktop: desktops.currentDesktop
        filterByActivity: true
        activity: activities.currentActivity
        onDataChanged: look.restart()
        onCountChanged: look.restart()
        onRowsInserted: look.restart()
        onRowsRemoved: look.restart()
    }
    // windows move and resize in bursts; look once the burst is over
    Timer {
        id: look
        interval: 150
        onTriggered: {
            const windows = []
            let full = false
            for (let i = 0; i < tasks.count; ++i) {
                const idx = tasks.index(i, 0)
                const w = {
                    maximized: tasks.data(idx, TaskManager.AbstractTasksModel.IsMaximized) === true,
                    fullScreen: tasks.data(idx, TaskManager.AbstractTasksModel.IsFullScreen) === true,
                    minimized: tasks.data(idx, TaskManager.AbstractTasksModel.IsMinimized) === true
                }
                windows.push(w)
                if (w.fullScreen && !w.minimized)
                    full = true
            }
            root.covered = Rest.covered(windows)
            root.fullScreen = full
        }
    }
    Component.onCompleted: look.start()

    Timer {
        id: letGo
        interval: root.releaseAfter
        running: root.resting && !root.released
        onTriggered: root.release()
    }

    // shown while the file opens, in its place when it can't be read, and
    // during a long rest (the last frame when there is no still picture)
    Image {
        id: still
        anchors.fill: parent
        source: root.configuration.Still
        fillMode: root.fill
        asynchronous: true
        sourceSize: Qt.size(root.width * Screen.devicePixelRatio, root.height * Screen.devicePixelRatio)
        visible: root.released || (root.gif ? gifView.status !== AnimatedImage.Ready
                                            : player.playbackState === MediaPlayer.StoppedState
                                              || player.error !== MediaPlayer.NoError)
    }
    Image {
        id: lastFrame
        anchors.fill: parent
        visible: root.released && still.status !== Image.Ready
        fillMode: Image.Stretch
    }

    // --- video ---
    MediaPlayer {
        id: player
        source: root.gif || root.released ? "" : root.source
        loops: MediaPlayer.Infinite
        videoOutput: video
        // no audioOutput: a wallpaper is silent
        onMediaStatusChanged: if (mediaStatus === MediaPlayer.LoadedMedia) root.apply()
    }
    VideoOutput {
        id: video
        anchors.fill: parent
        visible: !root.gif
        fillMode: root.fill === Image.PreserveAspectFit ? VideoOutput.PreserveAspectFit
                : root.fill === Image.Stretch ? VideoOutput.Stretch : VideoOutput.PreserveAspectCrop
    }

    // --- GIF: stepped by a timer, so a 60 fps GIF is drawn at the cap ---
    AnimatedImage {
        id: gifView
        anchors.fill: parent
        visible: root.gif && status === AnimatedImage.Ready
        source: root.gif && !root.released ? root.source : ""
        fillMode: root.fill
        cache: false
        playing: false
        // the GIF's own pace, read from one turn of QMovie before we take over
        property real natural: 10
        property real carry: 0
        onStatusChanged: if (status === AnimatedImage.Ready) measure.begin()
    }
    QtObject {
        id: measure
        property double started: 0
        property int frames: 0
        function begin() {
            frames = 0
            started = Date.now()
            gifView.playing = true
        }
    }
    Connections {
        target: gifView
        enabled: gifView.playing
        function onCurrentFrameChanged() {
            if (++measure.frames < Math.min(12, gifView.frameCount))
                return
            const seconds = (Date.now() - measure.started) / 1000
            gifView.natural = seconds > 0 ? measure.frames / seconds : 10
            gifView.playing = false
            root.apply()
        }
    }
    Timer {
        id: stepper
        interval: Rest.interval(gifView.natural, root.maxFps)
        repeat: true
        onTriggered: {
            const n = Rest.nextFrame(gifView.currentFrame, gifView.frameCount, gifView.carry,
                                     gifView.natural, 1000 / interval)
            gifView.carry = n.carry
            gifView.currentFrame = n.frame
        }
    }

    function release() {
        const ticket = ++root.releaseTicket
        const drop = url => {
            if (ticket !== root.releaseTicket || !root.resting || root.released)
                return
            if (url)
                lastFrame.source = url
            root.released = true
            player.stop()
        }
        if (!root.gif && String(root.configuration.Still) === "" && video.width > 0)
            root.grabFrame(drop)
        else
            drop("")
    }

    function apply() {
        if (root.gif) {
            stepper.running = !root.resting && gifView.status === AnimatedImage.Ready && !gifView.playing
        } else if (root.resting) {
            player.pause()
        } else {
            player.play()
        }
    }
    onRestingChanged: {
        if (!resting)
            ++releaseTicket
        if (!resting && released) {
            released = false
            lastFrame.source = ""
        }
        apply()
    }
    onSourceChanged: apply()
}
