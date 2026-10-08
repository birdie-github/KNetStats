#include "qnetstats.h"
#include "qnetstatsview.h"
#include "configure.h"

#include <QApplication>
#include <QMenu>
#include <QNetworkInterface>
#include <QSettings>
#include <QMessageBox>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QTimer>
#include <QFontDatabase>
#include <QSet>
#include <algorithm>

extern const char *programName;

QNetStats::QNetStats() : QDialog(nullptr, Qt::Window), mConfigure(nullptr) {
	// read the current views from config file
	QSettings settings;
	QStringList views = settings.value("CurrentViews", QStringList()).toStringList();

	views.removeDuplicates();
	views.sort();
	QSet<int> digits;
	for (const QString &name : views) {
		ViewOptions opts;
		readInterfaceConfig(name, &opts);
		if (!opts.mDisplayTextStatistics)
			continue;
		int digit = opts.mTextDigit;
		if (digits.contains(digit)) {
			for (digit = 0; digit < 10 && digits.contains(digit); ++digit) {}
			settings.beginGroup(name);
			if (digit < 10)
				settings.setValue("TextStatisticsDigit", digit);
			else
				settings.setValue("DisplayTextStatistics", false);
			settings.endGroup();
		}
		if (digit < 10)
			digits.insert(digit);
	}

	setup();
	if (views.empty()) {    // no views... =/, display the configuration dialog
		mConfigure->show();
	} else {
		// start the views
		for (auto &view: views) {
			auto *kview = new QNetStatsView(this, view);
			mViews[view] = kview;
		}
	}
	checkTrayIconsAvailable();
}

void QNetStats::checkTrayIconsAvailable() {
	updateFallbackWindow();
	mBackupStatisticsMenu->clear();
	QStringList names = mViews.keys();
	names.sort();
	for (const QString &name : names) {
		auto *view = mViews.value(name);
		mBackupStatisticsMenu->addAction(view->displayName(), view, &QNetStatsView::showStatistics);
	}
	mBackupStatisticsMenu->setEnabled(!names.isEmpty());
	for (auto view: mViews) {
		if (view->trayIconVisible()) {
			mBackupTrayIcon->hide();
			return;
		}
	}
	mBackupTrayIcon->show();
}

void QNetStats::setup() {
	mConfigure = new Configure(this);
	setupBackupTrayIcon();
	setupFallbackWindow();

	if (QNetworkInterface::allInterfaces().empty()) {
		QMessageBox msg(this);
		msg.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		msg.setIcon(QMessageBox::Icon::Warning);
		msg.setText("Warning:");
		msg.setInformativeText("Could not find any network interfaces!");
		msg.exec();
	}

	connect(mConfigure->mOk, &QPushButton::clicked, this, [this]() {
		if (mConfigure->canSaveConfig()) {
			saveConfig(mConfigure->options());
			mConfigure->accept();
		}
	});
	connect(mConfigure->mApply, &QPushButton::clicked, this, &QNetStats::configApply);
	connect(mConfigure->mCancel, &QPushButton::clicked, mConfigure, &QDialog::reject);
}

void QNetStats::setupBackupTrayIcon() {
	mBackupTrayIcon = new QSystemTrayIcon(QIcon(":/img/interfaces_missing.png"), this);
	mBackupTrayIcon->setToolTip("QNetStats — Configure Interfaces");
	auto *mContextMenu = new QMenu(this);
	mBackupStatisticsMenu = mContextMenu->addMenu(tr("Statistics"));
	mContextMenu->addAction("Configure Interfaces", this, &QNetStats::showConfigure);
	mContextMenu->addAction("Quit QNetStats", this, []() { QApplication::quit(); });
	mBackupTrayIcon->setContextMenu(mContextMenu);

	connect(mBackupTrayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
		if (reason != QSystemTrayIcon::Trigger)
			return;
		if (mConfigure->isVisible()) {
			mConfigure->hide();
			return;
		}
		mConfigure->show();
	});
}

