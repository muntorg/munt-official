// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BITCOIN_QT_CLIENTMODEL_H
#define BITCOIN_QT_CLIENTMODEL_H

#include <QObject>
#include <QDateTime>

class AddressTableModel;
class BanTableModel;
class OptionsModel;
class PeerTableModel;
class TransactionTableModel;

class CWallet;
class CBlockIndex;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

enum BlockSource {
    BLOCK_SOURCE_NONE,
    BLOCK_SOURCE_REINDEX,
    BLOCK_SOURCE_DISK,
    BLOCK_SOURCE_NETWORK
};

enum NumConnections {
    CONNECTIONS_NONE = 0,
    CONNECTIONS_IN = (1U << 0),
    CONNECTIONS_OUT = (1U << 1),
    CONNECTIONS_ALL = (CONNECTIONS_IN | CONNECTIONS_OUT),
};

/** Model for Bitcoin network client. */
class ClientModel : public QObject {
    Q_OBJECT

public:
    explicit ClientModel(OptionsModel* optionsModel, QObject* parent = 0);
    ~ClientModel();

    OptionsModel* getOptionsModel();
    PeerTableModel* getPeerTableModel();
    BanTableModel* getBanTableModel();

    int getNumConnections(unsigned int flags = CONNECTIONS_ALL) const;
    int getNumBlocks() const;

    long getMempoolSize() const;

    size_t getMempoolDynamicUsage() const;

    quint64 getTotalBytesRecv() const;
    quint64 getTotalBytesSent() const;

    double getVerificationProgress(const CBlockIndex* tip) const;
    QDateTime getLastBlockDate() const;

    bool inInitialBlockDownload() const;

    enum BlockSource getBlockSource() const;

    QString getStatusBarWarnings() const;

    QString formatFullVersion() const;
    QString formatSubVersion() const;
    bool isReleaseVersion() const;
    QString formatClientStartupTime() const;
    QString dataDir() const;

private:
    OptionsModel* optionsModel;
    PeerTableModel* peerTableModel;
    BanTableModel* banTableModel;

    QTimer* pollTimer;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

Q_SIGNALS:
    void numConnectionsChanged(int count);
    void numBlocksChanged(int count, const QDateTime& blockDate, double nVerificationProgress, bool header);
    void mempoolSizeChanged(long count, size_t mempoolSizeInBytes);
    void alertsChanged(const QString& warnings);
    void bytesChanged(quint64 totalBytesIn, quint64 totalBytesOut);

    void message(const QString& title, const QString& message, unsigned int style);

    void showProgress(const QString& title, int nProgress);

public Q_SLOTS:
    void updateTimer();
    void updateNumConnections(int numConnections);
    void updateAlert(const QString& hash, int status);
    void updateBanlist();
};

#endif // BITCOIN_QT_CLIENTMODEL_H
