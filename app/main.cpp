#include "chat.h"
#include "connections.h"
#include "overview.h"
#include "routing.h"
#include "theme.h"
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <functional>

// Presentation and desktop adapters sit over the packaged Rust Router.
// No shell commands or user-supplied executable paths are launched by this UI.
class Window : public QMainWindow {
  friend class DesktopTest;
  QProcess process;
  QTimer deadline;
  QTimer forceStop;
  QByteArray output;
  QByteArray secret;
  QString action;
  QString config =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
      "/wayfinder/wayfinder-router.toml";
#ifdef WAYFINDER_TEST
  QString router;
#else
  const QString router = "/usr/bin/wayfinder-router";
#endif
  const QString endpoint = "http://127.0.0.1:8088";
  QLabel *status;
  QLabel *stage;
  QLineEdit *key;
  QComboBox *models;
  QTextEdit *details;
  QTabWidget *tabs;
  QList<QPushButton *> actions;
  QListWidget *navigation;
  RoutingPanel *routingPanel = nullptr;
  OverviewPanel *overview;
  ChatPanel *chat;
  ConnectionsPanel *connectionsPanel = nullptr;
  QPushButton *cancelButton;
  bool cancelling = false;
  bool initializing = false;

  QPushButton *button(QVBoxLayout *layout, const QString &label,
                      std::function<void()> fn) {
    auto *b = new QPushButton(label);
    b->setMinimumHeight(34);
    layout->addWidget(b, 0, Qt::AlignLeft);
    actions.append(b);
    connect(b, &QPushButton::clicked, this, fn);
    return b;
  }
  QVBoxLayout *page(const QString &name, const QString &explanation) {
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(28, 24, 28, 24);
    l->setSpacing(12);
    auto *heading = new QLabel(name);
    auto headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 7);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    l->addWidget(heading);
    auto *text = new QLabel(explanation);
    text->setWordWrap(true);
    l->addWidget(text);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(w);
    tabs->addTab(scroll, name);
    return l;
  }
  bool confirm(const QString &text) {
    return QMessageBox::question(this, "Wayfinder", text) == QMessageBox::Yes;
  }
  void run(const QString &name, const QStringList &args, QByteArray input = {},
           const QString &program = {}) {
    if ((routingPanel && routingPanel->command.busy()) ||
        (connectionsPanel && connectionsPanel->command.busy())) {
      input.fill(0);
      status->setText("Wait for the current configuration action to finish.");
      return;
    }
    if (process.state() != QProcess::NotRunning) {
      input.fill(0);
      return;
    }
    if (program.isEmpty() && !QFileInfo::exists(router)) {
      initializing = false;
      input.fill(0);
      status->setText("The packaged Router is missing. Reinstall Wayfinder "
                      "with your package manager.");
      return;
    }
    action = name;
    output.clear();
    secret = input;
    input.fill(0);
    cancelling = false;
    for (auto *b : actions)
      b->setEnabled(false);
    cancelButton->show();
    status->setText(name + "…");
    process.setProgram(program.isEmpty() ? router : program);
    process.setArguments(args);
    deadline.start(100000);
    process.start();
  }
  void setup(const QString &verb, QByteArray input = {}) {
    QStringList args = {"setup", verb,         "--config",
                        config,  "--endpoint", endpoint};
    if (verb == "activate") {
      if (models->currentText().isEmpty()) {
        status->setText("Choose a discovered model first.");
        return;
      }
      args << "--model" << models->currentText();
    }
    if (verb == "activate" || verb == "repair" || verb == "disconnect") {
      const auto unit =
          QDir::homePath() + "/.config/systemd/user/wayfinder-router.service";
      if (QFileInfo::exists(unit) &&
          !QFile::copy(
              unit, unit + ".before-arch-" +
                        QUuid::createUuid().toString(QUuid::WithoutBraces))) {
        status->setText("Cannot back up the existing service. Action stopped.");
        input.fill(0);
        return;
      }
    }
    run(verb, args, input);
    input.fill(0);
  }
  void cancel() {
    if (process.state() == QProcess::NotRunning)
      return;
    cancelling = true;
    initializing = false;
    process.terminate();
    forceStop.start(5000);
    status->setText("Cancelling. Interrupted setup can be repaired.");
    // Native setup handles SIGTERM and cleans up its bounded subprocesses.
  }
  void complete(int code, QProcess::ExitStatus exitStatus) {
    cancelButton->hide();
    deadline.stop();
    forceStop.stop();
    output += process.readAllStandardOutput();
    secret.fill(0);
    secret.clear();
    for (auto *b : actions)
      b->setEnabled(true);
    if (cancelling) {
      status->setText("Cancelled. Use Refresh or Repair to inspect setup.");
      return;
    }
    auto doc = QJsonDocument::fromJson(output);
    auto obj = doc.object();
    bool ok = code == 0 && exitStatus == QProcess::NormalExit &&
              obj.value("ok").toBool(true);
    if (!ok) {
      initializing = false;
      status->setText(obj.value("error").toString(
          "Action failed. Check Diagnostics or repair setup."));
      // Discovery stderr is deliberately discarded, never displayed or logged.
      if (action != "discover" && action != "Project setup")
        details->setPlainText(QString::fromUtf8(output));
      return;
    }
    if (initializing) {
      initializing = false;
      setup("status");
      return;
    }
    status->setText(action == "test" ? "Request verified by the Router."
                                     : action + " completed.");
    if (obj.contains("stage")) {
      stage->setText("Setup: " + obj.value("stage").toString() +
                     "    Model: " + obj.value("model").toString());
      const auto selected = models->currentText();
      models->clear();
      for (const auto &m : obj.value("models").toArray())
        models->addItem(m.toString());
      int i = models->findText(selected);
      if (i >= 0)
        models->setCurrentIndex(i);
      if (obj.contains("keyringInstalled") &&
          !obj.value("keyringInstalled").toBool())
        status->setText("Secret Service is unavailable. Install libsecret and "
                        "unlock your desktop keyring.");
    }
    if (action != "discover" && action != "Project setup")
      details->setPlainText(QString::fromUtf8(output));
  }

