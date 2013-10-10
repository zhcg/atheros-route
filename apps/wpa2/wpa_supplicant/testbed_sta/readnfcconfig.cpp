/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: readnfcconfig.cpp
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
#include "readnfcconfig.h"
#include "testbedsta.h"
#include "mainprocess.h"

#include <QMessageBox>
#include <QTimer>


ReadNfcConfig::ReadNfcConfig(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedSta *>(wizard);
	setupUi(this);

	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), this, SLOT(increment()));
}

ReadNfcConfig::~ReadNfcConfig()
{
	delete timer;
}

bool ReadNfcConfig::pre_back()
{
	label->setText("NFC Config method");

	wiz->pbCancel->setText("&Cancel");
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);
	return true;
}

bool ReadNfcConfig::pre_next()
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
		if (!MainProcess::readNfcConfig()) {
			progressBar->setVisible(false);
			MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
			QMessageBox::warning(this, label->text(),
								 "Could not scan NFC token\n");
			break;
		} else {
			lblComment->setText("Touch NFC config token to be read network configuration\n");
			progressBar->setValue(0);
			progressBar->setVisible(true);
			timer->start(100);
		}

		ret = true;
	} while (0);

	return ret;
}

void ReadNfcConfig::cancel()
{
	if (!wiz->pbCancel->text().compare("&Retry")) {
		wiz->pbCancel->setText("&Cancel");

		progressBar->setFormat("Touch NFC token in 30 sec");
		progressBar->update();

		if (!MainProcess::readNfcConfig()) {
			progressBar->setVisible(false);
			MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
			QMessageBox::warning(this, label->text(),
								 "Could not scan NFC token\n");
			MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
			wiz->close();
		} else {
			lblComment->setText("Touch NFC config token to be read network configuration\n");
			progressBar->setValue(0);
			progressBar->setVisible(true);
			timer->start(100);
		}
	} else {
		MainProcess::cancelScanNfcToken();
		MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
		wiz->close();
	}
}

bool ReadNfcConfig::post_back()
{
	wiz->pbCancel->setText("&Cancel");
	MainProcess::cancelScanNfcToken();
	MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
	return true;
}

bool ReadNfcConfig::post_next()
{
	wiz->pbCancel->setText("&Cancel");
	MainProcess::cancelScanNfcToken();
	MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
	return true;
}

void ReadNfcConfig::processCtrlRequest(char *buf, size_t len)
{
	int priority;
	char req[BUFSIZ];
	char msg[BUFSIZ];

	if (MainProcess::getCtrlRequelst(buf, len, &priority, req, msg)) {
		if (!strcmp(req, CTRL_REQ_NFC_READ_TIMEOUT)) {
			timer->stop();

			wiz->pbCancel->setText("&Retry");
			progressBar->setFormat("Timeout");
			progressBar->update();
		} else if (!strcmp(req, CTRL_REQ_NFC_ADD_NEW_AP)) {
			int index;
			char ssid[33];

			timer->stop();

			index = atoi(msg + 1);

			if (0 <= index) {
				MainProcess::setNetworkIndex(index);
				if (!MainProcess::getNetworkParam(index, "ssid", ssid, true)) {
					QMessageBox::warning(this, label->text(),
										 "Could not get parameter : ssid\n");
				}
				MainProcess::connectNetwork(MainProcess::getNetworkIndex());

				label->setText("Complete");
				lblComment->setText("Complete to read network configuration on NFC token.\n\nSSID : ");
				lblComment->setText(lblComment->text() + ssid);
				wiz->pbBack->setEnabled(false);
				wiz->pbNext->setEnabled(true);
				wiz->pbNext->setFocus();
				progressBar->setVisible(false);
			} else {
				lblComment->setText("Fail to read network configuration on NFC token.\n"
									"Probably the token is NOT config token, \n"
									"or invalid format.\n");

				wiz->pbCancel->setText("&Retry");
				progressBar->setVisible(false);
			}
		}
	}
}

void ReadNfcConfig::receiveMsgs()
{
	char msg[BUFSIZ];
	size_t len;

	while (MainProcess::ctrlPending()) {
		len = sizeof(msg)  - 1;
		if (MainProcess::receive(msg, &len))
			processCtrlRequest(msg, len);
	}
	MainProcess::reconnectMonitor();
}

void ReadNfcConfig::increment()
{
	if (progressBar->value() < progressBar->maximum())
		progressBar->setValue(progressBar->value() + 1);
	else
		timer->stop();
}

