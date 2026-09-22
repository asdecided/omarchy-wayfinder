#define WAYFINDER_TEST
#include "main.cpp"
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

class DesktopTest : public QObject {
  Q_OBJECT
  QTemporaryDir directory;
  QString fake(const QByteArray &body) {
    const auto path = directory.filePath("fake-router");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return {};
    f.write("#!/bin/sh\n");
    f.write(body);
    f.close();
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
  }
private slots:
  void routingRejectsStaleEdits() {
    const auto path = directory.filePath("routing.toml");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[routing]\nthreshold=0.5\n");
    f.close();
    RoutingPanel panel(
        fake("printf '%s' "
             "'{\"mode\":\"binary\",\"tiers\":[{\"model\":\"local\",\"min_"
             "score\":0},{\"model\":\"cloud\",\"min_score\":0.5}],\"weights\":["
             "{\"id\":\"word_count\",\"label\":\"Words\",\"value\":3}]}'\n"),
        path);
    panel.load();
    QTRY_COMPARE(panel.mode, QString("binary"));
    QVERIFY(panel.fragment().contains("threshold = 0.5"));
    QVERIFY(panel.fragment().contains("word_count = 3"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[routing]\nthreshold=0.9\n");
    f.close();
    panel.apply();
    QVERIFY(panel.message->text().contains("changed outside"));
    QCOMPARE(panel.command.process.state(), QProcess::NotRunning);
  }
  void nativeRoutingRoundTripPreservesGateway() {
    const auto router = qEnvironmentVariable("WAYFINDER_TEST_ROUTER");
    if (router.isEmpty())
      QSKIP("Released Router supplied by Arch CI");
    const auto path = directory.filePath("native-routing.toml");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("# preserve me\n[routing]\nthreshold = 0.5\n[gateway]\noffline = "
            "true\n[gateway.models.local]\nbase_url = "
            "\"http://127.0.0.1:11434/v1\"\nmodel = \"llama\"\n");
    f.close();
    RoutingPanel panel(router, path);
    panel.load();
    QTRY_COMPARE(panel.mode, QString("binary"));
    qobject_cast<QDoubleSpinBox *>(panel.tiers->cellWidget(1, 1))
        ->setValue(0.73);
    panel.prompt->setPlainText("Hello there");
    panel.explain();
    QTRY_VERIFY(panel.result->text().startsWith("Draft policy"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.readAll().contains("threshold = 0.5"));
    f.close();
    QProcess child;
    child.start(router, {"config", "apply-routing", "--path", path});
    QVERIFY(child.waitForStarted());
    child.write(panel.fragment());
    child.closeWriteChannel();
    QVERIFY(child.waitForFinished());
    QCOMPARE(child.exitCode(), 0);
    QVERIFY(f.open(QIODevice::ReadOnly));
    auto result = f.readAll();
    QVERIFY(result.contains("# preserve me"));
    QVERIFY(result.contains("offline = true"));
    QVERIFY(result.contains("threshold = 0.73"));
  }
  void themeKeepsLastGoodPalette() {
    const auto original = qApp->palette();
    const auto previous = qgetenv("PATH");
    const auto helper = directory.filePath("omarchy-theme-color");
    QFile f(helper);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("#!/bin/sh\nprintf "
            "'background\\t#112233\\nforeground\\t#ddeeff\\naccent\\t#"
            "abcdef\\n'\n");
    f.close();
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    qputenv("PATH", directory.path().toUtf8() + ":" + previous);
    {
      DesktopTheme theme;
      QTRY_COMPARE(qApp->palette().color(QPalette::Window), QColor("#112233"));
      QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
      f.write("#!/bin/sh\nprintf 'background\\tbroken\\n'\n");
      f.close();
      theme.refresh();
      QTest::qWait(100);
      QCOMPARE(qApp->palette().color(QPalette::Window), QColor("#112233"));
    }
    qputenv("PATH", previous);
    qApp->setPalette(original);
  }
  void classifierIsReadOnly() {
    const auto path = directory.filePath("classifier.toml");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("fixture");
    f.close();
    RoutingPanel panel(fake("printf '%s' "
                            "'{\"mode\":\"classifier\",\"models\":[\"local\"],"
                            "\"weights\":[]}'\n"),
                       path);
    panel.load();
    QTRY_COMPARE(panel.mode, QString("classifier"));
    QVERIFY(!panel.save->isEnabled());
  }
  void pinnedUnavailableChatRouteIsRetained() {
    ChatPanel panel;
    panel.setCatalog(
        QJsonArray{QJsonObject{{"name", "local"}, {"model", "llama"}}});
    panel.destination->setCurrentIndex(1);
    panel.setCatalog({});
    QCOMPARE(panel.destination->currentData().toString(), QString("local"));
    QVERIFY(panel.destination->currentText().contains("unavailable"));
  }
  void streamDecoderRequiresDecisionAndCompletion() {
    GatewayStream stream;
    stream.consume(
        "data: "
        "{\"wayfinder\":{\"model\":\"local\",\"request_id\":\"fixture\"}}\n\n");
    stream.consume("data: {\"choices\":[{\"delta\":{\"content\":\"hel");
    QVERIFY(stream.text.isEmpty());
    stream.consume("lo\"}}]}\n\ndata: [DONE]\n\n");
    QVERIFY(stream.complete);
    QVERIFY(!stream.failed);
    QCOMPARE(stream.text, QString("hello"));
    stream.consume("data: [DONE]\n\n");
    QVERIFY(stream.failed);
    GatewayStream invalid;
    invalid.consume(
        "data: {\"choices\":[{\"delta\":{\"content\":\"unverified\"}}]}\n\n");
    QVERIFY(invalid.failed);
  }
  void chatAcceptsCompletedStreamAndDisplaysPlainText() {
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 8088))
      QSKIP("Loopback fixture port occupied");
    connect(&server, &QTcpServer::newConnection, this, [&] {
      auto *socket = server.nextPendingConnection();
      connect(socket, &QTcpSocket::readyRead, socket, [socket] {
        socket->readAll();
        QByteArray body =
            "data: "
            "{\"wayfinder\":{\"model\":\"local\",\"score\":0.1,\"mode\":"
            "\"scored\",\"request_id\":\"fixture\"}}\n\ndata: "
            "{\"choices\":[{\"delta\":{\"content\":\"<b>Literal "
            "reply</b>\"}}]}\n\ndata: [DONE]\n\n";
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: "
                      "text/event-stream\r\nx-wayfinder-router-request-id: "
                      "fixture\r\nContent-Length: " +
                      QByteArray::number(body.size()) +
                      "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
      });
      connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    });
    ChatPanel panel;
    panel.draft->setPlainText("hello");
    panel.submit(false);
    QTRY_VERIFY(!panel.busy());
    QVERIFY(panel.transcript->toPlainText().contains("<b>Literal reply</b>"));
    QVERIFY(panel.receipt->toPlainText().contains("Request: fixture"));
    QVERIFY(!panel.retry->isEnabled());
  }
  void gatewayDoesNotFollowRedirects() {
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 8088))
      QSKIP("Loopback fixture port occupied");
    connect(&server, &QTcpServer::newConnection, this, [&] {
      auto *socket = server.nextPendingConnection();
      connect(socket, &QTcpSocket::readyRead, socket, [socket] {
        socket->readAll();
        socket->write("HTTP/1.1 302 Found\r\nLocation: "
                      "http://127.0.0.1:1/elsewhere\r\nContent-Length: "
                      "2\r\nConnection: close\r\n\r\n{}");
        socket->disconnectFromHost();
      });
      connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    });
    Gateway gateway;
    bool finished = false, success = true;
    gateway.request("healthz", {},
                    [&](bool ok, const QJsonObject &, const QByteArray &) {
                      success = ok;
                      finished = true;
                    });
    QTRY_VERIFY(finished);
    QVERIFY(!success);
  }
  void chatRejectsUnverifiedReply() {
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 8088))
      QSKIP("Loopback fixture port occupied");
    connect(&server, &QTcpServer::newConnection, this, [&] {
      auto *socket = server.nextPendingConnection();
      connect(socket, &QTcpSocket::readyRead, socket, [socket] {
        socket->readAll();
        QByteArray body =
            "{\"choices\":[{\"message\":{\"content\":\"unverified\"}}]}";
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: "
                      "application/json\r\nContent-Length: " +
                      QByteArray::number(body.size()) +
                      "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
      });
      connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    });
    ChatPanel panel;
    panel.draft->setPlainText("Hello");
    panel.submit(false);
    QTRY_VERIFY(!panel.busy());
    QVERIFY(panel.transcript->toPlainText().isEmpty());
    QVERIFY(panel.retry->isEnabled());
    QVERIFY(panel.status->text().contains("No verified reply"));
  }
  void captureReviewScreens() {
    const auto folder = qEnvironmentVariable("WAYFINDER_REVIEW_CAPTURES");
    if (folder.isEmpty())
      QSKIP("Optional real-widget offscreen captures");
    QDir().mkpath(folder);
    Window w("/nonexistent/wayfinder-router");
    w.show();
    for (int index : QList<int>{6, 0, 7, 8}) {
      w.tabs->setCurrentIndex(index);
      QTest::qWait(100);
      QVERIFY(w.grab().save(folder + "/" +
                            w.tabs->tabText(index).toLower().replace(' ', '-') +
                            ".png"));
    }
  }
  void releasedRouterStarterContract() {
    const auto router = qEnvironmentVariable("WAYFINDER_TEST_ROUTER");
    if (router.isEmpty())
      QSKIP("Set WAYFINDER_TEST_ROUTER to the released binary");
    Window w(router);
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("XDG_CONFIG_HOME", directory.path());
    w.process.setProcessEnvironment(env);
    QDir().mkpath(directory.filePath("wayfinder"));
    w.config = directory.filePath("wayfinder/wayfinder-router.toml");
    w.initializing = true;
    w.run("Create local starter",
          {"init", "--preset", "local", "--path", w.config});
    QTRY_VERIFY_WITH_TIMEOUT(w.stage->text().contains("provider"), 10000);
    QVERIFY(QFileInfo::exists(w.config));
    QFile policy(w.config);
    QVERIFY(policy.open(QIODevice::ReadOnly));
    const auto original = policy.readAll();
    policy.close();
    w.setup("status");
    QTRY_COMPARE(w.process.state(), QProcess::NotRunning);
    QVERIFY(policy.open(QIODevice::ReadOnly));
    QCOMPARE(policy.readAll(), original);
  }
  void discoversWithoutExposingKeyInArguments() {
    const auto captured = directory.filePath("arguments");
    const auto input = directory.filePath("input");
    Window w(
        fake(("printf '%s\\n' \"$@\" > '" + captured + "'\ncat > '" + input +
              "'\nprintf '%s' "
              "'{\"ok\":true,\"stage\":\"model\",\"models\":[\"gpt-test\"],"
              "\"keyringInstalled\":true}'\n")
                 .toUtf8()));
    w.config = directory.filePath("config with spaces.toml");
    w.setup("discover", "test-secret\n");
    QTRY_COMPARE(w.process.state(), QProcess::NotRunning);
    QTRY_COMPARE(w.models->currentText(), QString("gpt-test"));
    QFile args(captured);
    QVERIFY(args.open(QIODevice::ReadOnly));
    const auto data = args.readAll();
    QVERIFY(!data.contains("test-secret"));
    QVERIFY(data.contains(w.config.toUtf8() + "\n"));
    QFile stdinFile(input);
    QVERIFY(stdinFile.open(QIODevice::ReadOnly));
    QCOMPARE(stdinFile.readAll(), QByteArray("test-secret\n"));
    QVERIFY(w.secret.isEmpty());
    QVERIFY(w.details->toPlainText().isEmpty());
  }
  void rejectsMissingModelAndRouter() {
    Window w("/nonexistent/wayfinder-router");
    w.setup("activate");
    QVERIFY(w.status->text().contains("Choose"));
    w.initializing = true;
    w.setup("status");
    QVERIFY(!w.initializing);
    QVERIFY(w.status->text().contains("missing"));
    QCOMPARE(w.process.state(), QProcess::NotRunning);
  }
  void cancellationReleasesControls() {
    Window w(fake("trap '' TERM\nwhile :; do :; done\n"));
    w.run("slow", {});
    QTRY_COMPARE(w.process.state(), QProcess::Running);
    w.cancel();
    w.forceStop.start(50);
    QTRY_COMPARE(w.process.state(), QProcess::NotRunning);
    QTRY_VERIFY(w.actions.first()->isEnabled());
    QVERIFY(!w.deadline.isActive());
    QVERIFY(!w.forceStop.isActive());
    QVERIFY(w.secret.isEmpty());
  }
  void failureDoesNotReportSuccess() {
    Window w(fake("printf '%s' '{\"ok\":false,\"error\":\"Repair "
                  "required\"}'\nexit 1\n"));
    w.setup("status");
    QTRY_COMPARE(w.status->text(), QString("Repair required"));
    QVERIFY(w.actions.first()->isEnabled());
  }
  void serializesCommands() {
    Window w(fake("sleep 0.2\nprintf '%s' '{\"ok\":true}'\n"));
    w.run("first", {});
    w.run("second", {}, "discard-me\n");
    QCOMPARE(w.action, QString("first"));
    QTRY_COMPARE(w.process.state(), QProcess::NotRunning);
  }
  void companionDoesNotRequireRouter() {
    Window w("/nonexistent/wayfinder-router");
    const auto helper = fake("printf 'Bar companion enabled.\\n'\n");
    w.run("Enable bar companion", {"enable"}, {}, helper);
    QTRY_COMPARE(w.status->text(), QString("Enable bar companion completed."));
    QCOMPARE(w.details->toPlainText(), QString("Bar companion enabled.\n"));
  }
};
QTEST_MAIN(DesktopTest)
#include "desktop-test.moc"
