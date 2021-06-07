// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying file COPYING

#if defined(HAVE_CONFIG_H)
#include "config/build-config.h"
#endif

#include "miningaccountdialog.h"
#include <qt/forms/ui_miningaccountdialog.h>

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "units.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "receiverequestdialog.h"
#include "recentrequeststablemodel.h"
#include "walletmodel.h"
#include "wallet/wallet.h"

#include <QAction>
#include <QCursor>
#include <QItemSelection>
#include <QMessageBox>
#include <QScrollBar>
#include <QTextDocument>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QToolTip>

#include "clientmodel.h"
#include "editaddressdialog.h"

#include "GuldenGUI.h"
#include <generation/miner.h>

#include <crypto/hash/sigma/sigma.h>
#include <compat/sys.h>
#include "alert.h"

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

MiningAccountDialog::MiningAccountDialog(const QStyle *_platformStyle, QWidget *parent)
: QDialog( parent )
, ui( new Ui::MiningAccountDialog )
, model( 0 )
, platformStyle( _platformStyle )
, buyReceiveAddress( NULL )
, currentAccount( NULL )
{
    ui->setupUi(this);

    ui->miningStartminingButton->setCursor(Qt::PointingHandCursor);
    ui->miningStopminingButton->setCursor(Qt::PointingHandCursor);
    ui->miningCopyToClipboardButton->setCursor(Qt::PointingHandCursor);
    ui->miningCopyToClipboardButton->setTextFormat( Qt::RichText );
    ui->miningCopyToClipboardButton->setText( GUIUtil::fontAwesomeRegular("\uf0c5") );
    ui->miningCopyToClipboardButton->setContentsMargins(0, 0, 0, 0);
    ui->miningEditAddressButton->setCursor(Qt::PointingHandCursor);
    ui->miningEditAddressButton->setTextFormat( Qt::RichText );
    ui->miningEditAddressButton->setText( GUIUtil::fontAwesomeRegular("\uf044") );
    ui->miningEditAddressButton->setContentsMargins(0, 0, 0, 0);
    ui->miningResetAddressButton->setCursor(Qt::PointingHandCursor);
    ui->miningResetAddressButton->setTextFormat( Qt::RichText );
    ui->miningResetAddressButton->setText( GUIUtil::fontAwesomeRegular("\uf12d") );
    ui->miningResetAddressButton->setContentsMargins(0, 0, 0, 0);
    ui->miningCheckBoxKeepOpen->setCursor(Qt::PointingHandCursor);
    ui->miningCheckBoxMineAtStartup->setCursor(Qt::PointingHandCursor);
    
    ui->miningThreadSlider->setCursor(Qt::PointingHandCursor);
    ui->miningThreadSlider->setContentsMargins(0, 0, 0, 0);
    ui->miningThreadSlider->setToolTip(tr("Increasing the number of threads will increase your mining speed, but will also increase your energy usage and slow down other appliations that might be running."));
    
    ui->miningArenaThreadSlider->setCursor(Qt::PointingHandCursor);
    ui->miningArenaThreadSlider->setContentsMargins(0, 0, 0, 0);
    ui->miningArenaThreadSlider->setToolTip(tr("Increasing the number of threads will increase your arena setup time, but will also increase your energy usage and slow down other appliations that might be running."));
    
    ui->miningMemorySlider->setCursor(Qt::PointingHandCursor);
    ui->miningMemorySlider->setContentsMargins(0, 0, 0, 0);
    ui->miningMemorySlider->setToolTip(tr("Reducing memory usage is not recommended. It will slow down your mining while using the same amount of processor power and energy as before. Only use this as a last resort on machines that have low memory availability."));

    ui->miningStopminingButton->setVisible(false);
    
    uint32_t nThreadMax = std::max((uint32_t)2, (uint32_t)std::thread::hardware_concurrency());
    uint32_t nThreadSel = std::max((uint32_t)2, nThreadMax-2);
    ui->miningThreadSlider->setMinimum(1);
    ui->miningThreadSlider->setMaximum(nThreadMax);
    ui->miningThreadSlider->setTickInterval(1);
    ui->miningThreadSlider->setPageStep(1);
    ui->miningThreadSlider->setSingleStep(1);
    ui->miningThreadSlider->setTickPosition(QSlider::TicksBelow);
    ui->miningThreadSlider->setValue(nThreadSel);
    ui->miningThreadSlider->setTracking(false);
    
    ui->miningArenaThreadSlider->setMinimum(1);
    ui->miningArenaThreadSlider->setMaximum(nThreadMax);
    ui->miningArenaThreadSlider->setTickInterval(1);
    ui->miningArenaThreadSlider->setPageStep(1);
    ui->miningArenaThreadSlider->setSingleStep(1);
    ui->miningArenaThreadSlider->setTickPosition(QSlider::TicksBelow);
    ui->miningArenaThreadSlider->setValue(nThreadSel);
    ui->miningArenaThreadSlider->setTracking(false);
    
    uint64_t systemMemoryInMb = systemPhysicalMemoryInBytes()/1024/1024;
    uint64_t nMaxMemoryInMb = std::min(systemMemoryInMb, defaultSigmaSettings.arenaSizeKb/1024);
    // 32 bit windows can only address 2gb of memory per process (3gb if /largeaddressaware)
    // 32 bit linux is 4gb per process.
    // Limit both accordingly
    #ifdef ARCH_X86
        #ifdef WIN32
            nMaxMemoryInMb = std::min((uint64_t)nMaxMemoryInMb, (uint64_t)1*1024);
        #else
            nMaxMemoryInMb = std::min((uint64_t)nMaxMemoryInMb, (uint64_t)2*1024);
        #endif
    #endif

    ui->miningMemorySlider->setMinimum(128);
    ui->miningMemorySlider->setMaximum(nMaxMemoryInMb);
    ui->miningMemorySlider->setTickInterval(16);
    ui->miningMemorySlider->setSingleStep(16);
    ui->miningMemorySlider->setPageStep(1024);
    //fixme: (SIGMA) - Try pick a reasonable default for lower memory (e.g. 3gb) machines.
    ui->miningMemorySlider->setValue(ui->miningMemorySlider->maximum());
    ui->miningMemorySlider->setTracking(false);
    
    updateSliderLabels();
    
    ui->miningResetAddressButton->setVisible(false);
    updateAddress("");
    
    if (PoWGenerationIsActive())
    {
        slotUpdateMiningStats();
    }
}

