#pragma once
#include "command.h"
#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QRegularExpression>
#include <QStandardPaths>

// Use Omarchy's semantic resolver, including aliases and light/dark fallbacks.
// Polling also detects atomic directory/symlink replacement during theme
// changes.
class DesktopTheme : public QObject {
  Command command;
  QTimer timer;
  QPalette original;

public:
  explicit DesktopTheme(QObject *parent = nullptr)
      : QObject(parent), original(qApp->palette()) {
    connect(&timer, &QTimer::timeout, this, [this] { refresh(); });
    timer.start(3000);
    refresh();
  }
  void refresh() {
    const auto helper = QStandardPaths::findExecutable("omarchy-theme-color");
    if (helper.isEmpty() || command.busy())
      return;
    command.start(
        helper, {"--all"}, {}, [this](bool ok, const QByteArray &data) {
          if (!ok)
            return;
          QHash<QString, QColor> colors;
          for (const auto &line : QString::fromUtf8(data).split('\n')) {
            auto fields = line.split('\t');
            if (fields.size() == 2 && QRegularExpression("^#[0-9a-fA-F]{6}$")
                                          .match(fields[1])
                                          .hasMatch())
              colors.insert(fields[0], QColor(fields[1]));
          }
          if (!colors.contains("background") || !colors.contains("foreground"))
            return;
          auto p = original;
          auto bg = colors["background"], fg = colors["foreground"];
          const auto accent = colors.value("accent", colors.value("blue", fg));
          for (auto role : {QPalette::Window, QPalette::Base, QPalette::Button})
            p.setColor(role, bg);
          for (auto role : {QPalette::WindowText, QPalette::Text,
                            QPalette::ButtonText, QPalette::ToolTipText})
            p.setColor(role, fg);
          p.setColor(QPalette::Mid,
                     bg.lightnessF() < 0.5 ? bg.lighter(170) : bg.darker(125));
          p.setColor(QPalette::ToolTipBase, bg);
          p.setColor(QPalette::Highlight, accent);
          p.setColor(QPalette::HighlightedText, accent.lightnessF() > 0.5
                                                    ? QColor("#111111")
                                                    : QColor("#ffffff"));
          p.setColor(QPalette::AlternateBase,
                     colors.value("surface", bg.lightnessF() < 0.5
                                                 ? bg.lighter(125)
                                                 : bg.darker(105)));
          p.setColor(QPalette::Disabled, QPalette::Text,
                     colors.value("muted", fg));
          qApp->setPalette(p);
        });
  }
};
