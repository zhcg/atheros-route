/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: netconfig.cpp
//  Description: Network configuration source
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
#include "netconfig.h"
#include "mainprocess.h"
#include "testbedsta.h"

#include <QMessageBox>

#include <time.h>


NetConfig::NetConfig(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedSta *>(wizard);
	setupUi(this);

	cmbAuth->addItem("Open");
	cmbAuth->addItem("WPA-PSK");
	cmbAuth->addItem("WPA2-PSK");

	connect(cmbAuth, SIGNAL(currentIndexChanged(int)), SLOT(selectAuth(int)));
	connect(cmbEncr, SIGNAL(currentIndexChanged(int)), SLOT(selectEncr(int)));
	connect(pbGenSsid, SIGNAL(clicked()), SLOT(generateSsid()));
	connect(pbGenNetKey, SIGNAL(clicked()), SLOT(generateNetKey()));

	cmbAuth->setCurrentIndex(1);
	cmbNetKeyIdx->addItem("1");
	cmbNetKeyIdx->addItem("2");
	cmbNetKeyIdx->addItem("3");
	cmbNetKeyIdx->addItem("4");

	srand(time(0));
}

NetConfig::~NetConfig()
{
}

bool NetConfig::pre_back()
{
	MainProcess::removeNetwork(MainProcess::getNetworkIndex());

	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

	leSsid->setFocus();
	return true;
}

bool NetConfig::pre_next()
{
	switch (MainProcess::getMode()) {
	case MainProcess::MODE_REG_CONFAP:
		pbGenSsid->setVisible(true);
		pbGenNetKey->setVisible(true);
		break;
	case MainProcess::MODE_MANUAL:
		pbGenSsid->setVisible(false);
		pbGenNetKey->setVisible(false);
		break;
	default:
		return false;
	}

	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

	leSsid->setText("");
	leNetKey->setText("");

	leSsid->setFocus();
	return true;
}

bool NetConfig::post_next()
{
	bool ret = false;
	char ssid[32 + 1], *key_mgmt = 0, *proto = 0,
		 *pairwise = 0, wep_key_idx[0x10], netKey[64 + 1];
	bool passphrase = false;

	do {
		if (!checkInputs()) {
			QMessageBox::critical(this, label->text(), "Input error");
			break;
		}

		MainProcess::setNetworkIndex(MainProcess::addNetwork());
		if (0 > MainProcess::getNetworkIndex()) {
			QMessageBox::warning(this, label->text(),
								 "Could not add a network.\n");
			break;
		}

		os_snprintf(ssid, sizeof(ssid), "%s", (const char *)leSsid->text().toAscii());
		ssid[leSsid->text().length()] = 0;
		if (!MainProcess::setNetworkParam(MainProcess::getNetworkIndex(), "ssid", ssid, true)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set parameter : ssid.\n");
			break;
		}

		switch (cmbAuth->currentIndex()) {
		case 0:
			key_mgmt = "NONE";
			if (0 != cmbEncr->currentIndex()) {
				os_snprintf(wep_key_idx, sizeof(wep_key_idx), "wep_key%d", cmbNetKeyIdx->currentText().toInt() - 1);
				os_snprintf(netKey, sizeof(netKey), "%s", (const char *)leNetKey->text().toAscii());
				netKey[leNetKey->text().length()] = 0;
				if ((5 == leNetKey->text().length()) ||
					(13 == leNetKey->text().length()))
					passphrase = true;
			}
			break;
		case 1:
		case 2:
			key_mgmt = "WPA-PSK";
			proto = (char *)((1 == cmbAuth->currentIndex())?"WPA":"RSN");
			pairwise = (char *)((0 == cmbEncr->currentIndex())?"TKIP":"CCMP");
			os_snprintf(netKey, sizeof(netKey), "%s", (const char *)leNetKey->text().toAscii());
			netKey[leNetKey->text().length()] = 0;
			if (64 > leNetKey->text().length())
				passphrase = true;
			break;
		default:
			break;
		}

		if (!MainProcess::setNetworkParam(MainProcess::getNetworkIndex(), "key_mgmt", key_mgmt, false)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set parameter : key_mgmt.\n");
			break;
		}

		if (proto && !MainProcess::setNetworkParam(MainProcess::getNetworkIndex(), "proto", proto, false)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set parameter : proto.\n");
			break;
		}

		if (pairwise && !MainProcess::setNetworkParam(MainProcess::getNetworkIndex(), "pairwise", pairwise, false)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set parameter : pairwise.\n");
			break;
		}

		if (proto && leNetKey->isEnabled() && !MainProcess::setNetworkParam(MainProcess::getNetworkIndex(), "psk", netKey, passphrase)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set parameter : psk.\n");
			break;
		} else if (!proto && leNetKey->isEnabled() && cmbNetKeyIdx->isEnabled()) {
			if (!MainProcess::setNetworkParam(MainProcess::getNetworkIndex(), wep_key_idx, netKey, passphrase)) {
				QMessageBox::warning(this, label->text(),
									 "Could not set parameter : wep_key.\n");
				break;
			}

			os_snprintf(wep_key_idx, sizeof(wep_key_idx), "%d", cmbNetKeyIdx->currentText().toInt() - 1);
			if (!MainProcess::setNetworkParam(MainProcess::getNetworkIndex(), "wep_tx_keyidx", wep_key_idx, false)) {
				QMessageBox::warning(this, label->text(),
									 "Could not set parameter : wep_tx_keyidx.\n");
				break;
			}
		}

		if (MainProcess::MODE_REG_CONFAP == MainProcess::getMode())
			(void)MainProcess::disableNetwork(MainProcess::getNetworkIndex());

		if (!MainProcess::setWpsConfiguration(MainProcess::getNetworkIndex())) {
			QMessageBox::warning(this, label->text(),
								 "Could not set WPS network configuration.\n");
			break;
		}

		if (MainProcess::MODE_MANUAL == MainProcess::getMode()) {
			if (!MainProcess::connectNetwork(MainProcess::getNetworkIndex())) {
				QMessageBox::warning(this, label->text(),
									 "Could not connect network.\n");
			}
		}

		ret = true;
	} while (0);

	if (!ret)
		MainProcess::removeNetwork(MainProcess::getNetworkIndex());

	return ret;
}