void MiningAccountDialog::updateSliderLabels()
{
    ui->miningMemorySliderLabel->setText(tr("%1 MB").arg(ui->miningMemorySlider->value()));
    ui->miningThreadSliderLabel->setText(tr("%1 threads").arg(ui->miningThreadSlider->value()));
    ui->miningArenaThreadSliderLabel->setText(tr("%1 threads").arg(ui->miningArenaThreadSlider->value()));
}

void MiningAccountDialog::startMining()
{
    LogPrintf("MiningAccountDialog::startMining\n");

    uint64_t nGenProcLimit = ui->miningThreadSlider->value();
    uint64_t nGenArenaProcLimit = ui->miningArenaThreadSlider->value();
    uint64_t nGenMemoryLimitKilobytes = ((uint64_t)ui->miningMemorySlider->value())*1024;
    
    if(clientModel && clientModel->getOptionsModel())
    {
        clientModel->getOptionsModel()->setMineThreadCount(nGenProcLimit);
        clientModel->getOptionsModel()->setMineArenaThreadCount(nGenArenaProcLimit);
        clientModel->getOptionsModel()->setMineMemory(nGenMemoryLimitKilobytes);
    }
    
    startMining(currentAccount, nGenProcLimit, nGenArenaProcLimit, nGenMemoryLimitKilobytes, overrideAddress.size() > 0 ? overrideAddress.toStdString() : accountAddress.toStdString());

    ui->miningStopminingButton->setVisible(true);
    ui->miningStartminingButton->setVisible(false);
    
    slotUpdateMiningStats();
}

