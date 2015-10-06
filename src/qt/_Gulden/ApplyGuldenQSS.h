//Font sizes - NB! We specifically use 'px' and not 'pt' for all font sizes, as qt scales 'pt' dynamically in a way that makes our fonts unacceptably small on OSX etc. it doesn't do this with px so we use px instead.
//QString CURRENCY_DECIMAL_FONT_SIZE = "11px"; // For .00 in currency and PND text.
//QString BODY_FONT_SIZE = "12px"; // Standard body font size used in 'most places'.
//QString CURRENCY_FONT_SIZE = "13px"; // For currency
//QString TOTAL_FONT_SIZE = "15px"; // For totals and account names
//QString HEADER_FONT_SIZE = "16px"; // For large headings

//Fonts
// We 'abuse' the translation system here to allow different 'font stacks' for different languages.
//QString MAIN_FONTSTACK = QObject::tr("Arial, 'Helvetica Neue', Helvetica, sans-serif");

//Load our own QSS stylesheet template for 'whole app'
QFile styleFile(":Gulden/qss");
styleFile.open(QFile::ReadOnly);

//Use the same style on all platforms to simplify skinning
//QApplication::setStyle("windows");

//Replace variables in the 'template' with actual values
QString style(styleFile.readAll());
//style.replace("BODY_FONT_SIZE",BODY_FONT_SIZE);
//style.replace("HEADER_FONT_SIZE",HEADER_FONT_SIZE);
//style.replace("CURRENCY_FONT_SIZE",CURRENCY_FONT_SIZE);
//style.replace("TOTAL_FONT_SIZE",TOTAL_FONT_SIZE);
//style.replace("CURRENCY_DECIMAL_FONT_SIZE",CURRENCY_DECIMAL_FONT_SIZE);
//style.replace("MAIN_FONTSTACK",MAIN_FONTSTACK);

//Below block is all changes relating to the status bar
{
	//Below block is all changes relating to the progress bar
	{
		//Use our own styling - clear the styling that is already applied
		progressBar->setStyleSheet("");

		//Hide text we don't want it as it looks cluttered.
		progressBar->setTextVisible(false);

		//Spacers on either side of progress bar to help force it into center
		QWidget* statusProgressSpacerL = new QWidget(statusBar());
		statusProgressSpacerL->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
		statusBar()->insertWidget(1, statusProgressSpacerL, 1);

		QWidget* statusProgressSpacerR = new QWidget(statusBar());
		statusProgressSpacerR->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
		statusBar()->insertWidget(3, statusProgressSpacerR, 1);

		//Add a spacer to the frameBlocks so that we can force them to expand to same size as the progress text (Needed for proper centering of progress bar)
		QWidget* frameBlocksSpacerL = new QWidget(frameBlocks);
		frameBlocksSpacerL->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
		frameBlocksLayout->insertWidget(0,frameBlocksSpacerL);

		//Allow us to target the progress label for easy styling
		progressBarLabel->setObjectName("progress_bar_label");
	}
	//Hide some of the 'task items' we don't need
	unitDisplayControl->setVisible(false);
	labelBlocksIcon->setVisible(false);
}

backupWalletAction->setIconText(QCoreApplication::translate("toolbar","Backup"));

//Change shortcut keys because we have hidden overview pane and changed tab orders
overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_0));
sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));

//Apply the final QSS - after making the 'template substitutions'
//NB! This should happen last after all object IDs etc. are set.
setStyleSheet(style);
