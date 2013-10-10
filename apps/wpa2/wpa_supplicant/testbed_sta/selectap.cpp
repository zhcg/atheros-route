/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: apscanlist.cpp
//  Description: Select Access-Point source
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//     * Neither the name of Sony Corporation nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************/

#include "pagetemplate.h"
#include "selectap.h"
#include "testbedsta.h"
#include "mainprocess.h"

#include <QMessageBox>
#include <QTimer>


SelectAp::SelectAp(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedSta *>(wizard);
	setupUi(this);

	timer = new QTimer;
	connect(pbUpdate, SIGNAL(clicked()), SLOT(update()));
	connect(timer, SIGNAL(timeout()), SLOT(progress()));

	connect(cmbSelArea, SIGNAL(currentIndexChanged(int)), SLOT(selectArea(int)));

	pbUpdate->setText("Update");
	progressBar->setVisible(false);

	preArea = -1;
	selectedArea = SELECT_AP_AREA_NONE;
}

SelectAp::~SelectAp()
{
	delete timer;
}

bool SelectAp::pre_back()
{
	if (MainProcess::MODE_REG_REGSTA != MainProcess::getMode()) {
		MainProcess::removeNetwork(MainProcess::getNetworkIndex());
	}

	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

	displayList();

	return true;
}

bool SelectAp::pre_next()
{
	bool ret = true;

	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

	if (preArea != MainProcess::getArea()) {
		switch (MainProcess::getArea()) {
		case MainProcess::AREA_BOTH_INBAND_UPNP:
			cmbSelArea->clear();
			cmbSelArea->addItem("In-band");
			cmbSelArea->addItem("UPnP");
			cmbSelArea->setEnabled(true);
			cmbSelArea->setFocus();
			break;
		case MainProcess::AREA_INBAND_ONLY:
			cmbSelArea->clear();
			cmbSelArea->addItem("In-band");
			cmbSelArea->setEnabled(false);
			pbUpdate->setFocus();
			break;
		case MainProcess::AREA_UPNP_ONLY:
			cmbSelArea->clear();
#ifndef CONFIG_NATIVE_WINDOWS
			cmbSelArea->addItem("Wired UPnP");
			cmbSelArea->addItem("Wireless UPnP");
			cmbSelArea->setEnabled(true);
#else // CONFIG_NATIVE_WINDOWS
			cmbSelArea->addItem("UPnP");
			cmbSelArea->setEnabled(false);
#endif // CONFIG_NATIVE_WINDOWS
			pbUpdate->setFocus();
			break;
		default:
			ret = false;
			break;
		}
		selectArea(0);

		if (ret) {
			preArea = MainProcess::getArea();
		}
	}

	return ret;
}

void SelectAp::cancel()
{
	reset();
	wiz->close();
}

bool SelectAp::post_back()
{
	reset();
	if (MainProcess::MODE_REG_REGSTA != MainProcess::getMode()) {
		MainProcess::setEnabledUpnp(false);
	}
	return true;
}

bool SelectAp::post_next()
{
	bool ret = false;

	do {
		reset();
		if (!apList->currentItem()) {
			QMessageBox::warning(this, label->text(),
								 "Select target AP first.\n");
			break;
		}

		if (SELECT_AP_AREA_INBAND == selectedArea) {
			MainProcess::setEnabledUpnp(false);

			MainProcess::setNetworkIndex(MainProcess::addNetwork());
			if (0 > MainProcess::getNetworkIndex()) {
				QMessageBox::warning(this, label->text(),
									 "Could not add a network.\n");
				break;
			}

			QTreeWidgetItem *item = apList->currentItem();
			if (!MainProcess::setNetworkParam(MainProcess::getNetworkIndex(), "ssid", (char *)((const char *)item->text(0).toAscii()), true)) {
				QMessageBox::warning(this, label->text(),
									 "Could not set parameter : ssid.\n");
				break;
			}

			if (!MainProcess::setNetworkParam(MainProcess::getNetworkIndex(), "bssid", (char *)((const char *)item->text(1).toAscii()), false)) {
				QMessageBox::warning(this, label->text(),
									 "Could not set parameter : bssid.\n");
				break;
			}
		} else {
			QTreeWidgetItem *item = apList->currentItem();
			MainProcess::setControlUrl(item->text(0).toAscii());
		}

		ret = true;
	} while (0);

	if (!ret)
		MainProcess::removeNetwork(MainProcess::getNetworkIndex());

	return ret;
}

void SelectAp::reset()
{
	pbUpdate->setText("Update");
	timer->stop();
	progressBar->setValue(0);
	progressBar->setVisible(false);

#ifndef CONFIG_NATIVE_WINDOWS
	if (MainProcess::AREA_INBAND_ONLY != MainProcess::getArea())
#else // CONFIG_NATIVE_WINDOWS
	if (MainProcess::AREA_BOTH_INBAND_UPNP == MainProcess::getArea())
#endif // CONFIG_NATIVE_WINDOWS
		cmbSelArea->setEnabled(true);
	apList->setEnabled(true);
	wiz->pbNext->setEnabled(true);
}

void SelectAp::setInbandScan()
{
	QStringList header;
	apList->setColumnCount(3);
	header << "SSID" << "BSSID" << "Chanel";
	apList->setHeaderLabels(header);
	apList->setSortingEnabled(true);
	apList->setColumnWidth(0, apList->width() / 3);
	apList->setColumnWidth(1, apList->width() / 3);
	apList->setColumnWidth(2, apList->width() / 3);

}

void SelectAp::setUpnpScan()
{
	QStringList header;
	apList->setColumnCount(1);
	header << "Control URL";
	apList->setHeaderLabels(header);
	apList->setSortingEnabled(true);

}

