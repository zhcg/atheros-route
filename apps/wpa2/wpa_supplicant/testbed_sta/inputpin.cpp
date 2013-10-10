/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: inputpin.cpp
//  Description: Input PIN source
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
#include "inputpin.h"
#include "testbedsta.h"
#include "mainprocess.h"

#include <QMessageBox>
#include <QTimer>

#define WPS_IDENTITY_ENROLLEE	"WFA-SimpleConfig-Enrollee-1-0"
#define WPS_IDENTITY_REGISTRAR	"WFA-SimpleConfig-Registrar-1-0"


InputPin::InputPin(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedSta *>(wizard);
	setupUi(this);

	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), this, SLOT(increment()));
}

InputPin::~InputPin()
{
	delete timer;
}

bool InputPin::pre_back()
{
	wiz->pbCancel->setText("&Cancel");
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);
	label->setText("PIN method");
	label_2->setVisible(true);
	lePin->setVisible(true);
	lePin->setFocus();

	progressBar->setFormat("Touch NFC token in 30 sec");
	progressBar->update();

	if (MainProcess::MODE_REG_REGSTA == MainProcess::getMode())
		lblComment->setText("Input PIN of a target Station, \nthen push [Next] button.\n");
	else
		lblComment->setText("Input PIN of a target Access Point, \nthen push [Next] button.\n");

	MainProcess::connectMonitor(this, SLOT(receiveMsgs));
	if (!MainProcess::readNfcPassword(MainProcess::getNetworkIndex())) {
		progressBar->setVisible(false);
		MainProcess::disconnectMonitor(this, SLOT(receiveMsgs));
		QMessageBox::warning(this, label->text(),
							 "Could not scan NFC token\n");
	} else {
		lblComment->setText(lblComment->text() + "Or touch NFC token to be read device password\n");
		progressBar->setValue(0);
		progressBar->setVisible(true);
		timer->start(100);
	}

	return true;
}

bool InputPin::pre_next()
{
	wiz->pbCancel->setText("&Cancel");
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);
	lePin->setFocus();

	progressBar->setFormat("Touch NFC token in 30 sec");
	progressBar->update();

	label->setText("PIN method");
	label_2->setVisible(true);
	lePin->clear();
	lePin->setVisible(true);
	lePin->setEnabled(true);

	if (MainProcess::MODE_REG_REGSTA == MainProcess::getMode())
		lblComment->setText("Input PIN of a target Station, \nthen push [Next] button.\n");
	else
		lblComment->setText("Input PIN of a target Access Point, \nthen push [Next] button.\n");

	MainProcess::connectMonitor(this, SLOT(receiveMsgs()));
	if (!MainProcess::readNfcPassword(MainProcess::getNetworkIndex())) {
		progressBar->setVisible(false);
		MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
		QMessageBox::warning(this, label->text(),
							 "Could not scan NFC token\n");
	} else {
		lblComment->setText(lblComment->text() + "Or touch NFC token to be read device password\n");
		progressBar->setValue(0);
		progressBar->setVisible(true);
		timer->start(100);
	}

	return true;
}

void InputPin::cancel()
{
	if (!wiz->pbCancel->text().compare("&Retry")) {
		wiz->pbCancel->setText("&Cancel");

		progressBar->setFormat("Touch NFC token in 30 sec");
		progressBar->update();

		if (MainProcess::MODE_REG_REGSTA == MainProcess::getMode())
			lblComment->setText("Input PIN of a target Station, \nthen push [Next] button.\n");
		else
			lblComment->setText("Input PIN of a target Access Point, \nthen push [Next] button.\n");

		if (!MainProcess::readNfcPassword(MainProcess::getNetworkIndex())) {
			progressBar->setVisible(false);
			MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
			QMessageBox::warning(this, label->text(),
								 "Could not scan NFC token\n");
		} else {
			lblComment->setText(lblComment->text() + "Or touch NFC token to be read device password\n");
			progressBar->setValue(0);
			progressBar->setVisible(true);
			timer->start(100);
		}
	} else {
		MainProcess::cancelScanNfcToken();
		MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));

		if ((MainProcess::MODE_REG_REGSTA != MainProcess::getMode()) ||
			!wiz->pbBack->isEnabled()) {
			wiz->close();
		} else {
			label->setText("Canceled by user indication");
			lblComment->setText("Cancel WPS authentication.\n");
			timer->stop();
			wiz->pbBack->setEnabled(false);
			wiz->pbNext->setEnabled(true);
			wiz->pbNext->setFocus();
			progressBar->setVisible(false);
			label_2->setVisible(false);
			lePin->clear();
			lePin->setVisible(false);
		}
	}
}