protected:
  void closeEvent(QCloseEvent *e) override {
    key->clear();
    if (routingPanel->dirty &&
        QMessageBox::question(this, "Unsaved routing edits",
                              "Discard unsaved routing edits and close?") !=
            QMessageBox::Yes) {
      e->ignore();
      return;
    }
    if (chat->busy() || routingPanel->command.busy() ||
        connectionsPanel->command.busy()) {
      status->setText(
          "Stop the current chat or wait for routing changes before closing.");
      e->ignore();
      return;
    }
    if (process.state() != QProcess::NotRunning) {
      cancel();
      e->ignore();
    } else
      e->accept();
  }

public:
#ifdef WAYFINDER_TEST
  explicit Window(const QString &testRouter)
      : router(testRouter){
#else
  Window() {
#endif
            setStyleSheet(R"(
            QMainWindow { background: palette(window); }
            QTabWidget::pane { border: 0; }
            QListWidget { background: palette(alternate-base); border: 0; padding: 8px; }
            QListWidget::item { padding-left: 12px; border-radius: 6px; }
            QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }
            QPushButton { min-height: 28px; padding: 4px 12px; border: 1px solid palette(mid); border-radius: 6px; background: palette(button); }
            QPushButton:hover { background: palette(alternate-base); }
            QPushButton:disabled { color: palette(mid); }
            QLineEdit, QComboBox, QDoubleSpinBox { min-height: 28px; padding: 3px 6px; }
            QHeaderView::section { background: palette(alternate-base); padding: 8px; border: 0; border-bottom: 1px solid palette(mid); }
            QTableWidget { border: 1px solid palette(mid); border-radius: 6px; gridline-color: palette(alternate-base); }
            QGroupBox { margin-top: 10px; padding-top: 12px; }
        )");
  setWindowTitle("Wayfinder");
  resize(1120, 780);
  setMinimumSize(800, 580);
  auto *central = new QWidget;
  auto *layout = new QVBoxLayout(central);
  layout->setContentsMargins(20, 20, 20, 20);
  auto *title = new QLabel("Wayfinder");
  title->setStyleSheet("font-size:28px;font-weight:600");
  layout->addWidget(title);
  status = new QLabel("Ready · routing runs independently of this window.");
  status->setWordWrap(true);
  status->setTextFormat(Qt::PlainText);
  auto *workspace = new QHBoxLayout;
  navigation = new QListWidget;
  navigation->setFixedWidth(190);
  navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  navigation->setAccessibleName("Sections");
  workspace->addWidget(navigation);
  tabs = new QTabWidget;
  tabs->tabBar()->hide();
  workspace->addWidget(tabs, 1);
  layout->addLayout(workspace, 1);
  auto *connectionPage =
      page("Connections", "Connect accounts, APIs and local model servers. "
                          "Your desktop keyring holds the credentials.");
  auto *connectionTabs = new QTabWidget;
  connectionPage->addWidget(connectionTabs);
  auto *guided = new QWidget;
  auto *onboard = new QVBoxLayout(guided);
  onboard->setSpacing(12);
  connectionTabs->addTab(guided, "OpenAI quick start");
  stage = new QLabel("Setup has not been checked.");
  stage->setWordWrap(true);
  stage->setTextFormat(Qt::PlainText);
  onboard->addWidget(stage);
  button(onboard, "Prepare / refresh setup", [this] {
    if (!QFileInfo::exists(config)) {
      QDir().mkpath(QFileInfo(config).absolutePath());
      initializing = true;
      run("Create local starter",
          {"init", "--preset", "local", "--path", config});
    } else
      setup("status");
  });
  key = new QLineEdit;
  key->setMinimumHeight(34);
  key->setEchoMode(QLineEdit::Password);
  key->setPlaceholderText("OpenAI API key");
  key->setMaxLength(4096);
  onboard->addWidget(key);
  button(onboard, "Connect OpenAI", [this] {
    auto bytes = key->text().toUtf8();
    key->clear();
    if (bytes.isEmpty())
      return;
    setup("discover", bytes + "\n");
    bytes.fill(0);
  });
  button(onboard, "Refresh models", [this] { setup("refresh-models"); });
  models = new QComboBox;
  models->setMinimumHeight(34);
  models->setMinimumContentsLength(24);
  onboard->addWidget(models);
  button(onboard, "Activate selected model", [this] {
    if (confirm("Activate this model and restart the service at "
                "127.0.0.1:8088? The Router refuses to replace custom routing "
                "policies. Any existing service definition is backed up."))
      setup("activate");
  });
  button(onboard, "Verify a request", [this] {
    if (confirm("Send a small test request to your configured provider? Normal "
                "provider charges may apply."))
      setup("test");
  });
  connectionsPanel = new ConnectionsPanel(config);
  connectionTabs->addTab(connectionsPanel, "Manage connections");
  connect(connectionTabs, &QTabWidget::currentChanged, this, [this](int index) {
    key->clear();
    connectionsPanel->key->clear();
    if (index == 1)
      connectionsPanel->load();
  });
  onboard->addStretch();
  auto *service =
      page("Gateway", "The gateway runs as your user, independently of this "
                      "window. Start it when ready. Arch owns the executables; "
                      "your configuration and keyring stay in your account.");
  button(service, "Install / migrate and start service", [this] {
    const auto unit =
        QDir::homePath() + "/.config/systemd/user/wayfinder-router.service";
    QFile existing(unit);
    QString previous;
    if (existing.exists()) {
      if (!existing.open(QIODevice::ReadOnly)) {
        status->setText("Cannot read the existing service. Migration stopped.");
        return;
      }
      previous = QString::fromUtf8(existing.readAll());
      existing.close();
    }
    QMessageBox review(QMessageBox::Question, "Migrate Wayfinder service",
                       "Install and start the packaged Router at " + endpoint +
                           " using " + config +
                           "? Review any existing service below. A backup will "
                           "be saved beside it; configuration, credentials and "
                           "the old binary are preserved.",
                       QMessageBox::Yes | QMessageBox::No, this);
    review.setDefaultButton(QMessageBox::No);
    if (!previous.isEmpty())
      review.setDetailedText(previous);
    if (review.exec() != QMessageBox::Yes)
      return;
    if (existing.exists() &&
        !QFile::copy(unit,
                     unit + ".before-arch-" +
                         QUuid::createUuid().toString(QUuid::WithoutBraces))) {
      status->setText(
          "Cannot back up the existing service. Migration stopped.");
      return;
    }
    run("Install service", {"service", "install", "--host", "127.0.0.1",
                            "--port", "8088", "--config", config});
  });
  for (const auto &verb : QStringList{"start", "stop", "restart", "status"})
    button(service, verb.left(1).toUpper() + verb.mid(1) + " service",
           [this, verb] {
             run(verb + " service",
                 {"--user", verb, "wayfinder-router.service"}, {},
                 "/usr/bin/systemctl");
           });
  button(service, "Inspect gateway health", [this] {
    run("Gateway status", {"service", "status", "--config", config});
    tabs->setCurrentIndex(4);
  });
  button(service, "Repair provider setup", [this] {
    if (confirm("Repair interrupted setup? This may restore configuration and "
                "restart the standard service. Any existing service definition "
                "is backed up."))
      setup("repair");
  });
  button(service, "Disconnect OpenAI", [this] {
    if (confirm("Remove the setup-owned OpenAI key, restore the local starter "
                "and restart the standard service?"))
      setup("disconnect");
  });
  auto *endpoints = new QLabel("Integration endpoints");
  service->addWidget(endpoints);
  for (const auto &suffix : QStringList{"/v1", "/v1/messages", "/healthz"})
    button(service, "Copy " + endpoint + suffix, [this, suffix] {
      QApplication::clipboard()->setText(endpoint + suffix);
      status->setText("Endpoint copied.");
    });
  button(service, "Open configuration folder", [this] {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(config).absolutePath()));
  });
  service->addStretch();
  auto *agents = page("Connect an agent",
                      "Generate connection instructions for your coding agent. "
                      "This does not edit the agent’s configuration.");
  auto *agent = new QComboBox;
  agent->addItems({"codex", "claude-code", "opencode", "pi", "aider"});
  agents->addWidget(agent);
  button(agents, "Show connection instructions", [this, agent] {
    run("Connection instructions",
        {"connect", agent->currentText(), "--endpoint", endpoint});
    tabs->setCurrentIndex(4);
  });
  agents->addStretch();
  auto *project = page(
      "Project", "Choose a local repository to inspect or configure its Router "
                 "profile. Project decisions are handled by the Rust CLI.");
  auto *path = new QLineEdit;
  path->setPlaceholderText("Repository root");
  project->addWidget(path);
  button(project, "Choose repository", [this, path] {
    auto p = QFileDialog::getExistingDirectory(this, "Choose repository");
    if (!p.isEmpty())
      path->setText(p);
  });
  button(project, "Show profile", [this, path] {
    if (path->text().trimmed().isEmpty()) {
      status->setText("Choose a repository first.");
      return;
    }
    run("Project profile",
        {"project", "status", "--root", path->text(), "--json"});
    tabs->setCurrentIndex(4);
  });
  auto *token = new QLineEdit;
  token->setEchoMode(QLineEdit::Password);
  token->setPlaceholderText("Project capability token");
  project->addWidget(token);
  button(project, "Set up project profile", [this, path, token] {
    if (path->text().trimmed().isEmpty() || token->text().isEmpty()) {
      status->setText(
          "Choose a repository and enter its project capability token.");
      return;
    }
    auto bytes = token->text().toUtf8();
    token->clear();
    run("Project setup",
        {"project", "setup", "--root", path->text(), "--prompt-token",
         "--json"},
        bytes + "\n");
    bytes.fill(0);
  });
  button(project, "Roll back owned profile", [this, path] {
    if (path->text().trimmed().isEmpty())
      return;
    if (confirm("Roll back the Wayfinder-owned profile for this repository?"))
      run("Project rollback",
          {"project", "rollback", "--root", path->text(), "--json"});
  });
  connect(tabs, &QTabWidget::currentChanged, this, [this, token] {
    key->clear();
    token->clear();
    connectionsPanel->key->clear();
  });
  project->addStretch();
  auto *diag =
      page("Diagnostics",
           "Inspect the installed Router and configuration. Updates and "
           "package removal use the normal Arch package manager. Close this "
           "window before updating, then restart the service.");
  button(diag, "Check configuration", [this] {
    run("Diagnostics", {"doctor", "--config", config, "--json"});
  });
  details = new QTextEdit;
  details->setReadOnly(true);
  diag->addWidget(details);
  auto *bar = page(
      "Omarchy bar",
      "The optional bar companion comes with this app. It shows gateway status "
      "and opens Wayfinder. An existing plugin is backed up when you enable "
      "it; your routing configuration and credentials are preserved.");
  button(bar, "Enable bar companion", [this] {
    if (confirm("Enable the bar companion included with Wayfinder? Any "
                "existing Wayfinder plugin directory will be moved to a "
                "recoverable backup. Your bar placement is retained.")) {
      run("Enable bar companion", {"enable"}, {}, "/usr/bin/wayfinder-omarchy");
      tabs->setCurrentIndex(4);
    }
  });
  button(bar, "Remove bar companion", [this] {
    run("Remove bar companion", {"disable"}, {}, "/usr/bin/wayfinder-omarchy");
    tabs->setCurrentIndex(4);
  });
  button(bar, "Inspect bar integration", [this] {
    run("Bar integration", {"status"}, {}, "/usr/bin/wayfinder-omarchy");
    tabs->setCurrentIndex(4);
  });
  bar->addStretch();
  overview = new OverviewPanel;
  tabs->addTab(overview, "Overview");
  routingPanel = new RoutingPanel(router, config);
  auto *routingScroll = new QScrollArea;
  routingScroll->setWidgetResizable(true);
  routingScroll->setFrameShape(QFrame::NoFrame);
  routingScroll->setWidget(routingPanel);
  tabs->addTab(routingScroll, "Routing");
  routingPanel->canRun = [this] {
    return process.state() == QProcess::NotRunning &&
           !connectionsPanel->command.busy();
  };
  connectionsPanel->canRun = [this] {
    return process.state() == QProcess::NotRunning &&
           !routingPanel->command.busy();
  };
  chat = new ChatPanel;
  tabs->addTab(chat, "Chat");
  overview->catalogChanged = [this](const QJsonArray &models) {
    chat->setCatalog(models);
  };
  for (int index : QList<int>{6, 8, 0, 7, 1, 2, 3, 5, 4}) {
    auto *item = new QListWidgetItem(tabs->tabText(index), navigation);
    item->setData(Qt::UserRole, index);
    item->setSizeHint(QSize(0, 42));
  }
  connect(navigation, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row >= 0)
      tabs->setCurrentIndex(navigation->item(row)->data(Qt::UserRole).toInt());
  });
  connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
    for (int row = 0; row < navigation->count(); ++row)
      if (navigation->item(row)->data(Qt::UserRole).toInt() == index) {
        QSignalBlocker block(navigation);
        navigation->setCurrentRow(row);
      }
    if (index == 6)
      overview->update();
    if (index == 7 && routingPanel->mode.isEmpty()) {
      routingPanel->config = config;
      routingPanel->load();
    }
  });
  navigation->setCurrentRow(0);
  auto *shortcut = new QShortcut(QKeySequence("Ctrl+R"), this);
  connect(shortcut, &QShortcut::activated, overview,
          [this] { overview->update(); });
  cancelButton = new QPushButton("Cancel current action");
  cancelButton->hide();
  auto *footer = new QHBoxLayout;
  footer->addWidget(status, 1);
  footer->addWidget(cancelButton);
  layout->addLayout(footer);
  connect(cancelButton, &QPushButton::clicked, this, [this] { cancel(); });
  setCentralWidget(central);
  connect(&process, &QProcess::started, this, [this] {
    if (!secret.isEmpty())
      process.write(secret);
    secret.fill(0);
    secret.clear();
    process.closeWriteChannel();
  });
  connect(&process, &QProcess::readyReadStandardOutput, this, [this] {
    output += process.readAllStandardOutput();
    if (output.size() > 1024 * 1024) {
      output.clear();
      cancel();
    }
  });
  connect(&process, &QProcess::readyReadStandardError, this, [this] {
    const auto bytes = process.readAllStandardError();
    // Only the read-only native service report is written on stderr.
    // Never surface stderr from credential-bearing commands.
    if (action == "Gateway status") {
      output += bytes;
      if (output.size() > 1024 * 1024) {
        output.clear();
        cancel();
      }
    }
  });
  connect(&process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
              cancelButton->hide();
              deadline.stop();
              secret.fill(0);
              secret.clear();
              initializing = false;
              for (auto *b : actions)
                b->setEnabled(true);
              status->setText("Could not start the packaged Router.");
            }
          });
  connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, &Window::complete);
  forceStop.setSingleShot(true);
  connect(&forceStop, &QTimer::timeout, &process, &QProcess::kill);
  deadline.setSingleShot(true);
  connect(&deadline, &QTimer::timeout, this, [this] { cancel(); });
}
}
;
#ifndef WAYFINDER_TEST
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setApplicationName("wayfinder");
  app.setOrganizationName("AsDecided");
  app.setDesktopFileName("wayfinder");
  DesktopTheme theme;
  const auto socketName =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
      "/wayfinder-control";
  QLockFile instance(socketName + ".lock");
  instance.setStaleLockTime(0);
  if (!instance.tryLock()) {
    QLocalSocket socket;
    socket.connectToServer(socketName);
    if (socket.waitForConnected(1000)) {
      socket.write("activate");
      socket.waitForBytesWritten(1000);
      return 0;
    }
    qWarning("Another Wayfinder instance holds the lock but could not be "
             "activated.");
    return 1;
  }
  QLocalServer::removeServer(socketName);
  QLocalServer server;
  server.setSocketOptions(QLocalServer::UserAccessOption);
  if (!server.listen(socketName))
    qWarning() << "Window activation is unavailable:" << server.errorString();
  Window w;
  w.show();
  QObject::connect(&server, &QLocalServer::newConnection, &w, [&] {
    while (auto *socket = server.nextPendingConnection()) {
      socket->disconnectFromServer();
      socket->deleteLater();
      w.showNormal();
      w.raise();
      w.activateWindow();
    }
  });
  const auto shot = app.arguments().indexOf("--screenshot");
  if (shot >= 0 && shot + 1 < app.arguments().size())
    QTimer::singleShot(200, [&] {
      w.grab().save(app.arguments().at(shot + 1));
      app.quit();
    });
  if (app.arguments().contains("--smoke-test"))
    QTimer::singleShot(250, &app, &QApplication::quit);
  return app.exec();
}

#endif
