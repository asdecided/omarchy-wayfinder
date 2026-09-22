#pragma once
#include "command.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class ConnectionsPanel : public QWidget {
public:
  Command command;
  QString config, revision;
  std::function<bool()> canRun = [] { return true; };
  QJsonArray connections;
  QComboBox *existing, *provider;
  QLineEdit *id, *endpoint, *model, *key;
  QCheckBox *offline;
  QComboBox *localModels;
  QLabel *message;
  QList<QPushButton *> buttons;
  explicit ConnectionsPanel(const QString &path, QWidget *parent = nullptr)
      : QWidget(parent), config(path) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 16, 0, 0);
    auto *title = new QLabel("Manage connections");
    auto font = title->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    title->setFont(font);
    layout->addWidget(title);
    auto *intro = new QLabel(
        "Add API providers or local model servers. Keys stay in the desktop "
        "keyring; routing policy chooses when a connection is used.");
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *prepare = new QPushButton("Prepare local starter");
    layout->addWidget(prepare);
    connect(prepare, &QPushButton::clicked, this, [this, prepare] {
      if (command.busy() || !canRun())
        return;
      if (QFileInfo::exists(config)) {
        message->setText(
            "A configuration already exists. Reload connections to manage it.");
        return;
      }
      QDir().mkpath(QFileInfo(config).absolutePath());
      prepare->setEnabled(false);
      command.start("/usr/bin/wayfinder-router",
                    {"init", "--preset", "local", "--path", config}, {},
                    [this, prepare](bool ok, const QByteArray &) {
                      prepare->setEnabled(true);
                      if (ok)
                        load();
                      else
                        message->setText(
                            "Could not prepare a starter configuration.");
                    });
    });
    existing = new QComboBox;
    existing->addItem("New connection", -1);
    layout->addWidget(existing);
    auto *form = new QFormLayout;
    provider = new QComboBox;
    provider->addItems({"OpenAI API", "Anthropic API", "Google Gemini API",
                        "Ollama", "LM Studio", "Custom API"});
    form->addRow("Service", provider);
    id = new QLineEdit;
    id->setMaxLength(64);
    id->setPlaceholderText("e.g. cloud or local");
    form->addRow("Route ID", id);
    endpoint = new QLineEdit;
    endpoint->setMaxLength(2048);
    form->addRow("API endpoint", endpoint);
    model = new QLineEdit;
    model->setMaxLength(256);
    model->setPlaceholderText("Model ID from your provider");
    form->addRow("Model", model);
    key = new QLineEdit;
    key->setMaxLength(4096);
    key->setEchoMode(QLineEdit::Password);
    key->setPlaceholderText("Leave blank to retain an existing key");
    form->addRow("API key", key);
    layout->addLayout(form);
    localModels = new QComboBox;
    localModels->setPlaceholderText("Discovered local models");
    layout->addWidget(localModels);
    auto *discover = new QPushButton("Discover local model servers");
    layout->addWidget(discover);
    connect(discover, &QPushButton::clicked, this, [this, discover] {
      if (command.busy() || !canRun()) {
        message->setText(
            "Wait for the current configuration action to finish.");
        return;
      }
      discover->setEnabled(false);
      message->setText("Checking local model catalogs…");
      command.start(
          "/usr/bin/wayfinder-router", {"local", "discover", "--json"}, {},
          [this, discover](bool ok, const QByteArray &data) {
            discover->setEnabled(true);
            localModels->clear();
            auto obj = QJsonDocument::fromJson(data).object();
            if (!ok) {
              message->setText(
                  "Local discovery failed. Check the packaged Router.");
              return;
            }
            for (const auto &v : obj["candidates"].toArray()) {
              auto candidate = v.toObject();
              localModels->addItem(candidate["runtime"].toString() + " · " +
                                       candidate["model"].toString(),
                                   candidate);
            }
            message->setText(localModels->count()
                                 ? "Choose a discovered model to fill a new "
                                   "connection. Save when ready."
                                 : "No running local model server found. Start "
                                   "Ollama or LM Studio, then retry.");
          });
    });
    connect(localModels, qOverload<int>(&QComboBox::activated), this,
            [this](int index) {
              auto candidate = localModels->itemData(index).toJsonObject();
              existing->setCurrentIndex(0);
              provider->setCurrentIndex(5);
              id->setText("local");
              endpoint->setText(candidate["endpoint"].toString());
              model->setText(candidate["model"].toString());
              key->clear();
            });
    auto *row = new QHBoxLayout;
    for (const auto &name : QStringList{"Reload connections", "Save connection",
                                        "Remove connection"}) {
      auto *b = new QPushButton(name);
      buttons.append(b);
      row->addWidget(b);
    }
    layout->addLayout(row);
    auto *forget = new QPushButton("Remove saved API key");
    buttons.append(forget);
    layout->addWidget(forget);
    connect(forget, &QPushButton::clicked, this, [this] {
      if (existing->currentIndex() <= 0)
        return;
      if (QMessageBox::question(
              this, "Remove saved API key?",
              "Permanently remove this connection’s app-owned key from the "
              "desktop keyring? Restart the gateway afterwards to stop using "
              "its cached key.") == QMessageBox::Yes)
        execute({{"action", "forget-key"},
                 {"revision", revision},
                 {"id", id->text()}},
                false);
    });
    offline = new QCheckBox("Offline mode · block hosted requests");
    layout->addWidget(offline);
    auto *applyOffline = new QPushButton("Apply offline mode");
    buttons.append(applyOffline);
    layout->addWidget(applyOffline);
    message = new QLabel("Prepare setup above, then reload connections. Adding "
                         "a connection does not change Automatic routing.");
    message->setWordWrap(true);
    message->setTextFormat(Qt::PlainText);
    layout->addWidget(message);
    auto *accountNote =
        new QLabel("ChatGPT subscription sign-in and Apple Foundation Models "
                   "are not available in this Linux build. API keys are "
                   "separate from a ChatGPT subscription.");
    accountNote->setWordWrap(true);
    layout->addWidget(accountNote);
    connect(provider, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
              if (existing->currentIndex() != 0)
                return;
              QStringList urls = {
                  "https://api.openai.com/v1",
                  "https://api.anthropic.com",
                  "https://generativelanguage.googleapis.com/v1beta/openai",
                  "http://127.0.0.1:11434/v1",
                  "http://127.0.0.1:1234/v1",
                  ""};
              endpoint->setText(urls[index]);
              key->clear();
            });
    endpoint->setText("https://api.openai.com/v1");
    connect(existing, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
              key->clear();
              if (index <= 0) {
                id->clear();
                id->setEnabled(true);
                model->clear();
                return;
              }
              auto entry = connections[index - 1].toObject();
              id->setText(entry["id"].toString());
              id->setEnabled(false);
              endpoint->setText(entry["endpoint"].toString());
              model->setText(entry["model"].toString());
              provider->setCurrentIndex(
                  entry["provider"].toString() == "anthropic" ? 1 : 5);
            });
    connect(buttons[0], &QPushButton::clicked, this, [this] { load(); });
    connect(buttons[1], &QPushButton::clicked, this, [this] {
      if (revision.isEmpty()) {
        message->setText("Reload connections first.");
        return;
      }
      if (QMessageBox::question(this, "Save connection?",
                                "Save this model and its endpoint? Adding or "
                                "editing a connection does not change routing. "
                                "Restart the gateway afterwards.") !=
          QMessageBox::Yes)
        return;
      QJsonObject request{{"action", "save"},
                          {"revision", revision},
                          {"id", id->text().trimmed()},
                          {"provider", provider->currentIndex() == 1
                                           ? "anthropic"
                                           : "openai-compatible"},
                          {"endpoint", endpoint->text().trimmed()},
                          {"model", model->text().trimmed()},
                          {"key", key->text()}};
      key->clear();
      execute(request, false);
    });
    connect(buttons[2], &QPushButton::clicked, this, [this] {
      if (existing->currentIndex() <= 0)
        return;
      if (QMessageBox::question(
              this, "Remove connection?",
              "Remove this connection and its app-owned credential? "
              "Connections used by routing cannot be removed.") ==
          QMessageBox::Yes)
        execute(
            {{"action", "remove"}, {"revision", revision}, {"id", id->text()}},
            false);
    });
    connect(applyOffline, &QPushButton::clicked, this, [this] {
      if (revision.isEmpty()) {
        message->setText("Reload connections first.");
        return;
      }
      if (QMessageBox::question(
              this, "Change offline mode?",
              offline->isChecked()
                  ? "Block hosted requests after the gateway restarts?"
                  : "Permit hosted requests after the gateway restarts?") ==
          QMessageBox::Yes)
        execute({{"action", "offline"},
                 {"revision", revision},
                 {"offline", offline->isChecked()}},
                false);
    });
  }
  void load() { execute({{"action", "list"}}, true); }
  void execute(const QJsonObject &request, bool listing) {
    if (command.busy() || !canRun()) {
      message->setText("Wait for the current configuration action to finish.");
      return;
    }
    for (auto *b : buttons)
      b->setEnabled(false);
    message->setText("Working…");
    command.start("/usr/bin/wayfinder-connections", {"--config", config},
                  QJsonDocument(request).toJson(QJsonDocument::Compact),
                  [this, listing](bool ok, const QByteArray &bytes) {
                    for (auto *b : buttons)
                      b->setEnabled(true);
                    auto result = QJsonDocument::fromJson(bytes).object();
                    if (!ok || result["ok"] != true) {
                      message->setText(result["error"].toString(
                          "Could not manage connections. Check the installed "
                          "helper, configuration and keyring."));
                      return;
                    }
                    if (listing) {
                      revision = result["revision"].toString();
                      connections = result["connections"].toArray();
                      existing->clear();
                      existing->addItem("New connection", -1);
                      for (const auto &v : connections) {
                        auto entry = v.toObject();
                        existing->addItem(entry["id"].toString() + " · " +
                                          entry["model"].toString());
                      }
                      offline->setChecked(result["offline"].toBool());
                      message->setText("Connections loaded. Route IDs are used "
                                       "by the Routing editor.");
                    } else {
                      revision.clear();
                      message->setText(
                          result["message"].toString() +
                          " Reload connections before another change.");
                    }
                  });
  }
};
