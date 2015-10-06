//History at front, add a backup button as well.
toolbar->removeAction(historyAction);
toolbar->insertAction(overviewAction, historyAction);
historyAction->setChecked(true);
toolbar->addAction(backupWalletAction);

//Spacers around so that tabs are centered.
QWidget* spacerL = new QWidget();
spacerL->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
toolbar->insertWidget(historyAction, spacerL);
QWidget* spacerR = new QWidget();
spacerR->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
toolbar->addWidget(spacerR);

//Expand toolbar to take full width
toolbar->setMovable(false);
toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

//Hide the 'Overview'
overviewAction->setVisible(false);

toolbar->setWindowTitle(QCoreApplication::translate("toolbar","Navigation toolbar"));

//Add the 'Gulden bar'
QToolBar *guldenBar = new QToolBar(QCoreApplication::translate("toolbar","Overview toolbar"));
insertToolBar(toolbar, guldenBar);
insertToolBarBreak(toolbar);
guldenBar->setObjectName("gulden_bar");
guldenBar->setMovable(false);
guldenBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
guldenBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
guldenBar->setIconSize(QSize(24,24));

float devicePixelRatio = 1.0;
#if QT_VERSION > 0x050100
    devicePixelRatio = ((QGuiApplication*)QCoreApplication::instance())->devicePixelRatio();
#endif

QAction *homeAction = new QAction(QIcon(devicePixelRatio<1.1?":/Gulden/logo":":/Gulden/logo_x2"), "", this);
homeAction->setCheckable(false);
guldenBar->addAction(homeAction);
connect(guldenBar->widgetForAction(homeAction), SIGNAL(clicked()), this, SLOT(gotoWebsite()));

QWidget* spacer = new QWidget();
spacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
guldenBar->addWidget(spacer);

QLabel* labelBalance = new QLabel("");
labelBalance->setObjectName("gulden_label_balance");
labelBalance->setText(BitcoinUnits::formatWithUnit(BitcoinUnits::NLG, 0, false, BitcoinUnits::separatorStandard, 2));


QLabel* labelChange = new QLabel("");
labelChange->setObjectName("gulden_label_change");
labelChange->hide();

guldenBar->addWidget(labelBalance);
guldenBar->addWidget(labelChange);

QWidget* margin = new QWidget();
margin->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
margin->setObjectName("gulden_bar_right_margin");
guldenBar->addWidget(margin);