void SelectAp::displayList()
{
	if (!cmbSelArea->currentText().compare("In-band"))
		displayInbandScanList();
	else
		displayUpnpScanList();
}

void SelectAp::displayInbandScanList()
{
	char res[0x1000];
	size_t len = sizeof(res) - 1;
	bool first = true;

	if (!MainProcess::getScanResults(res, &len)) {
		QMessageBox::warning(this, label->text(),
							 "Could not get AP scan results.\n");
		return;
	}

	apList->clear();

	QStringList lines = QString(res).split(QChar('\n'));
	for (QStringList::Iterator it = lines.begin();
		 it != lines.end(); it++) {
		if (first) {
			first = false;
			continue;
		}

		QStringList cols = it->split(QChar('\t'));
		QString flags, ssid, bssid, freq, channel;

		flags = cols.count() > 3?cols[3]:"";
		if (flags.contains("WPS")) {
			ssid = cols.count() > 4?cols[4]:"";
			bssid = cols.count() > 0?cols[0]:"";
			freq = cols.count() > 1?cols[1]:"";
			if (freq.toULong() >= 5170) {
				channel.setNum((freq.toULong() - 5170) / 5 + 34);
			} else if (freq.toULong() >= 2412) {
				channel.setNum((freq.toULong() - 2412) / 5 + 1);
			} else
				channel = "";

			if (bssid.length()) {
				QStringList item;
				item << ssid << bssid << channel;
				new QTreeWidgetItem(apList, item);
			}
		}
	}
}

void SelectAp::displayUpnpScanList()
{
	char res[0x1000];
	size_t len = sizeof(res) - 1;
	bool first = true;

	if (!MainProcess::getUpnpScanResults(res, &len)) {
		QMessageBox::warning(this, label->text(),
							 "Could not get AP scan results.\n");
		return;
	}

	apList->clear();

	QStringList lines = QString(res).split(QChar('\n'));
	for (QStringList::Iterator it = lines.begin();
		 it != lines.end(); it++) {
		if (first) {
			first = false;
			continue;
		}

		QStringList cols = it->split(QChar('\t'));
		QString ctrl_url, udn, manufacturer,
				model_name, model_number, serial_number;

		ctrl_url = cols.count() > 0?cols[0]:"";
		udn = cols.count() > 1 ? cols[1] : "";
		manufacturer = cols.count() > 2 ? cols[2] : "";
		model_name = cols.count() > 3 ? cols[3] : "";
		model_number = cols.count() > 4 ? cols[3] : "";
		serial_number = cols.count() > 5 ? cols[4] : "";

		if (ctrl_url.length()) {
			QStringList item;
			item << ctrl_url;
			new QTreeWidgetItem(apList, item);
		}
	}
}

void SelectAp::update()
{
	if (!pbUpdate->text().compare("Update")) {
		if (!cmbSelArea->currentText().compare("In-band")) {
			if (!MainProcess::scanRequest()) {
				QMessageBox::warning(this, label->text(),
									 "Could not scan AP.\n");
				return;
			}
		} else {
			if (!MainProcess::upnpScanRequest()) {
				QMessageBox::warning(this, label->text(),
									 "Could not scan AP.\n");
				return;
			}
		}

		pbUpdate->setText("Stop");
		progressBar->setValue(0);
		progressBar->setVisible(true);
		timer->start(1000);

		cmbSelArea->setEnabled(false);
		apList->setEnabled(false);
		wiz->pbNext->setEnabled(false);
	} else {
		reset();
		displayList();
	}
}

void SelectAp::progress()
{

	if (progressBar->maximum() > progressBar->value()) {
		progressBar->setValue(progressBar->value() + 1);
	} else {
		reset();
		displayList();
	}
}

void SelectAp::selectArea(int selected)
{
	char *upnpInterface;

	if ((0 >= cmbSelArea->count()) ||
		(cmbSelArea->count() <= selected))
		return;

	if (!cmbSelArea->currentText().compare("In-band")) {
		setInbandScan();
		displayInbandScanList();
		selectedArea = SELECT_AP_AREA_INBAND;
	} else {
		if (MainProcess::isEnabledUpnp()) {
			if (((selectedArea != SELECT_AP_AREA_WIRED_UPNP) &&
				 (!cmbSelArea->currentText().compare("UPnP") ||
				  !cmbSelArea->currentText().compare("Wired UPnP"))) ||
				((selectedArea != SELECT_AP_AREA_WIRELESS_UPNP) &&
				 !cmbSelArea->currentText().compare("Wireless UPnP"))) {
				(void)MainProcess::setEnabledUpnp(false);
			}
		}

		if (!cmbSelArea->currentText().compare("UPnP") ||
			!cmbSelArea->currentText().compare("Wired UPnP")) {
			upnpInterface = MainProcess::getWiredInterface();
			selectedArea = SELECT_AP_AREA_WIRED_UPNP;
		} else if (!cmbSelArea->currentText().compare("Wireless UPnP")) {
			upnpInterface = MainProcess::getWirelessInterface();
			selectedArea = SELECT_AP_AREA_WIRELESS_UPNP;
		} else {
			QMessageBox::warning(this, label->text(),
								 "Unknown selected item in combobox.\n");
			return;
		}

		if (!MainProcess::isEnabledUpnp()) {
			if (!MainProcess::setUpnpInterface(upnpInterface)) {
				QMessageBox::warning(this, label->text(),
									 "Could not set UPnP interface.");
				cmbSelArea->setCurrentIndex(0);
				return;
			}

			if (!MainProcess::setEnabledUpnp(true)) {
					QMessageBox::warning(this, label->text(),
										 "Could not be enabled UPnP.");
					cmbSelArea->setCurrentIndex(0);
				return;
			}
		}

		setUpnpScan();
		displayUpnpScanList();
	}
}

