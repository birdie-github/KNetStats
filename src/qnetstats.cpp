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

extern const char *programName;

QNetStats::QNetStats() : QDialog(nullptr, Qt::Window), mConfigure(nullptr) {
	// read the current views from config file
	QSettings settings;
	QStringList views = settings.value("CurrentViews", QStringList()).toStringList();

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
	mBackupTrayIcon->setToolTip("All Interfaces Unavailable");
	auto *mContextMenu = new QMenu(this);
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
		displayed.append(mFallbackInterfaces->item(i)->text());
	if (names != displayed) {
		const auto *selected = mFallbackInterfaces->currentItem();
		const QString selectedName = selected ? selected->text() : QString();
		const QSignalBlocker blocker(mFallbackInterfaces);
		mFallbackInterfaces->clear();
		mFallbackInterfaces->addItems(names);
		const auto matches = mFallbackInterfaces->findItems(selectedName, Qt::MatchExactly);
		if (!matches.isEmpty())
			mFallbackInterfaces->setCurrentItem(matches.first());
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
	auto *view = mViews.value(item->text(), nullptr);
	if (view)
		view->showStatistics();
}

void QNetStats::showConfigure() {
	mConfigure->show();
	mConfigure->raise();
	mConfigure->activateWindow();
}

void QNetStats::readInterfaceConfig(const QString &ifName, ViewOptions *opts) {
	QSettings settings;
	int defaultTheme = ifName.startsWith("wlan") ? 3 : 0;

	settings.beginGroup(ifName);
	// General Settings
	opts->mUpdateInterval = settings.value("UpdateInterval", 500).toInt();
	if (opts->mUpdateInterval <= 0)
		opts->mUpdateInterval = 500;
	opts->mMonitoring = settings.value("Monitoring", true).toBool();
	opts->mNotifications = settings.value("DisplayNotifications", true).toBool();
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