void MiningAccountDialog::startMining(CAccount* forAccount, uint64_t numThreads, uint64_t numArenaThreads, uint64_t mineMemoryKb, std::string overrideAddress)
{
    std::thread([=]
    {
        try
        {
            PoWGenerateBlocks(true, numThreads, numArenaThreads, mineMemoryKb, Params(), forAccount, overrideAddress);
        }
        catch(...)
        {
            CAlert::Notify("Error in mining loop.", true, true);
        }
    }).detach();
}

//fixme: (SIGMA) Fire from timer instead of immediately
void MiningAccountDialog::restartMiningIfNeeded()
{
    if (PoWGenerationIsActive())
    {
        startMining();
    }
}

void MiningAccountDialog::updateAddress(const QString& address)
{
    accountAddress = address;
    
    if (overrideAddress.size() > 0)
    {
        ui->miningEditAddressButton->setVisible(false);
        ui->miningResetAddressButton->setVisible(true);
        ui->accountAddress->setText(overrideAddress);
    }
    else
    {
        ui->miningEditAddressButton->setVisible(true);
        ui->miningResetAddressButton->setVisible(false);
        ui->accountAddress->setText(accountAddress);
    }
    
    if (PoWGenerationIsActive())
    {
        ui->miningStopminingButton->setVisible(true);
        ui->miningStartminingButton->setVisible(false);
    }
    else
    {
        ui->miningStopminingButton->setVisible(false);
        ui->miningStartminingButton->setVisible(true);
    }
}

void MiningAccountDialog::setActiveAccount(CAccount* account)
{
    currentAccount = account;
}


void MiningAccountDialog::activeAccountChanged(CAccount* activeAccount)
{
    LogPrintf("MiningAccountDialog::activeAccountChanged\n");

    setActiveAccount(activeAccount);
}

void MiningAccountDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    connect(model, SIGNAL(activeAccountChanged(CAccount*)), this, SLOT(activeAccountChanged(CAccount*)));
    activeAccountChanged(model->getActiveAccount());
    
    std::string readOverrideAddress;
    CWalletDB(*pactiveWallet->dbw).ReadMiningAddressString(readOverrideAddress);
    overrideAddress = QString::fromStdString(readOverrideAddress);
    updateAddress(accountAddress);
    
    static bool init=true;
    if (init)
    {
        init=false;
        connect(ui->miningCopyToClipboardButton, SIGNAL(clicked()),         this, SLOT(slotCopyAddressToClipboard()));
        connect(ui->miningEditAddressButton,     SIGNAL(clicked()),         this, SLOT(slotEditMiningAddress()));
        connect(ui->miningResetAddressButton,    SIGNAL(clicked()),         this, SLOT(slotResetMiningAddress()));
        connect(ui->miningStartminingButton,     SIGNAL(clicked()),         this, SLOT(slotStartMining()));
        connect(ui->miningStopminingButton,      SIGNAL(clicked()),         this, SLOT(slotStopMining()));
        connect(ui->miningCheckBoxKeepOpen,      SIGNAL(clicked()),         this, SLOT(slotKeepOpenWhenMining()));
        connect(ui->miningCheckBoxMineAtStartup, SIGNAL(clicked()),         this, SLOT(slotMineAtStartup()));
        connect(ui->miningMemorySlider,          SIGNAL(sliderMoved(int)),  this, SLOT(slotMiningMemorySettingChanging(int)));
        connect(ui->miningThreadSlider,          SIGNAL(sliderMoved(int)),  this, SLOT(slotMiningThreadSettingChanging(int)));
        connect(ui->miningArenaThreadSlider,     SIGNAL(sliderMoved(int)),  this, SLOT(slotMiningArenaThreadSettingChanging(int)));
        connect(ui->miningMemorySlider,          SIGNAL(valueChanged(int)), this, SLOT(slotMiningMemorySettingChanged())); //Emitted only when user releases mouse at end of slider movement, not for every change
        connect(ui->miningThreadSlider,          SIGNAL(valueChanged(int)), this, SLOT(slotMiningThreadSettingChanged())); //Emitted only when user releases mouse at end of slider movement, not for every change
        connect(ui->miningArenaThreadSlider,     SIGNAL(valueChanged(int)), this, SLOT(slotMiningArenaThreadSettingChanged())); //Emitted only when user releases mouse at end of slider movement, not for every change
    }
}

