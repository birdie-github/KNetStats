#include <QListWidget>
#include <QNetworkInterface>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QStringList>

#include "configure.h"
#include "knetstats.h"

#include "ui_configurebase.h"

Configure::Configure(QWidget *parent)
		: QDialog(parent), Ui::ConfigureBase(), mInterfaceIcon(":/img/icon_pci.png") {
	setupUi(this);
	mInterfaces->setViewMode(QListWidget::ListMode);

	refreshInterfaces();
	connect(this->mRefreshBtn, &QPushButton::clicked, this, &Configure::refreshInterfaces);
	connect(mInterfaces, &QListWidget::currentItemChanged, this, &Configure::changeInterface);
	connect(mTheme, qOverload<int>(&QComboBox::activated), this, &Configure::changeTheme);
}

void Configure::showEvent(QShowEvent *event) {
	if (event->spontaneous()) {
		QDialog::showEvent(event);
		return;
	}
	const QString selected = mCurrentItem;
	mCurrentItem.clear();
	mConfig.clear();
	QSettings settings;
	const QStringList monitored = settings.value("CurrentViews", QStringList()).toStringList();
	for (const QString &name : monitored)
		KNetStats::readInterfaceConfig(name, &mConfig[name]);
	refreshInterfaces();
	const auto matches = mInterfaces->findItems(selected, Qt::MatchExactly);
	if (!matches.isEmpty())
		mInterfaces->setCurrentItem(matches.first());
	QDialog::showEvent(event);
}

void Configure::refreshInterfaces() {
	storeCurrentOptions();
	const QString selected = mCurrentItem;
	QStringList available;
	const auto interfaces = QNetworkInterface::allInterfaces();
	for (const auto &interface : interfaces) {
		const QString name = interface.name();
		available.append(name);
		if (!mConfig.contains(name))
			KNetStats::readInterfaceConfig(name, &mConfig[name]);
	}

	QListWidgetItem *current = nullptr;
	{
		const QSignalBlocker blocker(mInterfaces);
		mInterfaces->clear();
		for (auto it = mConfig.constBegin(); it != mConfig.constEnd(); ++it) {
			auto *item = new QListWidgetItem(mInterfaceIcon, it.key(), mInterfaces);
			if (!available.contains(it.key())) {
				item->setIcon(QIcon(":/img/interfaces_missing.png"));
				item->setToolTip(tr("Interface currently unavailable"));
			}
			if (it.key() == selected)
				current = item;
		}
		if (!current)
			current = mInterfaces->item(0);
		mInterfaces->setCurrentItem(current);
	}
	mCurrentItem.clear();
	changeInterface(current);
}

void Configure::storeCurrentOptions() {
	if (mCurrentItem.isEmpty() || !mConfig.contains(mCurrentItem))
		return;
	ViewOptions &view = mConfig[mCurrentItem];
	view.mMonitoring = mMonitoringInterface->isChecked();
	view.mNotifications = mDisplayNotifications->isChecked();
	view.mUpdateInterval = mUpdateInterval->value();
	view.mTheme = mTheme->currentIndex();
	view.mChartUplColor = mChartUplColor->color().name();
	view.mChartDldColor = mChartDldColor->color().name();
	view.mChartBgColor = mChartBgColor->color().name();
	view.mChartTransparentBackground = mChartTransparentBackground->isChecked();
}

void Configure::changeInterface(QListWidgetItem *item) {
	storeCurrentOptions();
	mConfigurationGroup->setEnabled(item != nullptr);
	mAppearanceGroup->setEnabled(item != nullptr);
	if (!item) {
		mCurrentItem.clear();
		return;
	}
	const QString interface = item->text();
	if (interface == mCurrentItem)
		return;
	// Load the new interface options
	ViewOptions &view = mConfig[interface];
	// General options
	mMonitoringInterface->setChecked(view.mMonitoring);
	mDisplayNotifications->setChecked(view.mNotifications);
	mUpdateInterval->setValue(view.mUpdateInterval);
	mTheme->setCurrentIndex(view.mTheme);
	// Chart Options
	mChartUplColor->setColor(QColor(view.mChartUplColor));
	mChartDldColor->setColor(QColor(view.mChartDldColor));
	mChartBgColor->setColor(QColor(view.mChartBgColor));
	mChartTransparentBackground->setChecked(view.mChartTransparentBackground);
	mCurrentItem = interface;

	changeTheme(view.mTheme);
}

bool Configure::canSaveConfig() {
	// update the options
	storeCurrentOptions();

	bool ok = false;
	for (OptionsMap::ConstIterator i = mConfig.begin(); i != mConfig.end(); ++i)
		if (i.value().mMonitoring) {
			ok = true;
			break;
		}

	if (!ok) {
		QMessageBox msg(this);
		msg.setIcon(QMessageBox::Icon::Information);
		msg.setText("Error");
		msg.setInformativeText("You need to select at least one interface to monitor.");
		msg.exec();
	}

	return ok;
}

void Configure::changeTheme(int theme) {
	mIconError->setPixmap(QPixmap(":/img/theme" + QString::number(theme) + "_error.png"));
	mIconNone->setPixmap(QPixmap(":/img/theme" + QString::number(theme) + "_none.png"));
	mIconTx->setPixmap(QPixmap(":/img/theme" + QString::number(theme) + "_tx.png"));
	mIconRx->setPixmap(QPixmap(":/img/theme" + QString::number(theme) + "_rx.png"));
	mIconBoth->setPixmap(QPixmap(":/img/theme" + QString::number(theme) + "_both.png"));
}