void QNetStats::setupFallbackWindow() {
	mFallbackWindow = new QDialog(this);
	mFallbackWindow->setWindowTitle(programName);
	auto *layout = new QVBoxLayout(mFallbackWindow);
	auto *notice = new QLabel(tr("No system tray is available. Select an interface to view its statistics."), mFallbackWindow);
	notice->setWordWrap(true);
	layout->addWidget(notice);
	mFallbackInterfaces = new QListWidget(mFallbackWindow);
	layout->addWidget(mFallbackInterfaces);

	auto *buttons = new QHBoxLayout;
	mFallbackStatistics = new QPushButton(tr("Statistics"), mFallbackWindow);
	mFallbackStatistics->setEnabled(false);
	mFallbackStatistics->setDefault(true);
	auto *configure = new QPushButton(tr("Configure Interfaces"), mFallbackWindow);
	auto *quit = new QPushButton(tr("Quit QNetStats"), mFallbackWindow);
	buttons->addWidget(mFallbackStatistics);
	buttons->addWidget(configure);
	buttons->addWidget(quit);
	layout->addLayout(buttons);

	connect(mFallbackStatistics, &QPushButton::clicked, this, &QNetStats::showSelectedStatistics);
	connect(mFallbackInterfaces, &QListWidget::itemActivated, this, &QNetStats::showSelectedStatistics);
	connect(mFallbackInterfaces, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
		mFallbackStatistics->setEnabled(item != nullptr);
	});
	connect(configure, &QPushButton::clicked, this, &QNetStats::showConfigure);
	connect(quit, &QPushButton::clicked, this, []() { QApplication::quit(); });
	connect(mFallbackWindow, &QDialog::rejected, this, []() {
		if (!QSystemTrayIcon::isSystemTrayAvailable())
			QApplication::quit();
	});

	// Qt exposes tray availability as a query, without an availability-change signal.
	auto *timer = new QTimer(this);
	timer->setInterval(1000);
	connect(timer, &QTimer::timeout, this, &QNetStats::updateFallbackWindow);
	timer->start();
}

void QNetStats::updateFallbackWindow() {
	if (QSystemTrayIcon::isSystemTrayAvailable()) {
		mFallbackWindow->hide();
		return;
	}

	QStringList names = mViews.keys();
	names.sort();
	QStringList displayed;
	for (int i = 0; i < mFallbackInterfaces->count(); ++i)
		displayed.append(mFallbackInterfaces->item(i)->data(Qt::UserRole).toString());
	if (names != displayed) {
		const auto *selected = mFallbackInterfaces->currentItem();
		const QString selectedName = selected ? selected->data(Qt::UserRole).toString() : QString();
		const QSignalBlocker blocker(mFallbackInterfaces);
		mFallbackInterfaces->clear();
		QListWidgetItem *current = nullptr;
		for (const QString &name : names) {
			auto *item = new QListWidgetItem(interfaceDisplayName(name), mFallbackInterfaces);
			item->setData(Qt::UserRole, name);
			item->setToolTip(name);
			if (name == selectedName)
				current = item;
		}
		if (current)
			mFallbackInterfaces->setCurrentItem(current);
		else if (!names.isEmpty())
			mFallbackInterfaces->setCurrentRow(0);
	}
	mFallbackStatistics->setEnabled(mFallbackInterfaces->currentItem() != nullptr);
	if (!mFallbackWindow->isVisible())
		mFallbackWindow->show();
}

void QNetStats::showSelectedStatistics() {
	const auto *item = mFallbackInterfaces->currentItem();
	if (!item)
		return;
	auto *view = mViews.value(item->data(Qt::UserRole).toString(), nullptr);
	if (view)
		view->showStatistics();
}

void QNetStats::showConfigure() {
	mConfigure->show();
	mConfigure->raise();
	mConfigure->activateWindow();
}

QString QNetStats::interfaceDisplayName(const QString &name) {
#ifdef Q_OS_WIN
	const auto interface = QNetworkInterface::interfaceFromName(name);
	const QString friendlyName = interface.humanReadableName();
	if (!friendlyName.isEmpty())
		return friendlyName;
#endif
	return name;
}