void MiningAccountDialog::setClientModel(ClientModel* clientModel_)
{
    LogPrint(BCLog::QT, "MiningAccountDialog::setClientModel\n");
    clientModel = clientModel_;
    
    if(clientModel->getOptionsModel())
    {
        ui->miningCheckBoxKeepOpen->setChecked(clientModel->getOptionsModel()->getKeepOpenWhenMining());
        ui->miningCheckBoxMineAtStartup->setChecked(clientModel->getOptionsModel()->getMineAtStartup());
        
        uint64_t nGenProcLimit = clientModel->getOptionsModel()->getMineThreadCount();
        uint64_t nGenArenaProcLimit = clientModel->getOptionsModel()->getMineArenaThreadCount();
        uint64_t nGenMemoryLimitKilobytes = clientModel->getOptionsModel()->getMineMemory();
        if (nGenProcLimit > 0)
            ui->miningThreadSlider->setValue(nGenProcLimit);
        if (nGenArenaProcLimit > 0)
            ui->miningArenaThreadSlider->setValue(nGenProcLimit);
        if (nGenMemoryLimitKilobytes > 0)
            ui->miningMemorySlider->setValue(nGenMemoryLimitKilobytes/1024);
    }
    updateSliderLabels();
}

MiningAccountDialog::~MiningAccountDialog()
{
    delete ui;
}

void MiningAccountDialog::slotCopyAddressToClipboard()
{
    GUIUtil::setClipboard(accountAddress);
    QToolTip::showText(ui->miningCopyToClipboardButton->mapToGlobal(QPoint(0,0)), tr("Address copied to clipboard"),ui->miningCopyToClipboardButton);
}

void MiningAccountDialog::slotEditMiningAddress()
{
    LogPrintf("MiningAccountDialog::slotEditMiningAddress\n");
    if (model)
    {
        EditAddressDialog dlg(EditAddressDialog::NewMiningAddress, this, tr("Mining address"));
        dlg.setModel(model->getAddressTableModel());
        if(dlg.exec())
        {
            overrideAddress = dlg.getAddress();
            std::string strWriteOverrideAddress = overrideAddress.toStdString();
            CWalletDB(*pactiveWallet->dbw).WriteMiningAddressString(strWriteOverrideAddress);
            updateAddress(accountAddress);
            restartMiningIfNeeded();
        }
    }
}

void MiningAccountDialog::slotResetMiningAddress()
{
    LogPrintf("MiningAccountDialog::slotResetMiningAddress\n");

    if(pactiveWallet)
    {
        overrideAddress = "";
        CWalletDB(*pactiveWallet->dbw).WriteMiningAddressString("");
        updateAddress(accountAddress);
        restartMiningIfNeeded();
    }        
}

void MiningAccountDialog::slotStartMining()
{
    LogPrintf("MiningAccountDialog::slotStartMining\n");
    startMining();
}

