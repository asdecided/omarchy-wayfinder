#define WAYFINDER_TEST
#include "main.cpp"
#include <QTemporaryDir>
#include <QtTest>

class DesktopTest : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QString fake(const QByteArray &body) {
        const auto path=directory.filePath("fake-router");
        QFile f(path);
        if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate)) return {};
        f.write("#!/bin/sh\n"); f.write(body); f.close();
        f.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        return path;
    }
private slots:
    void releasedRouterStarterContract() {
        const auto router=qEnvironmentVariable("WAYFINDER_TEST_ROUTER");
        if(router.isEmpty()) QSKIP("Set WAYFINDER_TEST_ROUTER to the released binary");
        Window w(router);
        auto env=QProcessEnvironment::systemEnvironment();
        env.insert("XDG_CONFIG_HOME",directory.path());
        w.process.setProcessEnvironment(env);
        QDir().mkpath(directory.filePath("wayfinder"));
        w.config=directory.filePath("wayfinder/wayfinder-router.toml");
        w.initializing=true;
        w.run("Create local starter",{"init","--preset","local","--path",w.config});
        QTRY_VERIFY_WITH_TIMEOUT(w.stage->text().contains("provider"),10000);
        QVERIFY(QFileInfo::exists(w.config));
        QFile policy(w.config); QVERIFY(policy.open(QIODevice::ReadOnly));
        const auto original=policy.readAll(); policy.close();
        w.setup("status");
        QTRY_COMPARE(w.process.state(),QProcess::NotRunning);
        QVERIFY(policy.open(QIODevice::ReadOnly));
        QCOMPARE(policy.readAll(),original);
    }
    void discoversWithoutExposingKeyInArguments() {
        const auto captured=directory.filePath("arguments");
        const auto input=directory.filePath("input");
        Window w(fake(("printf '%s\\n' \"$@\" > '"+captured+"'\ncat > '"+input+"'\nprintf '%s' '{\"ok\":true,\"stage\":\"model\",\"models\":[\"gpt-test\"],\"keyringInstalled\":true}'\n").toUtf8()));
        w.config=directory.filePath("config with spaces.toml");
        w.setup("discover","test-secret\n");
        QTRY_COMPARE(w.process.state(),QProcess::NotRunning);
        QTRY_COMPARE(w.models->currentText(),QString("gpt-test"));
        QFile args(captured); QVERIFY(args.open(QIODevice::ReadOnly));
        const auto data=args.readAll();
        QVERIFY(!data.contains("test-secret"));
        QVERIFY(data.contains(w.config.toUtf8()+"\n"));
        QFile stdinFile(input); QVERIFY(stdinFile.open(QIODevice::ReadOnly));
        QCOMPARE(stdinFile.readAll(),QByteArray("test-secret\n"));
        QVERIFY(w.secret.isEmpty()); QVERIFY(w.details->toPlainText().isEmpty());
    }
    void rejectsMissingModelAndRouter() {
        Window w("/nonexistent/wayfinder-router");
        w.setup("activate");
        QVERIFY(w.status->text().contains("Choose"));
        w.initializing=true; w.setup("status");
        QVERIFY(!w.initializing);
        QVERIFY(w.status->text().contains("missing"));
        QCOMPARE(w.process.state(),QProcess::NotRunning);
    }
    void cancellationReleasesControls() {
        Window w(fake("trap '' TERM\nwhile :; do :; done\n"));
        w.run("slow",{});
        QTRY_COMPARE(w.process.state(),QProcess::Running);
        w.cancel(); w.forceStop.start(50);
        QTRY_COMPARE(w.process.state(),QProcess::NotRunning);
        QTRY_VERIFY(w.actions.first()->isEnabled());
        QVERIFY(!w.deadline.isActive()); QVERIFY(!w.forceStop.isActive());
        QVERIFY(w.secret.isEmpty());
    }
    void failureDoesNotReportSuccess() {
        Window w(fake("printf '%s' '{\"ok\":false,\"error\":\"Repair required\"}'\nexit 1\n"));
        w.setup("status");
        QTRY_COMPARE(w.status->text(),QString("Repair required"));
        QVERIFY(w.actions.first()->isEnabled());
    }
    void serializesCommands() {
        Window w(fake("sleep 0.2\nprintf '%s' '{\"ok\":true}'\n"));
        w.run("first",{});
        w.run("second",{},"discard-me\n");
        QCOMPARE(w.action,QString("first"));
        QTRY_COMPARE(w.process.state(),QProcess::NotRunning);
    }
};
QTEST_MAIN(DesktopTest)
#include "desktop-test.moc"
