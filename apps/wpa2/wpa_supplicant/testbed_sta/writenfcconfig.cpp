/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: writenfcconfig.cpp
//  Description: Read NFC config token source
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
#include "writenfcconfig.h"
#include "testbedsta.h"
#include "mainprocess.h"

#include <QMessageBox>
#include <QTimer>


WriteNfcConfig::WriteNfcConfig(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedSta *>(wizard);
	setupUi(this);

	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), this, SLOT(increment()));
}

WriteNfcConfig::~WriteNfcConfig()
{
	delete timer;
}

bool WriteNfcConfig::pre_back()
{
	label->setText("NFC Config method");

	wiz->pbCancel->setText("&Cancel");
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);
	return true;
}

bool WriteNfcConfig::pre_next()
{
	bool ret = false;

	label->setText("NFC Config method");

	wiz->pbCancel->setText("&Cancel");
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(false);
	wiz->pbCancel->setEnabled(true);

	do {
		progressBar->setFormat("Touch NFC token in 30 sec");
		progressBar->update();

		MainProcess::connectMonitor(this, SLOT(receiveMsgs()));
		if (!MainProcess::writeNfcConfig(MainProcess::getNetworkIndex())) {
			progressBar->setVisible(false);
			MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
			QMessageBox::warning(this, label->text(),
								 "Could not scan NFC token\n");
			break;
		} else {
			lblComment->setText("Touch NFC config token to be written network configuration\n");
			progressBar->setValue(0);
			progressBar->setVisible(true);
			timer->start(100);
		}

		ret = true;
	} while (0);

	return ret;
}

void WriteNfcConfig::cancel()
{
	if (!wiz->pbCancel->text().compare("&Retry")) {
		wiz->pbCancel->setText("&Cancel");

		progressBar->setFormat("Touch NFC token in 30 sec");
		progressBar->update();

		if (!MainProcess::writeNfcConfig(MainProcess::getNetworkIndex())) {
			progressBar->setVisible(false);
			MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
			QMessageBox::warning(this, label->text(),
								 "Could not scan NFC token\n");
			if (MainProcess::MODE_REG_REGSTA != MainProcess::getMode()) {
				MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
				wiz->close();
			} else {
				wiz->pbNext->setEnabled(true);
			}
		} else {
			lblComment->setText("Touch NFC config token to be written network configuration\n");
			progressBar->setValue(0);
			progressBar->setVisible(true);
			timer->start(100);
		}
	} else {
		if (wiz->pbNext->isEnabled() ||
			(MainProcess::MODE_REG_REGSTA != MainProcess::getMode())) {
			MainProcess::cancelScanNfcToken();
			MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
			wiz->close();
		} else {
			label->setText("Canceled by user indication");
			lblComment->setText("Cancel to write network configuration on NFC token.\n");
			timer->stop();
			wiz->pbBack->setEnabled(false);
			wiz->pbNext->setEnabled(true);
			wiz->pbNext->setFocus();
			progressBar->setVisible(false);
		}
	}
}

bool WriteNfcConfig::post_back()
{
	wiz->pbCancel->setText("&Cancel");
	MainProcess::cancelScanNfcToken();
	MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
	return true;
}

bool WriteNfcConfig::post_next()
{
	if (MainProcess::MODE_REG_REGSTA != MainProcess::getMode()) {
		MainProcess::connectNetwork(MainProcess::getNetworkIndex());
	}

	wiz->pbCancel->setText("&Cancel");
	MainProcess::cancelScanNfcToken();
	MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
	return true;
}

void WriteNfcConfig::processCtrlRequest(char *buf, size_t len)
{
	int priority;
	char req[BUFSIZ];
	char msg[BUFSIZ];

	if (MainProcess::getCtrlRequelst(buf, len, &priority, req, msg)) {
		if (!strcmp(req, CTRL_REQ_NFC_WRITE_TIMEOUT)) {
			timer->stop();

			wiz->pbCancel->setText("&Retry");
			progressBar->setFormat("Timeout");
			progressBar->update();
		} else if (!strcmp(req, CTRL_REQ_NFC_COMP_WRITE)) {
			char ssid[33];

			timer->stop();

			if (!MainProcess::getNetworkParam(MainProcess::getNetworkIndex(), "ssid", ssid, true)) {
				QMessageBox::warning(this, label->text(),
									 "Could not get parameter : ssid\n");
			}

			label->setText("Complete");
			lblComment->setText("Complete to write network configuration on NFC token.\n\nSSID : ");
			lblComment->setText(lblComment->text() + ssid);
			if (MainProcess::MODE_REG_REGSTA == MainProcess::getMode())
				lblComment->setText(lblComment->text() +
									"\n\nTouch a target Station with the token, \nthen push [Next] button.\n");
			else
				lblComment->setText(lblComment->text() +
									"\n\nTouch a target Access Point with the token, \nthen push [Next] button.\n");
			wiz->pbBack->setEnabled(false);
			wiz->pbNext->setEnabled(true);
			wiz->pbNext->setFocus();
			progressBar->setVisible(false);
		}
	}
}

void WriteNfcConfig::receiveMsgs()
{
	char msg[BUFSIZ];
	size_t len;

	while (MainProcess::ctrlPending()) {
		len = sizeof(msg)  - 1;
		if (MainProcess::receive(msg, &len)) {
			processCtrlRequest(msg, len);
		}
	}
	MainProcess::reconnectMonitor();
}

void WriteNfcConfig::increment()
{
	if (progressBar->value() < progressBar->maximum())
		progressBar->setValue(progressBar->value() + 1);
	else
		timer->stop();
}