void MiningAccountDialog::slotStopMining()
{
    LogPrintf("MiningAccountDialog::slotStopMining\n");
    std::thread([=]
    {
        PoWStopGeneration();
    }).detach();
    
    ui->miningStopminingButton->setVisible(false);
    ui->miningStartminingButton->setVisible(true);
    
    ui->labelLastMiningSpeed->setText("0.00");
    ui->labelLastMiningSpeedUnits->setText(" Mh/s");
    ui->labelAverageMiningSpeed->setText("0.00");
    ui->labelAverageMiningSpeedUnits->setText(" Mh/s");
    ui->labelBestMiningSpeed->setText("0.00");
    ui->labelBestMiningSpeedUnits->setText(" Mh/s");
    ui->labelLastArenaSetupTime->setText("0.00");
}

void MiningAccountDialog::slotKeepOpenWhenMining()
{
    if(clientModel && clientModel->getOptionsModel())
    {
        clientModel->getOptionsModel()->setKeepOpenWhenMining(ui->miningCheckBoxKeepOpen->isChecked());
    }
}

void MiningAccountDialog::slotMineAtStartup()
{
    if(clientModel && clientModel->getOptionsModel())
    {
        clientModel->getOptionsModel()->setMineAtStartup(ui->miningCheckBoxMineAtStartup->isChecked());
    }
}

void MiningAccountDialog::slotMiningMemorySettingChanged()
{
    updateSliderLabels();
    restartMiningIfNeeded();
}

void MiningAccountDialog::slotMiningThreadSettingChanged()
{
    updateSliderLabels();
    restartMiningIfNeeded();
}

void MiningAccountDialog::slotMiningArenaThreadSettingChanged()
{
    updateSliderLabels();
    restartMiningIfNeeded();
}

void MiningAccountDialog::slotMiningMemorySettingChanging(int val)
{
    ui->miningMemorySliderLabel->setText(tr("%1 MB").arg(val));
}

void MiningAccountDialog::slotMiningThreadSettingChanging(int val)
{
    ui->miningThreadSliderLabel->setText(tr("%1 threads").arg(val));
}

void MiningAccountDialog::slotMiningArenaThreadSettingChanging(int val)
{
    ui->miningArenaThreadSliderLabel->setText(tr("%1 threads").arg(val));
}

void MiningAccountDialog::slotUpdateMiningStats()
{
    if (PoWGenerationIsActive())
    {
        //fixme: (SIGMA) (DEDUP) - Share this code with RPC if possible
        double dHashPerSecLog = dHashesPerSec;
        std::string sHashPerSecLogLabel = " h";
        selectLargesHashUnit(dHashPerSecLog, sHashPerSecLogLabel);
        double dRollingHashPerSecLog = dRollingHashesPerSec;
        std::string sRollingHashPerSecLogLabel = " h";
        selectLargesHashUnit(dRollingHashPerSecLog, sRollingHashPerSecLogLabel);
        double dBestHashPerSecLog = dBestHashesPerSec;
        std::string sBestHashPerSecLogLabel = " h";
        selectLargesHashUnit(dBestHashPerSecLog, sBestHashPerSecLogLabel);

        
        ui->labelLastMiningSpeed->setText(QString::fromStdString(strprintf("%.2lf", dHashPerSecLog)));
        ui->labelLastMiningSpeedUnits->setText(QString::fromStdString(" "+sHashPerSecLogLabel+"/s"));
        ui->labelAverageMiningSpeed->setText(QString::fromStdString(strprintf("%.2lf", dRollingHashPerSecLog)));
        ui->labelAverageMiningSpeedUnits->setText(QString::fromStdString(" "+sRollingHashPerSecLogLabel+"/s"));
        ui->labelBestMiningSpeed->setText(QString::fromStdString(strprintf("%.2lf", dBestHashPerSecLog)));
        ui->labelBestMiningSpeedUnits->setText(QString::fromStdString(" "+sBestHashPerSecLogLabel+"/s"));
        ui->labelLastArenaSetupTime->setText(QString::fromStdString(strprintf("%.2lf", nArenaSetupTime/1000.0)));
    
    
        // Call again every 2 seconds as long as mining is active
        QTimer::singleShot( 2000, this, SLOT(slotUpdateMiningStats()) );
    }
}
