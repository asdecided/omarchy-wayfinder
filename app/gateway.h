#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QTimer>
#include <functional>
#include <memory>

// The gateway emits one JSON data record per SSE line. Reject incomplete or
// ambiguous streams instead of presenting partial text as a verified response.
struct GatewayStream {
  QByteArray pending;
  QString text;
  QJsonObject decision;
  bool complete = false, failed = false;
  qsizetype received = 0;
  void consume(const QByteArray &bytes) {
    received += bytes.size();
    if (received > 1024 * 1024) {
      failed = true;
      return;
    }
    pending += bytes;
    while (pending.contains('\n')) {
      auto end = pending.indexOf('\n');
      auto line = pending.left(end).trimmed();
      pending.remove(0, end + 1);
      if (!line.startsWith("data:"))
        continue;
      if (complete) {
        failed = true;
        return;
      }
      auto data = line.mid(5).trimmed();
      if (data == "[DONE]") {
        complete = true;
        continue;
      }
      QJsonParseError error;
      auto document = QJsonDocument::fromJson(data, &error);
      if (error.error != QJsonParseError::NoError || !document.isObject()) {
        failed = true;
        return;
      }
      auto obj = document.object();
      if (obj.contains("error")) {
        failed = true;
        return;
      }
      if (obj.contains("wayfinder")) {
        if (!decision.isEmpty() || !obj["wayfinder"].isObject()) {
          failed = true;
          return;
        }
        decision = obj["wayfinder"].toObject();
      }
      for (const auto &v : obj["choices"].toArray()) {
        const auto content =
            v.toObject()["delta"].toObject()["content"].toString();
        if (!content.isEmpty() && decision.isEmpty()) {
          failed = true;
          return;
        }
        text += content;
      }
    }
  }
};

class Gateway : public QObject {
  QNetworkAccessManager manager;

public:
  explicit Gateway(QObject *parent = nullptr) : QObject(parent) {
    manager.setProxy(QNetworkProxy::NoProxy);
  }
  QNetworkReply *
  stream(const QJsonObject &body, std::function<void(const QString &)> progress,
         std::function<void(bool, const QJsonObject &, const QByteArray &)>
             callback) {
    QNetworkRequest request(QUrl("http://127.0.0.1:8088/v1/chat/completions"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-Wayfinder-Debug", "1");
    request.setTransferTimeout(90000);
    auto *reply = manager.post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    reply->setReadBufferSize(65536);
    auto state = std::make_shared<GatewayStream>();
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->start(95000);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::readyRead, this, [reply, state, progress] {
      const auto code =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      auto bytes = reply->readAll();
      if (code < 200 || code >= 300) {
        reply->abort();
        return;
      }
      state->consume(bytes);
      if (state->failed) {
        reply->abort();
        return;
      }
      progress(state->text);
    });
    connect(reply, &QNetworkReply::finished, this, [reply, state, callback] {
      state->consume(reply->readAll());
      const auto id = reply->rawHeader("x-wayfinder-router-request-id");
      const auto status =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      const bool ok = reply->error() == QNetworkReply::NoError &&
                      status >= 200 && status < 300 && !state->failed &&
                      state->complete && !state->decision.isEmpty() &&
                      !id.isEmpty() &&
                      state->decision["request_id"].toString().toUtf8() == id;
      QJsonObject result{
          {"wayfinder", state->decision},
          {"choices",
           QJsonArray{QJsonObject{
               {"message", QJsonObject{{"content", state->text}}}}}}};
      callback(ok, ok ? result : QJsonObject{}, id);
      reply->deleteLater();
    });
    return reply;
  }
  QNetworkReply *
  request(const QString &path, const QJsonObject &body,
          std::function<void(bool, const QJsonObject &, const QByteArray &)>
              callback) {
    // All callers use literal paths; neither redirects nor proxies can send
    // prompts elsewhere.
    QNetworkRequest request(QUrl("http://127.0.0.1:8088/" + path));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-Wayfinder-Debug", "1");
    request.setTransferTimeout(body.isEmpty() ? 5000 : 90000);
    auto *reply =
        body.isEmpty()
            ? manager.get(request)
            : manager.post(request,
                           QJsonDocument(body).toJson(QJsonDocument::Compact));
    reply->setReadBufferSize(65536);
    auto data = std::make_shared<QByteArray>();
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->start(body.isEmpty() ? 6000 : 95000);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::readyRead, this, [reply, data] {
      *data += reply->readAll();
      if (data->size() > 1024 * 1024) {
        data->clear();
        reply->abort();
      }
    });
    connect(reply, &QNetworkReply::finished, this, [reply, data, callback] {
      *data += reply->readAll();
      QJsonParseError error;
      auto document = QJsonDocument::fromJson(*data, &error);
      const int status =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      bool ok = reply->error() == QNetworkReply::NoError && status >= 200 &&
                status < 300 && data->size() <= 1024 * 1024 &&
                error.error == QJsonParseError::NoError && document.isObject();
      callback(ok, ok ? document.object() : QJsonObject{},
               reply->rawHeader("x-wayfinder-router-request-id"));
      reply->deleteLater();
    });
    return reply;
  }
};
