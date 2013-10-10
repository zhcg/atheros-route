/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: displaypin.cpp
//  Description: Diplay PIN source
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
#include "displaypin.h"
#include "testbedsta.h"
#include "mainprocess.h"

#include <QMessageBox>
#include <QTimer>

#define WPS_IDENTITY_ENROLLEE	"WFA-SimpleConfig-Enrollee-1-0"
#define WPS_IDENTITY_REGISTRAR	"WFA-SimpleConfig-Registrar-1-0"


DisplayPin::DisplayPin(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedSta *>(wizard);
	setupUi(this);

	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), this, SLOT(increment()));
}

DisplayPin::~DisplayPin()
{
	delete timer;
}

bool DisplayPin::pre_back()
{
	wiz->pbCancel->setText("&Cancel");
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

	progressBar->setFormat("Touch NFC token in 30 sec");
	progressBar->update();

	lblComment->setText("Input the following PIN to registrar, then push [Next] button.\n");

	(void)MainProcess::clearWpsPassword();

	MainProcess::connectMonitor(this, SLOT(receiveMsgs));
	if (!MainProcess::writeNfcPassword(MainProcess::getNetworkIndex())) {
		progressBar->setVisible(false);
		MainProcess::disconnectMonitor(this, SLOT(receiveMsgs));
		QMessageBox::warning(this, label->text(),
							 "Could not scan NFC token\n");
	} else {
		lblComment->setText(lblComment->text() + "Or touch NFC token to be written device password\n");
		progressBar->setValue(0);
		progressBar->setVisible(true);
		timer->start(100);
	}

	return true;
}

bool DisplayPin::pre_next()
{
	char pin[9];

	wiz->pbCancel->setText("&Cancel");
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

	progressBar->setFormat("Touch NFC token in 30 sec");
	progressBar->update();

	MainProcess::generatePIN(pin);
	lblPIN->setText(pin);

	lblComment->setText("Input the following PIN to registrar, then push [Next] button.\n");

	(void)MainProcess::clearWpsPassword();

	MainProcess::connectMonitor(this, SLOT(receiveMsgs()));
	if (!MainProcess::writeNfcPassword(MainProcess::getNetworkIndex())) {
		progressBar->setVisible(false);
		MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
		QMessageBox::warning(this, label->text(),
							 "Could not scan NFC token\n");
	} else {
		lblComment->setText(lblComment->text() + "Or touch NFC token to be written device password\n");
		progressBar->setValue(0);
		progressBar->setVisible(true);
		timer->start(100);
	}

	return true;
}

void DisplayPin::cancel()
{
	if (!wiz->pbCancel->text().compare("&Retry")) {
		wiz->pbCancel->setText("&Cancel");

		progressBar->setFormat("Touch NFC token in 30 sec");
		progressBar->update();

		lblComment->setText("Input the following PIN to registrar, then push [Next] button.\n");

		if (!MainProcess::writeNfcPassword(MainProcess::getNetworkIndex())) {
			progressBar->setVisible(false);
			MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
			QMessageBox::warning(this, label->text(),
								 "Could not scan NFC token\n");
		} else {
			lblComment->setText(lblComment->text() + "Or touch NFC token to be written device password\n");
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

bool DisplayPin::post_back()
{
	wiz->pbCancel->setText("&Cancel");
	MainProcess::cancelScanNfcToken();
	MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
	return true;
}

bool DisplayPin::post_next()
{
	bool ret = false;
	int index;


	do {
		index = MainProcess::getNetworkIndex();

		if (!MainProcess::setNetworkParam(index, "key_mgmt", "IEEE8021X", false)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set parameter : key_mgmt.\n");
			break;
		}

		if (!MainProcess::setNetworkParam(index, "eap", "WPS", false)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set parameter : eap.\n");
			break;
		}

		if (!MainProcess::setNetworkParam(index, "identity", WPS_IDENTITY_ENROLLEE, true)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set parameter : eap.\n");
			break;
		}

		if (!lblPIN->text().compare("Written on NFC Token.")) {
			MainProcess::setNetworkParam(index, "password", "", true);
		} else {
			if (!MainProcess::setNetworkParam(index, "password", 
											 (char *)((const char *)lblPIN->text().toAscii()), true)) {
				QMessageBox::warning(this, label->text(),
									 "Could not set parameter : password.\n");
			}
		}

		wiz->pbCancel->setText("&Cancel");
		MainProcess::cancelScanNfcToken();
		MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));

		ret = true;
	} while (0);

	return ret;
}

void DisplayPin::processCtrlRequest(char *buf, size_t len)
{
	int priority;
	char req[BUFSIZ];
	char msg[BUFSIZ];

	if (MainProcess::getCtrlRequelst(buf, len, &priority, req, msg)) {
		if (!strcmp(req, CTRL_REQ_NFC_WRITE_TIMEOUT)) {
			wiz->pbCancel->setText("&Retry");
			progressBar->setFormat("Timeout");
			progressBar->update();
		} else if (!strcmp(req, CTRL_REQ_NFC_COMP_WRITE)) {
			lblComment->setText("Complete to write device password on NFC token.\n"
								"Touch a target registrar with the token, then push [Next] button.\n");
			lblPIN->setText("Written on NFC Token.");

			timer->stop();
			progressBar->setVisible(false);
		}
	}
}

void DisplayPin::receiveMsgs()
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

void DisplayPin::increment()
{
	if (progressBar->value() < progressBar->maximum())
		progressBar->setValue(progressBar->value() + 1);
	else
		timer->stop();
}

