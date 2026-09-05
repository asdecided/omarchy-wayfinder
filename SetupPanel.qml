import QtQuick
import Quickshell
import qs.Commons
import qs.Ui

Item {
  id: root
  property var service: null
  property var bar: null
  property color foreground: Color.foreground
  property string page: "provider"
  property string selectedModel: ""
  property string armedAction: ""
  signal back()
  readonly property var state: service ? service.onboarding : ({})
  readonly property bool working: !!service && service.onboardingBusy
  implicitHeight: Math.min(body.implicitHeight, Style.space(620))

  function clearSecrets() { providerKey.text = ""; armedAction = "" }
  onVisibleChanged: if (!visible) clearSecrets()

  function connectProvider() {
    if (!service || working || providerKey.text === "") return
    var value = providerKey.text
    providerKey.text = ""
    service.runOnboarding("discover", value)
  }

  function confirmAction(action) {
    if (armedAction !== action) {
      armedAction = action
      resetConfirmation.restart()
      return
    }
    armedAction = ""
    if (action === "disconnect") service.runOnboarding("disconnect")
    else service.maintain(action)
  }

  Timer { id: resetConfirmation; interval: 8000; onTriggered: root.armedAction = "" }

  Flickable {
    anchors.fill: parent
    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    Column {
      id: body
      width: parent.width
      spacing: Style.space(10)

      Row {
        spacing: Style.space(6)
        Button { text: "Back"; foreground: root.foreground; onClicked: root.back() }
        Text {
          text: "Wayfinder setup"
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.title
          font.bold: true
          anchors.verticalCenter: parent.verticalCenter
        }
      }

      Flow {
        width: parent.width
        spacing: Style.space(5)
        Repeater {
          model: [{id: "provider", label: "OpenAI"}, {id: "agents", label: "Coding agents"},
            {id: "maintenance", label: "Maintenance"}]
          Button {
            required property var modelData
            text: modelData.label
            bordered: root.page === modelData.id
            foreground: root.foreground
            onClicked: { root.clearSecrets(); root.page = modelData.id }
          }
        }
      }

      Column {
        width: parent.width
        spacing: Style.space(8)
        visible: root.page === "provider"

        Text {
          width: parent.width
          text: "1. Connect OpenAI"
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.subtitle
          font.bold: true
        }
        Text {
          width: parent.width
          text: "Use an OpenAI Platform API key. API billing is separate from a ChatGPT subscription. Your key stays in the desktop keyring; it is never written to plugin settings."
          textFormat: Text.PlainText
          wrapMode: Text.Wrap
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }
        Button {
          text: "Open API keys page"
          foreground: root.foreground
          onClicked: Qt.openUrlExternally("https://platform.openai.com/api-keys")
        }
        Text {
          width: parent.width
          visible: root.state.keyringInstalled === false
          text: "Secret store helper missing. Install libsecret and a Secret Service keyring, such as gnome-keyring, then log in with an unlocked keyring. See the setup guide."
          wrapMode: Text.Wrap
          color: Color.urgent
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }
        TextField {
          id: providerKey
          width: parent.width
          password: true
          placeholderText: "OpenAI API key"
          visible: !root.state.owned || root.state.stage === "saving-key"
          enabled: !root.working
          onAccepted: root.connectProvider()
        }
        Button {
          text: "Save key and find models"
          visible: !root.state.owned || root.state.stage === "saving-key"
          enabled: !root.working && providerKey.text.length > 0
          foreground: root.foreground
          bordered: true
          onClicked: root.connectProvider()
        }

        Text {
          width: parent.width
          text: "2. Choose a model"
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.subtitle
          font.bold: true
        }
        Text {
          width: parent.width
          text: root.state.model ? "Selected: " + root.state.model
            : "Models come from your account. The list can include models that do not support chat; the request test checks compatibility."
          textFormat: Text.PlainText
          wrapMode: Text.Wrap
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }
        TextField {
          id: modelFilter
          width: parent.width
          placeholderText: "Search available models"
          visible: root.state.stage === "model"
        }
        Column {
          width: parent.width
          visible: root.state.stage === "model"
          Repeater {
            model: (root.state.models || []).filter(function(name) {
              return name.toLowerCase().indexOf(modelFilter.text.toLowerCase()) >= 0
            }).slice(0, 12)
            Button {
              required property string modelData
              text: modelData
              foreground: root.foreground
              bordered: root.selectedModel === modelData
              enabled: !root.working
              onClicked: root.selectedModel = modelData
            }
          }
        }
        Text {
          width: parent.width
          visible: root.state.stage === "model"
          text: "Activating switches the untouched local starter policy to this one hosted model. Requests will be sent to OpenAI. No fallback or additional routing tier is added."
          wrapMode: Text.Wrap
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }
        Button {
          text: "Activate hosted routing"
          visible: root.state.stage === "model"
          enabled: !root.working && (root.state.models || []).indexOf(root.selectedModel) >= 0
          foreground: root.foreground
          bordered: true
          onClicked: root.service.runOnboarding("activate", root.selectedModel)
        }
        Button {
          text: "Refresh models"
          visible: !!root.state.owned
          enabled: !root.working
          foreground: root.foreground
          onClicked: root.service.runOnboarding("refresh-models")
        }

        Text {
          width: parent.width
          text: "3. Verify a request"
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.subtitle
          font.bold: true
        }
        Text {
          width: parent.width
          text: root.state.verifiedAt
            ? "Last successful test: " + new Date(root.state.verifiedAt * 1000).toLocaleString()
              + ". Re-test after restarting or changing credentials."
            : "Sends a short, billable test through the Router and checks its delivery receipt. No files or project content are sent."
          wrapMode: Text.Wrap
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }
        Button {
          text: "Send test request"
          enabled: !root.working && !!root.state.model
          foreground: root.foreground
          bordered: true
          onClicked: root.service.runOnboarding("test")
        }
        Button {
          text: "Connect a coding agent"
          visible: !!root.state.verifiedAt
          foreground: root.foreground
          onClicked: root.page = "agents"
        }
      }

      Column {
        width: parent.width
        spacing: Style.space(8)
        visible: root.page === "agents"
        Text {
          width: parent.width
          text: "Choose your agent to see the Router's connection instructions. Merge the settings into your existing configuration. Wayfinder does not overwrite agent files or import their keys."
          wrapMode: Text.Wrap
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }
        Flow {
          width: parent.width
          spacing: Style.space(4)
          Repeater {
            model: ["codex", "claude-code", "opencode", "pi", "aider"]
            Button {
              required property string modelData
              text: modelData
              foreground: root.foreground
              enabled: !root.working
              onClicked: root.service.loadRecipe(modelData)
            }
          }
        }
        TextEdit {
          width: parent.width
          text: root.service ? root.service.connectionRecipe : ""
          textFormat: TextEdit.PlainText
          readOnly: true
          selectByMouse: true
          wrapMode: TextEdit.Wrap
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.caption
        }
        Button {
          text: "Copy instructions"
          enabled: !!root.service && root.service.connectionRecipe !== ""
          foreground: root.foreground
          onClicked: Quickshell.execDetached(["bash", "-lc", "printf %s "
            + Util.shellQuote(root.service.connectionRecipe) + " | wl-copy"])
        }
        Text {
          width: parent.width
          text: "Ask the connected agent to reply with ‘connected’, then check a new successful receipt in Wayfinder. Repeat after reboot. See the guide for reversing each agent connection."
          wrapMode: Text.Wrap
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }
      }

      Column {
        width: parent.width
        spacing: Style.space(8)
        visible: root.page === "maintenance"
        Text {
          width: parent.width
          text: "Repair preserves the policy and keyring. Disconnect restores the untouched local starter, stops the Router to clear its cached key, and deletes only the credential saved by this setup."
          wrapMode: Text.Wrap
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }
        Button {
          text: "Repair service"
          enabled: !root.working
          foreground: root.foreground
          onClicked: root.service.runOnboarding("repair")
        }
        Button {
          text: root.armedAction === "disconnect" ? "Confirm disconnect" : "Disconnect OpenAI / replace key"
          enabled: !root.working && !!root.state.owned
          foreground: root.foreground
          onClicked: root.confirmAction("disconnect")
        }
        Text {
          width: parent.width
          text: "Router upgrades use the reviewed version bundled with this plugin update. Upgrade, rollback and recovery preserve independently installed binaries. Restart the service afterwards and repeat the request test."
          wrapMode: Text.Wrap
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }
        Flow {
          width: parent.width
          spacing: Style.space(4)
          Repeater {
            model: ["upgrade", "rollback", "recover"]
            Button {
              required property string modelData
              text: root.armedAction === modelData ? "Confirm " + modelData : modelData + " Router"
              foreground: root.foreground
              enabled: !!root.service && !root.service.busy
              onClicked: root.confirmAction(modelData)
            }
          }
        }
        Text {
          width: parent.width
          text: "To remove everything: disconnect OpenAI, reverse your agent settings, remove the service, then remove the owned binary and plugin together. Project profiles and custom files remain yours."
          wrapMode: Text.Wrap
          color: root.foreground
          font.family: root.bar.fontFamily
          font.pixelSize: Style.font.bodySmall
        }
        Button {
          text: root.armedAction === "uninstall-service" ? "Confirm service removal" : "Remove service"
          enabled: !!root.service && !root.service.busy && root.service.unitInstalled
          foreground: root.foreground
          onClicked: root.confirmAction("uninstall-service")
        }
        Button {
          text: root.armedAction === "remove-binary" ? "Confirm Router + plugin removal" : "Remove owned Router and plugin"
          enabled: !!root.service && !root.service.busy && !root.service.unitInstalled && !root.service.systemdActive
            && !root.state.owned && root.service.onboardingError === ""
          foreground: root.foreground
          onClicked: root.confirmAction("remove-binary")
        }
      }

      Text {
        width: parent.width
        visible: root.working
        text: root.service ? ({"discover": "Saving key and finding models…", "activate": "Activating selected model…",
          "test": "Sending test and checking receipt…", "repair": "Repairing service…",
          "disconnect": "Disconnecting and removing credential…", "refresh-models": "Refreshing models…",
          "status": "Checking setup…"}[root.service.onboardingAction] || "Loading instructions…") : ""
        wrapMode: Text.Wrap
        color: root.foreground
        font.family: root.bar.fontFamily
        font.pixelSize: Style.font.bodySmall
      }
      Text {
        width: parent.width
        text: root.service ? root.service.onboardingError || root.service.actionError || root.service.actionMessage : ""
        textFormat: Text.PlainText
        visible: text !== ""
        wrapMode: Text.Wrap
        color: root.service && (root.service.onboardingError !== "" || root.service.actionError !== "") ? Color.urgent : root.foreground
        font.family: root.bar.fontFamily
        font.pixelSize: Style.font.bodySmall
      }
      Button {
        text: root.working ? "Cancel operation" : "Recheck setup"
        foreground: root.foreground
        onClicked: root.working ? root.service.cancelOnboarding() : root.service.runOnboarding("status")
      }
      Button {
        text: "Open setup and recovery guide"
        foreground: root.foreground
        onClicked: Qt.openUrlExternally("https://github.com/asdecided/omarchy-wayfinder/blob/main/docs/setup.md")
      }
    }
  }
}
