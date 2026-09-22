import QtQuick
import Quickshell
import Quickshell.Io

Item {
  id: root
  visible: false
  property var shell: null
  property var manifest: null
  property bool checked: false
  property bool reachable: false
  readonly property string statusText: !checked ? "Checking gateway…"
    : reachable ? "Gateway reachable" : "Gateway unavailable"

  // One bounded probe shared by every monitor. Only the HTTP status code is
  // collected; no provider responses, prompts, keys or routing policy enter QML.
  function refresh() {
    if (!probe.running) probe.running = true
  }
  function openApplication() { Quickshell.execDetached(["/usr/bin/wayfinder"]) }

  Process {
    id: probe
    command: ["/usr/bin/curl", "--silent", "--noproxy", "*", "--proto", "=http",
      "--connect-timeout", "2", "--max-time", "5", "--output", "/dev/null",
      "--write-out", "%{http_code}", "http://127.0.0.1:8088/healthz"]
    stdout: StdioCollector { id: response; waitForEnd: true }
    onExited: function(code) {
      root.reachable = code === 0 && response.text.trim() === "200"
      root.checked = true
    }
  }
  Timer { interval: 15000; running: true; repeat: true; triggeredOnStart: true; onTriggered: root.refresh() }
}