void NetConfig::cancel()
{
	wiz->close();
}

void NetConfig::selectAuth(int selected)
{
	cmbEncr->clear();
	switch (selected) {
	case 0:
		cmbEncr->addItem("None");
		cmbEncr->addItem("WEP");
		leNetKey->setEnabled(false);
		pbGenNetKey->setEnabled(false);
		break;
	case 1:
	case 2:
		cmbEncr->addItem("TKIP");
		cmbEncr->addItem("CCMP");
		leNetKey->setEnabled(true);
		cmbEncr->setCurrentIndex(selected - 1);
		pbGenNetKey->setEnabled(true);
		cmbNetKeyIdx->setEnabled(false);
		break;
	default:
		break;
	}
}

void NetConfig::selectEncr(int selected)
{
	switch (cmbAuth->currentIndex()) {
	case 0:	// Open
		switch (selected) {
		case 0:	// NONE
			leNetKey->setEnabled(false);
			pbGenNetKey->setEnabled(false);
			break;
		case 1:	// WEP
			leNetKey->setEnabled(true);
			pbGenNetKey->setEnabled(true);
			cmbNetKeyIdx->setEnabled(true);
			break;
		default:
			break;
		}
		break;
	case 1:
	case 2:
	default:
		break;
	}
}

void NetConfig::generateSsid()
{
	char ssid[32 + 1];

	for (int i = 0; i < 32; i++) {
		ssid[i] = btoa(rand() % 16);
	}
	ssid[32] = 0;
	leSsid->clear();
	leSsid->setText(ssid);
}

void NetConfig::generateNetKey()
{
	char netKey[64 + 1] = {0};
	int length = 0;

	switch (cmbAuth->currentIndex()) {
	case 0:	// Open
		switch (cmbEncr->currentIndex()) {
		case 0:	// NONE
			break;
		case 1:	// WEP
			length = 26;
			break;
		default:
			break;
		}
		break;
	case 1:	// WPA-PSK
	case 2:	// WPA2-PSK
		length = 64;
		break;
	default:
		break;
	}

	for (int i = 0; i < length; i++)
		netKey[i] = btoa(rand() % 16);
	netKey[length] = 0;
	leNetKey->clear();
	leNetKey->setText(netKey);
}

char NetConfig::btoa(int b, bool capital /* = true */ )
{
	if ((0 <= b) && (9 >= b)) {
		return b + '0';
	} else if ((0xA <= b) && (0xF >= b)) {
		return (b - 0xA) + (capital?'A':'a');
	} else {
		return '0';
	}
}

bool NetConfig::checkInputs()
{
	bool ret = false;
	int i;

	do {
		if (!leSsid->text().length() || (32 < leSsid->text().length())) {
			leSsid->setFocus();
			break;
		}

		if (0 != cmbAuth->currentIndex()) {
			/* WPA-PSK / WPA2-PSK */
			if (64 == leNetKey->text().length()) {
				for (i = 0; i < 64; i++) {
					if (!isxdigit(*(const char *)(leNetKey->text().data() + i)))
						break;
				}
				if (64 != i) {
					leNetKey->setFocus();
					break;
				}
			} else if (8 > leNetKey->text().length()) {
				leNetKey->setFocus();
				break;
			}
		} else if (0 != cmbEncr->currentIndex()) {
			/* WEP */
			if ((10 == leNetKey->text().length()) ||
				(26 == leNetKey->text().length())) {
				for (i = 0; i < leNetKey->text().length(); i++) {
					if (!isxdigit(*(const char *)(leNetKey->text().data() + i)))
						break;
				}
				if (leNetKey->text().length() != i) {
					leNetKey->setFocus();
					break;
				}
			} else if ((5 != leNetKey->text().length()) &&
					   (13 != leNetKey->text().length())) {
				leNetKey->setFocus();
				break;
			}
		}
		ret = true;
	} while (0);

	return ret;
}

