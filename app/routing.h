#pragma once
#include "command.h"
#include <QCryptographicHash>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTemporaryFile>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

class RoutingPanel : public QWidget {
public:
  Command command;
  QString router, config, mode;
  QByteArray loadedHash;
  QJsonObject snapshot;
  QTableWidget *tiers, *weights;
  QLabel *message, *result;
  QPlainTextEdit *prompt;
  QPushButton *save, *reload, *preview;
  bool dirty = false, loading = false;
  std::unique_ptr<QTemporaryFile> draftFile;
  QPushButton *addTier, *removeTier, *resetWeights;
  std::function<bool()> canRun = [] { return true; };
  explicit RoutingPanel(const QString &binary, const QString &path,
                        QWidget *parent = nullptr)
      : QWidget(parent), router(binary), config(path) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    auto *title = new QLabel("Routing");
    auto font = title->font();
    font.setPointSize(font.pointSize() + 7);
    font.setBold(true);
    title->setFont(font);
    layout->addWidget(title);
    auto *intro = new QLabel("Choose where each complexity score goes. The "
                             "Rust Router validates and applies your policy.");
    intro->setWordWrap(true);
    layout->addWidget(intro);
    message = new QLabel("Load your saved policy to begin.");
    message->setWordWrap(true);
    layout->addWidget(message);
    tiers = new QTableWidget(0, 2);
    tiers->setHorizontalHeaderLabels({"Route ID", "Minimum score"});
    tiers->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tiers->setMaximumHeight(190);
    layout->addWidget(tiers);
    auto *tierActions = new QHBoxLayout;
    addTier = new QPushButton("Add tier");
    removeTier = new QPushButton("Remove selected tier");
    tierActions->addWidget(addTier);
    tierActions->addWidget(removeTier);
    tierActions->addStretch();
    layout->addLayout(tierActions);
    addTier->setEnabled(false);
    removeTier->setEnabled(false);
    connect(addTier, &QPushButton::clicked, this, [this] {
      if (mode.isEmpty() || mode == "classifier" || command.busy() ||
          tiers->rowCount() >= 32)
        return;
      const int row = tiers->rowCount();
      tiers->insertRow(row);
      tiers->setItem(row, 0, new QTableWidgetItem("new-route"));
      auto *score = new QDoubleSpinBox;
      score->setRange(0, 1);
      score->setDecimals(5);
      score->setSingleStep(0.01);
      score->setValue(1);
      tiers->setCellWidget(row, 1, score);
      connect(score, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
              [this] { dirty = true; });
      mode = "tiered";
      dirty = true;
      message->setText("Draft tier added. Use a configured route ID and "
                       "increasing minimum scores.");
    });
    connect(removeTier, &QPushButton::clicked, this, [this] {
      int row = tiers->currentRow();
      if (row <= 0 || tiers->rowCount() <= 1)
        return;
      tiers->removeRow(row);
      mode = "tiered";
      dirty = true;
    });
    auto *advanced = new QGroupBox("Advanced · feature weights");
    advanced->setCheckable(true);
    advanced->setChecked(false);
    auto *advancedLayout = new QVBoxLayout(advanced);
    weights = new QTableWidget(0, 2);
    weights->setHorizontalHeaderLabels({"Signal", "Weight"});
    weights->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    weights->setMaximumHeight(220);
    advancedLayout->addWidget(weights);
    resetWeights = new QPushButton("Reset weights to shipped defaults");
    advancedLayout->addWidget(resetWeights);
    resetWeights->hide();
    connect(advanced, &QGroupBox::toggled, resetWeights, &QWidget::setVisible);
    connect(resetWeights, &QPushButton::clicked, this, [this] {
      auto defaults = snapshot["weights"].toArray();
      for (int row = 0; row < weights->rowCount() && row < defaults.size();
           ++row)
        qobject_cast<QDoubleSpinBox *>(weights->cellWidget(row, 1))
            ->setValue(defaults[row].toObject()["default"].toDouble());
      dirty = true;
    });
    weights->hide();
    connect(advanced, &QGroupBox::toggled, weights, &QWidget::setVisible);
    layout->addWidget(advanced);
    auto *row = new QHBoxLayout;
    reload = new QPushButton("Load / discard edits");
    save = new QPushButton("Apply routing");
    save->setEnabled(false);
    row->addWidget(reload);
    row->addStretch();
    row->addWidget(save);
    layout->addLayout(row);
    prompt = new QPlainTextEdit;
    prompt->setPlaceholderText("Paste a prompt to inspect its route using your "
                               "current edits. This does "
                               "not contact a provider.");
    prompt->setMaximumHeight(100);
    layout->addWidget(prompt);
    preview = new QPushButton("Preview route with current edits");
    layout->addWidget(preview);
    result = new QLabel;
    result->setWordWrap(true);
    result->setTextFormat(Qt::PlainText);
    result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(result);
    layout->addStretch();
    connect(tiers, &QTableWidget::itemChanged, this, [this] {
      if (!loading)
        dirty = true;
    });
    connect(reload, &QPushButton::clicked, this, [this] {
      if (!dirty ||
          QMessageBox::question(
              this, "Discard routing edits?",
              "Reload the saved policy and discard your unsaved edits?") ==
              QMessageBox::Yes)
        load();
    });
    connect(save, &QPushButton::clicked, this, [this] { apply(); });
    connect(preview, &QPushButton::clicked, this, [this] { explain(); });
  }
  QByteArray hash() const {
    QFile f(config);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 1024 * 1024)
      return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
  }
  void busy(bool b) {
    reload->setEnabled(!b);
    for (auto *button : {addTier, removeTier, resetWeights})
      button->setEnabled(!b && !mode.isEmpty() && mode != "classifier");
    save->setEnabled(!b && !mode.isEmpty() && mode != "classifier");
    preview->setEnabled(!b);
    tiers->setEnabled(!b && mode != "classifier");
    weights->setEnabled(!b && mode != "classifier");
  }
  void load() {
    if (command.busy() || !canRun())
      return;
    auto before = hash();
    busy(true);
    message->setText("Loading policy…");
    command.start(
        router, {"config", "read-routing", "--path", config}, {},
        [this, before](bool ok, const QByteArray &data) {
          const auto doc = QJsonDocument::fromJson(data);
          const auto obj = doc.object();
          if (!ok || before.isEmpty() || before != hash() ||
              !obj.contains("mode")) {
            mode.clear();
            busy(false);
            message->setText("Could not load a stable policy. Prepare setup or "
                             "check your configuration, then reload.");
            return;
          }
          snapshot = obj;
          loadedHash = before;
          mode = obj["mode"].toString();
          loading = true;
          const auto rows = obj["tiers"].toArray();
          tiers->setRowCount(rows.size());
          for (int i = 0; i < rows.size(); ++i) {
            auto tier = rows[i].toObject();
            auto *name = new QTableWidgetItem(tier["model"].toString());
            tiers->setItem(i, 0, name);
            auto *score = new QDoubleSpinBox;
            score->setRange(0, 1);
            score->setDecimals(5);
            score->setSingleStep(0.01);
            score->setValue(tier["min_score"].toDouble());
            if (i == 0)
              score->setEnabled(false);
            tiers->setCellWidget(i, 1, score);
            connect(score, qOverload<double>(&QDoubleSpinBox::valueChanged),
                    this, [this] { dirty = true; });
          }
          const auto features = obj["weights"].toArray();
          weights->setRowCount(features.size());
          for (int i = 0; i < features.size(); ++i) {
            auto feature = features[i].toObject();
            auto *label = new QTableWidgetItem(feature["label"].toString());
            label->setFlags(label->flags() & ~Qt::ItemIsEditable);
            label->setData(Qt::UserRole, feature["id"].toString());
            weights->setItem(i, 0, label);
            auto *value = new QDoubleSpinBox;
            value->setRange(0, 1000000);
            value->setDecimals(6);
            value->setValue(feature["value"].toDouble());
            weights->setCellWidget(i, 1, value);
            connect(value, qOverload<double>(&QDoubleSpinBox::valueChanged),
                    this, [this] { dirty = true; });
          }
          loading = false;
          dirty = false;
          busy(false);
          message->setText(mode == "classifier"
                               ? "Classifier policy · read-only. Use your "
                                 "configuration editor to change its model."
                               : "Saved policy loaded · " + mode +
                                     " routing. Changes apply only when you "
                                     "choose Apply routing.");
        });
  }
  QByteArray fragment() const {
    QByteArray out;
    if (mode == "binary") {
      out =
          "[routing]\nthreshold = " +
          QByteArray::number(
              qobject_cast<QDoubleSpinBox *>(tiers->cellWidget(1, 1))->value(),
              'g', 12) +
          "\n";
      // Binary controls cannot rename canonical local/cloud arms.
      if (tiers->item(0, 0)->text() != "local" ||
          tiers->item(1, 0)->text() != "cloud")
        return {};
    } else {
      for (int i = 0; i < tiers->rowCount(); ++i) {
        const auto name = tiers->item(i, 0)->text().trimmed();
        if (!QRegularExpression("^[A-Za-z0-9_.-]{1,64}$")
                 .match(name)
                 .hasMatch())
          return {};
        out += "[[routing.tiers]]\nmodel = \"" + name.toUtf8() +
               "\"\nmin_score = " +
               QByteArray::number(
                   qobject_cast<QDoubleSpinBox *>(tiers->cellWidget(i, 1))
                       ->value(),
                   'g', 12) +
               "\n\n";
      }
    }
    out += "\n[routing.weights]\n";
    for (int i = 0; i < weights->rowCount(); ++i) {
      auto id = weights->item(i, 0)->data(Qt::UserRole).toString();
      if (!QRegularExpression("^[a-z_]+$").match(id).hasMatch())
        return {};
      out += id.toUtf8() + " = " +
             QByteArray::number(
                 qobject_cast<QDoubleSpinBox *>(weights->cellWidget(i, 1))
                     ->value(),
                 'g', 12) +
             "\n";
    }
    return out;
  }
  void apply() {
    if (command.busy() || !canRun() || mode.isEmpty() || mode == "classifier")
      return;
    if (hash() != loadedHash) {
      message->setText(
          "Configuration changed outside this editor. Reload before applying.");
      return;
    }
    auto input = fragment();
    if (input.isEmpty()) {
      message->setText(
          "Use valid route IDs. Binary policies must retain local and cloud.");
      return;
    }
    if (QMessageBox::question(
            this, "Apply routing?",
            "Save this routing policy? Provider configuration is preserved. "
            "Restart the gateway afterwards to load it.") != QMessageBox::Yes)
      return;
    if (hash() != loadedHash) {
      message->setText("Configuration changed. Reload before applying.");
      return;
    }
    const auto backup = config + ".before-routing-" +
                        QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QFile::copy(config, backup)) {
      message->setText("Could not back up the policy. Nothing changed.");
      return;
    }
    busy(true);
    command.start(
        router, {"config", "apply-routing", "--path", config}, input,
        [this](bool ok, const QByteArray &) {
          busy(false);
          if (ok) {
            dirty = false;
            loadedHash = hash();
            message->setText(
                "Policy saved. Restart the gateway in Gateway to apply it. A "
                "backup was kept beside the configuration.");
          } else
            message->setText(
                "The Router rejected the policy. Check tier order and route "
                "IDs, then reload. Your backup is retained.");
        });
  }
  void explain() {
    auto text = prompt->toPlainText().toUtf8();
    if (text.trimmed().isEmpty() || text.size() > 65536) {
      result->setText("Enter a prompt up to 64 KiB.");
      return;
    }
    if (command.busy() || !canRun())
      return;
    if (mode.isEmpty()) {
      result->setText("Load a policy before previewing.");
      return;
    }
    if (hash() != loadedHash) {
      result->setText("Configuration changed. Reload before previewing.");
      return;
    }
    busy(true);
    if (mode == "classifier") {
      score(text, config, "Saved classifier policy");
      return;
    }
    auto input = fragment();
    if (input.isEmpty()) {
      busy(false);
      result->setText("Correct the route IDs before previewing.");
      return;
    }
    QFile original(config);
    if (!original.open(QIODevice::ReadOnly) || original.size() > 1024 * 1024) {
      busy(false);
      result->setText("Could not read the saved policy.");
      return;
    }
    draftFile = std::make_unique<QTemporaryFile>();
    if (!draftFile->open()) {
      busy(false);
      result->setText("Could not prepare a private preview.");
      return;
    }
    auto source = original.readAll();
    if (draftFile->write(source) != source.size()) {
      draftFile.reset();
      busy(false);
      return;
    }
    const auto path = draftFile->fileName();
    draftFile->close();
    command.start(router, {"config", "apply-routing", "--path", path}, input,
                  [this, text, path](bool ok, const QByteArray &) {
                    if (!ok) {
                      draftFile.reset();
                      busy(false);
                      result->setText(
                          "The Router rejected these draft settings. Check "
                          "tier order and model IDs.");
                      return;
                    }
                    score(text, path, "Draft policy");
                  });
  }
  void score(const QByteArray &text, const QString &path,
             const QString &label) {
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYFINDER_CONFIG", path);
    env.remove("WAYFINDER_ROUTER_THRESHOLD");
    command.start(
        router, {"route", "-", "--json"}, text,
        [this, label](bool ok, const QByteArray &data) {
          draftFile.reset();
          busy(false);
          auto obj = QJsonDocument::fromJson(data).object();
          if (!ok || !obj.contains("recommendation")) {
            result->setText(
                "Could not score this prompt. Load a valid policy first.");
            return;
          }
          result->setText(label + " → " + obj["recommendation"].toString() +
                          "\nComplexity score: " +
                          QString::number(obj["score"].toDouble(), 'f', 4) +
                          "\nMode: " + obj["mode"].toString() +
                          "\nNo provider request was sent and the saved policy "
                          "was not changed.");
        },
        env);
  }
};