bool InputPin::post_back()
{
	wiz->pbCancel->setText("&Cancel");
	MainProcess::cancelScanNfcToken();
	MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
	return true;
}

bool InputPin::post_next()
{
	bool ret = false;
	do {
		if ((MainProcess::MODE_REG_REGSTA == MainProcess::getMode()) &&
			!lePin->isVisible()) {
			ret = true;
			break;
		}

		if (lePin->isEnabled()) {
			if (!lePin->text().length()) {
				lePin->setFocus();
				if (MainProcess::MODE_REG_REGSTA == MainProcess::getMode())
					QMessageBox::warning(this, label->text(),
										 "Should input PIN of a target Station.\n");
				else
					QMessageBox::warning(this, label->text(),
										 "Should input PIN of a target Access Point.\n");
				break;
			}

			if (8 == lePin->text().length()) {
				if (!MainProcess::validatePIN(lePin->text().toAscii())) {
					if (QMessageBox::No ==
						QMessageBox::question(this, label->text(),
								 "PIN has invalidate checksum.\n"
								 "Do you really use this PIN?\n",
								 QMessageBox::Yes|QMessageBox::No)) {
						lePin->setFocus();
						break;
					}
				}
			}
		}

		if (MainProcess::isEnabledUpnp()) {
			if (lePin->isEnabled()) {
				if (!MainProcess::setWpsPassword(lePin->text().toAscii())) {
					QMessageBox::warning(this, label->text(),
										 "Could not set WPS password.\n");
					break;
				}
			}

			if (MainProcess::MODE_REG_CONFAP == MainProcess::getMode()) {
				if (!MainProcess::sendUpnpGetDevInfo(MainProcess::getControlUrl())) {
					QMessageBox::warning(this, label->text(),
										 "Could not send GetDeviceInfo thru UPnP.\n");
					break;
				}
			} else if (MainProcess::MODE_REG_REGSTA == MainProcess::getMode()) {
				if (!MainProcess::setWpsConfiguration(MainProcess::getNetworkIndex())) {
					QMessageBox::warning(this, label->text(),
										 "Could not set WPS network configuration.\n");
					break;
				}

				if (!MainProcess::sendSelectedRegistrar(MainProcess::getControlUrl(), true)) {
					QMessageBox::warning(this, label->text(),
										 "Could not send SelectedRegistrar thru UPnP.\n");
					break;
				}
			}
		} else if (0 <= MainProcess::getNetworkIndex()) {
			int index = MainProcess::getNetworkIndex();
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

			if (!MainProcess::setNetworkParam(index, "identity", WPS_IDENTITY_REGISTRAR, true)) {
				QMessageBox::warning(this, label->text(),
									 "Could not set parameter : eap.\n");
				break;
			}

			if (!lePin->isEnabled()) {
				MainProcess::setNetworkParam(index, "password", "", true);
			} else {
				if (!MainProcess::setNetworkParam(index, "password", 
												 (char *)((const char *)lePin->text().toAscii()), true)) {
					QMessageBox::warning(this, label->text(),
										 "Could not set parameter : password.\n");
					break;
				}
			}
		}

		wiz->pbCancel->setText("&Cancel");
		MainProcess::cancelScanNfcToken();
		MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));

		ret = true;
	} while (0);

	return ret;
}

void InputPin::processCtrlRequest(char *buf, size_t len)
{
	int priority;
	char req[BUFSIZ];
	char msg[BUFSIZ];

	if (MainProcess::getCtrlRequelst(buf, len, &priority, req, msg)) {
		if (!strcmp(req, CTRL_REQ_NFC_READ_TIMEOUT)) {
			wiz->pbCancel->setText("&Retry");
			progressBar->setFormat("Timeout");
			progressBar->update();
		} else if (!strcmp(req, CTRL_REQ_NFC_FAIL_READ)) {
			timer->stop();
			lblComment->setText("Fail to read network configuration on NFC token.\n"
								"Probably the token is NOT config token, \n"
								"or invalid format.\n");

			wiz->pbCancel->setText("&Retry");
			progressBar->setVisible(false);
		} else if (!strcmp(req, CTRL_REQ_NFC_COMP_READ)) {
			lblComment->setText("Complete to read device password on NFC token.\n");
			lePin->setText("Read on NFC Token.");
			lePin->setEnabled(false);

			timer->stop();
			progressBar->setVisible(false);
			wiz->next();
		}
	}
}

void InputPin::receiveMsgs()
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

void InputPin::increment()
{
	if (progressBar->value() < progressBar->maximum())
		progressBar->setValue(progressBar->value() + 1);
	else
		timer->stop();
}

