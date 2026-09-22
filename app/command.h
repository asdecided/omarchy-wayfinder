#pragma once
#include <QProcess>
#include <QTimer>
#include <functional>

// One bounded child per feature. Never launch a shell or expose stderr.
class Command : public QObject {
public:
  QProcess process;
  QTimer timeout, killer;
  QByteArray bytes, input;
  bool cancelled = false;
  std::function<void(bool, const QByteArray &)> done;
  explicit Command(QObject *parent = nullptr) : QObject(parent) {
    timeout.setSingleShot(true);
    killer.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, this, [this] { cancel(); });
    connect(&killer, &QTimer::timeout, &process, &QProcess::kill);
    connect(&process, &QProcess::started, this, [this] {
      process.write(input);
      input.fill(0);
      input.clear();
      process.closeWriteChannel();
    });
    connect(&process, &QProcess::readyReadStandardOutput, this, [this] {
      bytes += process.readAllStandardOutput();
      if (bytes.size() > 1024 * 1024) {
        bytes.clear();
        cancel();
      }
    });
    connect(&process, &QProcess::readyReadStandardError, this,
            [this] { process.readAllStandardError(); });
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus state) {
              timeout.stop();
              killer.stop();
              input.fill(0);
              input.clear();
              bytes += process.readAllStandardOutput();
              if (bytes.size() > 1024 * 1024) {
                bytes.clear();
                cancelled = true;
              }
              auto callback = std::move(done);
              done = {};
              if (callback)
                callback(!cancelled && code == 0 &&
                             state == QProcess::NormalExit,
                         bytes);
              bytes.clear();
            });
    connect(&process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError e) {
              if (e != QProcess::FailedToStart)
                return;
              timeout.stop();
              input.fill(0);
              input.clear();
              auto callback = std::move(done);
              done = {};
              if (callback)
                callback(false, {});
            });
  }
  ~Command() override {
    done = {};
    if (busy()) {
      process.kill();
      process.waitForFinished(1000);
    }
  }
  bool busy() const { return process.state() != QProcess::NotRunning; }
  bool start(const QString &program, const QStringList &args,
             QByteArray payload,
             std::function<void(bool, const QByteArray &)> callback,
             const QProcessEnvironment &env =
                 QProcessEnvironment::systemEnvironment()) {
    if (busy()) {
      payload.fill(0);
      return false;
    }
    bytes.clear();
    cancelled = false;
    input = payload;
    payload.fill(0);
    done = std::move(callback);
    process.setProcessEnvironment(env);
    process.setProgram(program);
    process.setArguments(args);
    timeout.start(100000);
    process.start();
    return true;
  }
  void cancel() {
    cancelled = true;
    process.terminate();
    killer.start(1000);
  }
};
