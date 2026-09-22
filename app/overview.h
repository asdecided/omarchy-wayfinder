#pragma once
#include "gateway.h"
#include <QDateTime>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

class OverviewPanel : public QWidget {
  Gateway gateway;
  QTimer timer;
  int pending = 0;

public:
  QLabel *health, *summary, *savings, *emptyModels, *emptyRecent;
  QTableWidget *models, *recent;
  QJsonArray catalog;
  std::function<void(const QJsonArray &)> catalogChanged;
  explicit OverviewPanel(QWidget *parent = nullptr) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    auto *title = new QLabel("Your routing, at a glance");
    auto font = title->font();
    font.setPointSize(font.pointSize() + 7);
    font.setBold(true);
    title->setFont(font);
    layout->addWidget(title);
    health = new QLabel("Checking local gateway…");
    health->setWordWrap(true);
    summary = new QLabel("Recent routing: unavailable");
    savings = new QLabel("Savings today: unavailable");
    summary->setWordWrap(true);
    savings->setWordWrap(true);
    auto *metrics = new QHBoxLayout;
    const QList<QPair<QString, QLabel *>> cards = {
        {"Gateway", health}, {"Activity", summary}, {"Savings", savings}};
    for (const auto &card : cards) {
      auto *box = new QGroupBox(card.first);
      auto *content = new QVBoxLayout(box);
      content->addWidget(card.second);
      box->setMinimumHeight(100);
      metrics->addWidget(box, 1);
    }
    layout->addLayout(metrics);
    layout->addWidget(new QLabel("Configured connections"));
    models = new QTableWidget(0, 4);
    models->setHorizontalHeaderLabels(
        {"Route", "Model", "Provider", "Credential"});
    prepare(models);
    layout->addWidget(models);
    emptyModels = new QLabel(
        "No connections to display. Start the gateway or connect a provider.");
    emptyModels->setWordWrap(true);
    layout->addWidget(emptyModels);
    layout->addWidget(
        new QLabel("Recent decisions · gateway’s bounded history"));
    recent = new QTableWidget(0, 4);
    recent->setHorizontalHeaderLabels({"Time", "Route", "Score", "Mode"});
    prepare(recent);
    layout->addWidget(recent);
    emptyRecent = new QLabel("No routing decisions to display yet. Send a "
                             "message in Chat or connect an agent.");
    emptyRecent->setWordWrap(true);
    layout->addWidget(emptyRecent);
    layout->addStretch();
    auto *refresh = new QPushButton("Refresh overview");
    layout->addWidget(refresh);
    connect(refresh, &QPushButton::clicked, this, [this] { update(); });
    connect(&timer, &QTimer::timeout, this, [this] {
      if (isVisible())
        update();
    });
    timer.start(15000);
  }
  static void prepare(QTableWidget *table) {
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setMinimumHeight(130);
    table->setMaximumHeight(220);
    table->verticalHeader()->hide();
  }
  void update() {
    if (pending)
      return;
    pending = 4;
    gateway.request(
        "healthz", {},
        [this](bool ok, const QJsonObject &obj, const QByteArray &) {
          --pending;
          health->setText(
              !ok ? "Gateway unavailable · start it from Gateway, or complete "
                    "Connections setup."
              : obj["offline"].toBool()
                  ? "Gateway reachable · offline mode"
                  : "Gateway reachable · hosted requests permitted");
        });
    gateway.request(
        "router/models", {},
        [this](bool ok, const QJsonObject &obj, const QByteArray &) {
          --pending;
          catalog = ok ? obj["models"].toArray() : QJsonArray{};
          models->setRowCount(catalog.size());
          models->setVisible(!catalog.isEmpty());
          emptyModels->setVisible(catalog.isEmpty());
          for (int i = 0; i < catalog.size(); ++i) {
            auto row = catalog[i].toObject();
            QStringList values = {
                row["name"].toString(), row["model"].toString(),
                row["provider"].toString(),
                row["key_ok"].toBool() ? "Available" : "Missing / unavailable"};
            for (int j = 0; j < values.size(); ++j)
              models->setItem(i, j, new QTableWidgetItem(values[j]));
          }
          if (catalogChanged)
            catalogChanged(catalog);
        });
    gateway.request(
        "router/recent", {},
        [this](bool ok, const QJsonObject &obj, const QByteArray &) {
          --pending;
          summary->setText(
              ok ? QString("Recent decisions: %1").arg(obj["total"].toInt())
                 : "Recent routing: unavailable");
          const auto rows = ok ? obj["recent"].toArray() : QJsonArray{};
          recent->setRowCount(qMin(100, rows.size()));
          recent->setVisible(!rows.isEmpty());
          emptyRecent->setVisible(rows.isEmpty());
          for (int i = 0; i < recent->rowCount(); ++i) {
            auto row = rows[i].toObject();
            QStringList values = {
                QDateTime::fromSecsSinceEpoch(qint64(row["ts"].toDouble()))
                    .toLocalTime()
                    .toString("HH:mm:ss"),
                row["model"].toString(),
                QString::number(row["score"].toDouble(), 'f', 4),
                row["mode"].toString()};
            for (int j = 0; j < values.size(); ++j)
              recent->setItem(i, j, new QTableWidgetItem(values[j]));
          }
        });
    gateway.request(
        "v1/savings?period=today", {},
        [this](bool ok, const QJsonObject &obj, const QByteArray &) {
          --pending;
          savings->setText(
              ok && obj["priced"].toBool()
                  ? QString("Savings today: $%1 · %2 priced requests · Router "
                            "baseline estimate")
                        .arg(obj["saved"].toDouble(), 0, 'f', 2)
                        .arg(obj["requests"].toInt())
                  : "Savings today: unavailable — no priced evidence");
        });
  }
};
