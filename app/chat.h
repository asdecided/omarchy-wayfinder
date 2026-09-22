#pragma once
#include "gateway.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextEdit>
#include <QVBoxLayout>

class ChatPanel : public QWidget {
  Gateway gateway;
  QPointer<QNetworkReply> reply;
  bool stopped = false;
  bool historyReadFailed = false;
  QJsonArray history;
  QString lastPrompt;
  QJsonArray conversations;
  int current = -1;
  QString storage =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
      "/wayfinder/chat.json";
  QComboBox *threads;
  QCheckBox *remember;

public:
  QTextEdit *transcript, *receipt;
  QPlainTextEdit *draft;
  QComboBox *destination;
  QComboBox *turns;
  QPushButton *send, *stop, *retry, *clear;
  QLabel *status;
  explicit ChatPanel(QWidget *parent = nullptr) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    auto *title = new QLabel("Chat");
    auto font = title->font();
    font.setPointSize(font.pointSize() + 7);
    font.setBold(true);
    title->setFont(font);
    layout->addWidget(title);
    auto *intro =
        new QLabel("Talk through your gateway. Automatic follows your routing "
                   "policy; a pinned route never silently falls back. History "
                   "stays in memory unless you enable saving on this device.");
    intro->setWordWrap(true);
    layout->addWidget(intro);
    threads = new QComboBox;
    threads->setAccessibleName("Conversations");
    threads->addItem("New conversation");
    layout->addWidget(threads);
    remember = new QCheckBox("Save conversation history on this device");
    layout->addWidget(remember);
    auto *split = new QSplitter;
    transcript = new QTextEdit;
    transcript->setReadOnly(true);
    transcript->setPlaceholderText(
        "Start a conversation with your configured models.");
    receipt = new QTextEdit;
    receipt->setReadOnly(true);
    receipt->setPlaceholderText("Routing receipt\n\nThe gateway’s decision "
                                "appears here after a response.");
    split->addWidget(transcript);
    auto *inspector = new QWidget;
    auto *inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    turns = new QComboBox;
    turns->setAccessibleName("Inspect reply routing");
    inspectorLayout->addWidget(turns);
    inspectorLayout->addWidget(receipt);
    split->addWidget(inspector);
    connect(turns, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
              if (index < 0) {
                receipt->clear();
                return;
              }
              showReceipt(turns->itemData(index).toJsonObject());
            });
    split->setSizes({600, 250});
    layout->addWidget(split, 1);
    destination = new QComboBox;
    destination->addItem("Automatic", "auto");
    destination->setAccessibleName("Chat destination");
    layout->addWidget(destination);
    draft = new QPlainTextEdit;
    draft->setPlaceholderText("Message Wayfinder…");
    draft->setMaximumHeight(120);
    draft->setAccessibleName("Message");
    layout->addWidget(draft);
    status = new QLabel(
        "Requests may use a hosted provider and incur normal charges.");
    status->setWordWrap(true);
    layout->addWidget(status);
    auto *row = new QHBoxLayout;
    clear = new QPushButton("New conversation");
    retry = new QPushButton("Retry");
    stop = new QPushButton("Stop");
    send = new QPushButton("Send");
    row->addWidget(clear);
    row->addWidget(retry);
    row->addStretch();
    row->addWidget(stop);
    row->addWidget(send);
    layout->addLayout(row);
    stop->setEnabled(false);
    retry->setEnabled(false);
    connect(send, &QPushButton::clicked, this, [this] { submit(false); });
    connect(retry, &QPushButton::clicked, this, [this] { submit(true); });
    connect(stop, &QPushButton::clicked, this, [this] {
      if (reply) {
        stopped = true;
        reply->abort();
      }
    });
    connect(clear, &QPushButton::clicked, this, [this] {
      current = -1;
      history = {};
      lastPrompt.clear();
      transcript->clear();
      receipt->clear();
      draft->clear();
      retry->setEnabled(false);
      QSignalBlocker block(threads);
      threads->setCurrentIndex(0);
    });
    connect(threads, qOverload<int>(&QComboBox::activated), this,
            [this](int index) {
              if (busy())
                return;
              current = index - 1;
              history =
                  current < 0
                      ? QJsonArray{}
                      : conversations[current].toObject()["messages"].toArray();
              lastPrompt.clear();
              draft->clear();
              receipt->clear();
              retry->setEnabled(false);
              render();
            });
    connect(remember, &QCheckBox::toggled, this, [this](bool enabled) {
      if (!enabled) {
        if (QMessageBox::question(
                this, "Remove saved history?",
                "Delete Wayfinder’s saved conversations from this device? They "
                "remain in memory until you close the app.") !=
            QMessageBox::Yes) {
          QSignalBlocker block(remember);
          remember->setChecked(true);
          return;
        }
        if (QFileInfo::exists(storage) && !QFile::remove(storage)) {
          QSignalBlocker block(remember);
          remember->setChecked(true);
          status->setText("Could not remove saved history.");
        }
      } else
        persist();
    });
    loadHistory();
  }
  void render() {
    QString text;
    for (const auto &v : history) {
      auto m = v.toObject();
      text += (m["role"].toString() == "user" ? "You" : "Wayfinder") +
              QString("\n") + m["content"].toString() + "\n\n";
    }
    transcript->setPlainText(text);
    transcript->moveCursor(QTextCursor::End);
    QSignalBlocker block(turns);
    turns->clear();
    int count = 0;
    for (const auto &v : history) {
      auto m = v.toObject();
      if (m["receipt"].isObject()) {
        auto decision = m["receipt"].toObject();
        turns->addItem(QString("Reply %1 · %2")
                           .arg(++count)
                           .arg(decision["model"].toString()),
                       decision);
      }
    }
    if (turns->count()) {
      turns->setCurrentIndex(turns->count() - 1);
      showReceipt(turns->currentData().toJsonObject());
    } else
      receipt->clear();
  }
  void showReceipt(const QJsonObject &decision) {
    QString text =
        "Route: " + decision["model"].toString() +
        "\nScore: " + QString::number(decision["score"].toDouble(), 'f', 4) +
        "\nMode: " + decision["mode"].toString() +
        "\nRequest: " + decision["request_id"].toString() + "\n\nSignals\n";
    bool hasSignals = false;
    for (const auto &v : decision["contributions"].toArray()) {
      auto signal = v.toObject();
      if (signal["contribution"].toDouble() != 0) {
        hasSignals = true;
        text += signal["name"].toString().replace('_', ' ') + " · " +
                QString::number(signal["contribution"].toDouble(), 'f', 4) +
                "\n";
      }
    }
    if (!hasSignals)
      text += "No non-zero contributions reported.\n";
    receipt->setPlainText(text);
  }
  void record() {
    QJsonObject thread{
        {"title", history.first().toObject()["content"].toString().left(70)},
        {"messages", history}};
    if (current < 0) {
      conversations.append(thread);
      current = conversations.size() - 1;
    } else
      conversations[current] = thread;
    rebuildThreads();
    persist();
  }
  void rebuildThreads() {
    QSignalBlocker block(threads);
    threads->clear();
    threads->addItem("New conversation");
    for (const auto &v : conversations)
      threads->addItem(v.toObject()["title"].toString());
    threads->setCurrentIndex(current + 1);
  }
  void persist() {
    if (!remember->isChecked())
      return;
    if (historyReadFailed) {
      status->setText("Existing history could not be read. It has been "
                      "preserved; new messages remain in memory.");
      return;
    }
    const auto data = QJsonDocument(conversations).toJson();
    if (data.size() > 4 * 1024 * 1024) {
      status->setText("Saved history limit reached (4 MiB). This conversation "
                      "remains in memory.");
      return;
    }
    if (QFileInfo(storage).isSymLink()) {
      status->setText("History path is a symlink; no history was saved.");
      return;
    }
    QDir().mkpath(QFileInfo(storage).absolutePath());
    QSaveFile file(storage);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
      status->setText("Could not save history.");
      return;
    }
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    if (file.write(data) != data.size() || !file.commit())
      status->setText(
          "Could not save history. Existing saved history was preserved.");
  }
  void loadHistory() {
    QFile file(storage);
    if (!file.exists())
      return;
    if (QFileInfo(storage).isSymLink() || !file.open(QIODevice::ReadOnly) ||
        file.size() > 4 * 1024 * 1024) {
      historyReadFailed = true;
      status->setText(
          "Saved history could not be read; it has been preserved.");
      return;
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
      historyReadFailed = true;
      status->setText("Saved history is invalid; it has been preserved.");
      return;
    }
    for (const auto &v : doc.array()) {
      if (!v.isObject() || !v.toObject()["messages"].isArray()) {
        historyReadFailed = true;
        status->setText("Saved history is invalid; it has been preserved.");
        return;
      }
      const auto messages = v.toObject()["messages"].toArray();
      if (messages.isEmpty() || messages.size() > 64) {
        historyReadFailed = true;
        status->setText("Saved history is invalid; it has been preserved.");
        return;
      }
      for (const auto &m : messages) {
        auto role = m.toObject()["role"].toString();
        if ((role != "user" && role != "assistant") ||
            !m.toObject()["content"].isString()) {
          historyReadFailed = true;
          status->setText("Saved history is invalid; it has been preserved.");
          return;
        }
      }
    }
    conversations = doc.array();
    QSignalBlocker block(remember);
    remember->setChecked(true);
    rebuildThreads();
  }
  bool busy() const { return !reply.isNull(); }
  void setCatalog(const QJsonArray &models) {
    const auto selected = destination->currentData().toString();
    destination->clear();
    destination->addItem("Automatic", "auto");
    for (const auto &value : models) {
      auto model = value.toObject();
      destination->addItem(model["name"].toString() + " · " +
                               model["model"].toString(),
                           model["name"].toString());
    }
    int i = destination->findData(selected);
    if (i < 0 && !selected.isEmpty() && selected != "auto") {
      destination->addItem(selected + " · unavailable", selected);
      i = destination->count() - 1;
    }
    destination->setCurrentIndex(qMax(0, i));
  }
  void submit(bool again) {
    if (reply)
      return;
    const auto text = again ? lastPrompt : draft->toPlainText().trimmed();
    if (text.isEmpty() || text.toUtf8().size() > 32768) {
      status->setText("Enter a message up to 32 KiB.");
      return;
    }
    QJsonArray messages;
    for (const auto &v : history) {
      auto m = v.toObject();
      messages.append(
          QJsonObject{{"role", m["role"]}, {"content", m["content"]}});
    }
    messages.append(QJsonObject{{"role", "user"}, {"content", text}});
    if (messages.size() > 64 ||
        QJsonDocument(messages).toJson().size() > 131072) {
      status->setText(
          "This conversation reached its limit. Start a new conversation.");
      return;
    }
    lastPrompt = text;
    stopped = false;
    send->setEnabled(false);
    retry->setEnabled(false);
    clear->setEnabled(false);
    threads->setEnabled(false);
    destination->setEnabled(false);
    stop->setEnabled(true);
    status->setText("Waiting for the gateway…");
    reply = gateway.stream(
        {{"model", destination->currentData().toString()},
         {"messages", messages},
         {"stream", true}},
        [this, text](const QString &partial) {
          render();
          transcript->insertPlainText("You\n" + text +
                                      "\n\nWayfinder · responding\n" + partial);
        },
        [this, messages, text](bool ok, const QJsonObject &obj,
                               const QByteArray &id) {
          reply = nullptr;
          send->setEnabled(true);
          clear->setEnabled(true);
          threads->setEnabled(true);
          destination->setEnabled(true);
          stop->setEnabled(false);
          auto choices = obj["choices"].toArray();
          const auto answer = choices.isEmpty() ? QString()
                                                : choices.first()
                                                      .toObject()["message"]
                                                      .toObject()["content"]
                                                      .toString();
          if (!ok || answer.isEmpty() || !obj["wayfinder"].isObject() ||
              id.isEmpty()) {
            status->setText(
                stopped ? "Stopped. The provider may already have processed "
                          "the request. Retrying sends it again."
                        : "No verified reply received. Check the gateway, "
                          "selected route and provider credentials. Retry "
                          "sends a new request.");
            render();
            retry->setEnabled(true);
            return;
          }
          history.append(QJsonObject{{"role", "user"}, {"content", text}});
          const auto decision = obj["wayfinder"].toObject();
          history.append(QJsonObject{{"role", "assistant"},
                                     {"content", answer},
                                     {"receipt", decision}});
          render();
          if (draft->toPlainText().trimmed() == text)
            draft->clear();
          lastPrompt.clear();
          status->setText("Reply received with a Router receipt.");
          retry->setEnabled(false);
          record();
        });
  }
};
