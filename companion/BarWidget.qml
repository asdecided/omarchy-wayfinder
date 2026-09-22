import QtQuick
import qs.Commons
import qs.Ui

BarWidget {
  id: root
  moduleName: "io.github.asdecided.wayfinder"
  readonly property var wayfinder: bar && bar.shell ? bar.shell.serviceFor(moduleName) : null
  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    tooltipText: "Wayfinder · " + (root.wayfinder ? root.wayfinder.statusText : "Starting…")
      + " · Click to open"
    iconComponent: Component {
      RouteMark {
        markColor: root.bar ? root.bar.barForeground : Color.foreground
        statusColor: root.wayfinder && root.wayfinder.reachable ? Color.accent : Color.urgent
        reachable: true
      }
    }
    onPressed: function(mouseButton) {
      if (!root.wayfinder) return
      if (mouseButton === Qt.RightButton) root.wayfinder.refresh()
      else root.wayfinder.openApplication()
    }
  }
}
