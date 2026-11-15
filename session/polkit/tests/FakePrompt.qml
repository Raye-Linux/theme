// Stands for the agent's Prompt and Request objects in the tests and the
// preview: same names, and it records what the prompt asked of it.
import QtQuick

QtObject {
    id: fake

    property string appName: "Settings"
    property string appIcon: ""
    property string publisher: "Raye Linux"
    property string program: "/usr/bin/raye-settings"
    property string command: ""
    property string actionId: "org.freedesktop.timedate1.set-timezone"
    property string message: "Authentication is required to set the system timezone."
    property var users: [
        { user: "techoraye", name: "Techoraye", face: "" }
    ]
    property string user: users.length > 0 ? users[0].user : ""
    property bool userIsSelf: true

    property QtObject request: QtObject {
        property int state: 0
        property bool busy: false
        property bool wrongPassword: false
        property string question: ""
        property string message: ""

        property var accepted: []
        property int rejected: 0
        property int touched: 0

        function accept(answer) {
            accepted = accepted.concat([answer])
        }
        function reject() {
            rejected += 1
        }
        function touch() {
            touched += 1
        }
        // what the agent does after a wrong password
        function failOnce() {
            wrongPassword = true
            busy = false
            state = 1
            state = 0
        }
    }
}