void QNetStats::readInterfaceConfig(const QString &ifName, ViewOptions *opts) {
	QSettings settings;
	int defaultTheme = ifName.startsWith("wlan") ? 3 : 0;
#ifdef Q_OS_WIN
	if (QNetworkInterface::interfaceFromName(ifName).type() == QNetworkInterface::Wifi)
		defaultTheme = 3;
#endif

	settings.beginGroup(ifName);
	// General Settings
	opts->mUpdateInterval = settings.value("UpdateInterval", 500).toInt();
	if (opts->mUpdateInterval <= 0)
		opts->mUpdateInterval = 500;
	opts->mMonitoring = settings.value("Monitoring", true).toBool();
	opts->mNotifications = settings.value("DisplayNotifications", true).toBool();
	opts->mDisplayTrayIcon = settings.value("DisplayTrayIcon", true).toBool();
	opts->mDisplayTextStatistics = settings.value("DisplayTextStatistics", false).toBool();
	opts->mTextDigit = std::clamp(settings.value("TextStatisticsDigit", 0).toInt(), 0, 9);
	opts->mTextDigitPosition = std::clamp(settings.value("TextStatisticsDigitPosition", 0).toInt(), 0, 3);
	opts->mTextDigitColor = settings.value("TextStatisticsDigitColor", "#ffd700").toString();
	opts->mTextColor = settings.value("TextStatisticsColor", "#ffffff").toString();
	opts->mTextBackgroundColor = settings.value("TextStatisticsBackgroundColor", "#202020").toString();
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	font.setBold(true);
	opts->mTextFont = settings.value("TextStatisticsFont", QVariant::fromValue(font)).value<QFont>();
	opts->mTheme = settings.value("Theme", defaultTheme).toInt();
	// Graph Settings
	opts->mChartUplColor = settings.value("ChartUplColor", "#FF0000").toString();
	opts->mChartDldColor = settings.value("ChartDldColor", "#00FF00").toString();
	opts->mChartBgColor = settings.value("ChartBgColor", "#000000").toString();
	opts->mChartTransparentBackground = settings.value("ChartUseTransparentBackground", false).toBool();
	settings.endGroup();
}

void QNetStats::configApply() {
	if (mConfigure->canSaveConfig())
		saveConfig(mConfigure->options());
}

void QNetStats::saveConfig(const OptionsMap &options) {
	QSettings settings;

	for (OptionsMap::ConstIterator i = options.begin(); i != options.end(); ++i) {
		TrayIconMap::Iterator trayIcon = mViews.find(i.key());
		const ViewOptions &opt = i.value();

		settings.beginGroup(i.key());
		// General Options
		settings.setValue("UpdateInterval", opt.mUpdateInterval);
		settings.setValue("Monitoring", opt.mMonitoring);
		settings.setValue("DisplayNotifications", opt.mNotifications);
		settings.setValue("DisplayTrayIcon", opt.mDisplayTrayIcon);
		settings.setValue("DisplayTextStatistics", opt.mDisplayTextStatistics);
		settings.setValue("TextStatisticsDigit", opt.mTextDigit);
		settings.setValue("TextStatisticsDigitPosition", opt.mTextDigitPosition);
		settings.setValue("TextStatisticsDigitColor", opt.mTextDigitColor);
		settings.setValue("TextStatisticsColor", opt.mTextColor);
		settings.setValue("TextStatisticsBackgroundColor", opt.mTextBackgroundColor);
		settings.setValue("TextStatisticsFont", QVariant::fromValue(opt.mTextFont));
		settings.setValue("Theme", opt.mTheme);
		// Chart Options
		settings.setValue("ChartUplColor", opt.mChartUplColor);
		settings.setValue("ChartDldColor", opt.mChartDldColor);
		settings.setValue("ChartBgColor", opt.mChartBgColor);
		settings.setValue("ChartUseTransparentBackground", opt.mChartTransparentBackground);
		settings.endGroup();

		if (opt.mMonitoring) {    // check if we are already monitoring this interface.
			if (trayIcon == mViews.end()) { // new interface!
				auto *kview = new QNetStatsView(this, i.key());
				mViews[i.key()] = kview;
			} else
				trayIcon.value()->updateViewOptions();
		} else {
			// Check if a tray icon exist and remove then!
			if (trayIcon != mViews.end()) {
				delete trayIcon.value();
				mViews.erase(trayIcon);
			}
		}
		checkTrayIconsAvailable();
	}

	settings.setValue("CurrentViews", QStringList(mViews.keys()));
}

void QNetStats::showInterfaceNotification(const QString &message,
		QSystemTrayIcon::MessageIcon icon, int milliseconds) {
	// Notifications remain independent of either interface icon switch.
	QSystemTrayIcon *target = mBackupTrayIcon;
	for (auto *tray : findChildren<QSystemTrayIcon *>()) {
		if (tray->isVisible()) {
			target = tray;
			break;
		}
	}
	target->showMessage(programName, message, icon, milliseconds);
}
