// Copyright (c) 2015-2018 The Gulden developers
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef UPDATE_CHECK_H
#define UPDATE_CHECK_H

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateCheck : public QObject
{
    Q_OBJECT

public:
    UpdateCheck();
    ~UpdateCheck();

    void check(bool noisy);

Q_SIGNALS:
    void result(bool succes, QString msg, bool important, bool noisy);

private Q_SLOTS:
    void netRequestFinished();

private:
    QNetworkAccessManager* netManager;
    QNetworkReply* networkReply;
    bool noisy;

    void start(const QUrl& url);
    void jsonResponse(const QJsonDocument& json);
    void cancel();
};

#endif
