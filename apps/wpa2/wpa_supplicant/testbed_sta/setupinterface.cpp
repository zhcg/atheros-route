/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: setupinterface.cpp
//  Description: setup interface source
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
#include "setupinterface.h"
#include "mainprocess.h"
#include "testbedsta.h"

#include <QMessageBox>
#include <QProcess>
#include <QFile>


SetupInterface::SetupInterface(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
#ifndef CONFIG_NATIVE_WINDOWS
#define CONFIG_FILE "./testbed_sta.conf"
#define DEFAULT_WIRELESS_INTERFACE "ath0"
#define DEFAULT_WIRELESS_DRIVER "madwifi"
#define DEFAULT_IPADDRESS "192.168.1.2"
#define DEFAULT_NFC_INTERFACE "/dev/ttyUSB0"
#define DEFAULT_WIRED_INTERFACE "eth0"
#define DEFAULT_IPADDRESS2 "192.168.0.2"
#define DEFAULT_NETMASK "255.255.255.0"
#else // CONFIG_NATIVE_WINDOWS
#define CONFIG_FILE ".\\testbed_sta.conf"
#define DEFAULT_WIRELESS_INTERFACE ""
#define DEFAULT_WIRELESS_DRIVER "ndis"
#define DEFAULT_IPADDRESS "192.168.1.2"
#define DEFAULT_NFC_INTERFACE "USB0"
#define DEFAULT_WIRED_INTERFACE "eth0"
#define DEFAULT_IPADDRESS2 "192.168.0.2"
#define DEFAULT_NETMASK "255.255.255.0"
#endif // CONFIG_NATIVE_WINDOWS
	QFile *conf = new QFile(CONFIG_FILE);
	bool read_file = false;
	int index;

	setupUi(this);

	wiz = reinterpret_cast<TestbedSta *>(wizard);

	QRegExp rx("[0-9]\\d{0,2}\\.[0-9]\\d{0,2}\\.[0-9]\\d{0,2}\\.[0-9]\\d{0,2}");
	validator = new QRegExpValidator(rx, this);
	leIPAddress->setValidator(validator);
	validator2 = new QRegExpValidator(rx, this);
	leIPAddress2->setValidator(validator2);
	validator3 = new QRegExpValidator(rx, this);
	leNetMask->setValidator(validator3);
	validator4 = new QRegExpValidator(rx, this);
	leNetMask2->setValidator(validator4);

	if (conf && conf->exists() && conf->open(QIODevice::ReadOnly)) {
		char line[BUFSIZ], *tmp, *tmp2;

		index = 0;
		while((5 >= index) && !conf->atEnd()) {
			if (0 > conf->readLine(line, sizeof(line) - 1))
				break;

			if (0 != (tmp = strchr(line, '\n')))
				*tmp = 0;

			switch (index++) {
			case 0: leWInterface->setText(line); break;
			case 1: leWDriver->setText(line); break;
			case 2:
			{
				if (0 != (tmp = strchr(line, ','))) {
					*tmp = 0;
					tmp++;
					if (0 != (tmp2 = strchr(tmp, ','))) {
						*tmp2 = 0;
						*tmp2++;
					}
					cbIPAddress->setChecked(atoi(line));
					leIPAddress->setText(tmp);
					if (tmp2)
						leNetMask->setText(tmp2);
					else
						leNetMask->setText(DEFAULT_NETMASK);
					leIPAddress->setEnabled(cbIPAddress->isChecked());
					leNetMask->setEnabled(cbIPAddress->isChecked());
				}
				break;
			}
			case 3:
			{
				if (0 != (tmp = strchr(line, ','))) {
					*tmp = 0;
					tmp++;
					cbNFCInterface->setChecked(atoi(line));
					leNFCInterface->setText(tmp);
				}
				break;
			}
			case 4: leInterface2->setText(line); break;
			case 5:
			{
				if (0 != (tmp = strchr(line, ','))) {
					*tmp = 0;
					tmp++;
					if (0 != (tmp2 = strchr(tmp, ','))) {
						*tmp2 = 0;
						*tmp2++;
					}
					cbIPAddress2->setChecked(atoi(line));
					leIPAddress2->setText(tmp);
					if (tmp2)
						leNetMask2->setText(tmp2);
					else
						leNetMask2->setText(DEFAULT_NETMASK);
					leIPAddress2->setEnabled(cbIPAddress2->isChecked());
					leNetMask2->setEnabled(cbIPAddress2->isChecked());
				}
				break;
			}
			}
		}
		conf->close();

		if (5 < index)
			read_file = true;
	}

	if (conf)
		delete conf;

	if (!read_file) {
		leWInterface->setText(DEFAULT_WIRELESS_INTERFACE);
		leWDriver->setText(DEFAULT_WIRELESS_DRIVER);
		cbIPAddress->setChecked(true);
		leIPAddress->setText(DEFAULT_IPADDRESS);
		leNetMask->setText(DEFAULT_NETMASK);
		cbNFCInterface->setChecked(true);
		leNFCInterface->setText(DEFAULT_NFC_INTERFACE);
		leInterface2->setText(DEFAULT_WIRED_INTERFACE);
		cbIPAddress2->setChecked(true);
		leIPAddress2->setText(DEFAULT_IPADDRESS2);
		leNetMask2->setText(DEFAULT_NETMASK);
	}

#ifndef CONFIG_NATIVE_WINDOWS
	connect(cbIPAddress, SIGNAL(clicked()), SLOT(enabledDhcp()));
	connect(cbIPAddress2, SIGNAL(clicked()), SLOT(enabledDhcp2()));
#else // CONFIG_NATIVE_WINDOWS
	leWDriver->setText(DEFAULT_WIRELESS_DRIVER);

	leWInterface->setVisible(false);
	cbIPAddress->setVisible(false);
	leIPAddress->setVisible(false);
	label_4->setVisible(false);
	leNetMask->setVisible(false);
	label_5->setVisible(false);
	leInterface2->setVisible(false);
	cbIPAddress2->setVisible(false);
	leIPAddress2->setVisible(false);
	label_6->setVisible(false);
	leNetMask2->setVisible(false);

	MainProcess::setWirelessInterface(leWInterface->text().toAscii());

	if_list = 0;
	cmbWInterface = new QComboBox(gridLayout);
	cmbWInterface->setObjectName(QString::fromUtf8("cmbWInterface"));
	gridLayout1->addWidget(cmbWInterface, 0, 1, 1, 1);

	connect(cmbWInterface, SIGNAL(currentIndexChanged(int)), SLOT(selectWInterface(int)));
#endif // CONFIG_NATIVE_WINDOWS

#undef CONFIG_FILE
#undef DEFAULT_WIRELESS_INTERFACE
#undef DEFAULT_WIRELESS_DRIVER
#undef DEFAULT_IPADDRESS
#undef DEFAULT_NFC_INTERFACE
#undef DEFAULT_WIRED_INTERFACE
#undef DEFAULT_IPADDRESS2
#undef DEFAULT_NETMASK
}

SetupInterface::~SetupInterface()
{
	(void)end();

	disconnect(cbIPAddress);

	if (validator)
		delete validator;
	if (validator2)
		delete validator2;
	if (validator3)
		delete validator3;
	if (validator4)
		delete validator4;

#ifdef CONFIG_NATIVE_WINDOWS
	disconnect(cmbWInterface);
	delete cmbWInterface;

	win_if_list_free(&if_list);
#endif // CONFIG_NATIVE_WINDOWS
}

bool SetupInterface::pre_back()
{
	MainProcess::terminate();
	wiz->pbBack->setEnabled(false);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

#ifdef CONFIG_NATIVE_WINDOWS
	displayWInterfaceList();
#endif // CONFIG_NATIVE_WINDOWS
	return true;
}

bool SetupInterface::pre_next()
{
	wiz->pbBack->setEnabled(false);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

#ifdef CONFIG_NATIVE_WINDOWS
	displayWInterfaceList();
#endif // CONFIG_NATIVE_WINDOWS
	return true;
}

bool SetupInterface::post_next()
{
#ifndef CONFIG_NATIVE_WINDOWS
#define STA_START	"./sta_start"
#define CONFIG_FILE "./testbed_sta.conf"
#else // CONFIG_NATIVE_WINDOWS
#define CONFIG_FILE ".\\testbed_sta.conf"
#endif // CONFIG_NATIVE_WINDOWS

	bool ret = false;
	QProcess *prc = new QProcess(this);
#ifndef CONFIG_NATIVE_WINDOWS
	char cmd[BUFSIZ];
#endif // CONFIG_NATIVE_WINDOWS
	QFile *conf = new QFile(CONFIG_FILE);
	char line[BUFSIZ];

	do {
		if (!checkInputs()) {
			QMessageBox::critical(this, label->text(), "Input error");
			break;
		}

#ifndef CONFIG_NATIVE_WINDOWS
		snprintf(cmd, sizeof(cmd), "%s %s %s %s %s %s %s",
			STA_START,
			(const char *)leWInterface->text().toAscii(),
			cbIPAddress->isChecked()?
				(const char *)leIPAddress->text().toAscii():"dynamic",
			cbIPAddress->isChecked()?
				(const char *)leNetMask->text().toAscii():"255.255.255.0",
			(const char *)leInterface2->text().toAscii(),
			cbIPAddress2->isChecked()?
				(const char *)leIPAddress2->text().toAscii():"dynamic",
			cbIPAddress2->isChecked()?
				(const char *)leNetMask2->text().toAscii():"255.255.255.0");
		prc->start(cmd);
		prc->waitForFinished(-1);
		if(prc->exitCode()) {
			QMessageBox::critical(this, label->text(), "Set interface error");
			break;
		}
#endif // CONFIG_NATIVE_WINDOWS

		if (!MainProcess::start(leWInterface->text().toAscii(),
								leWDriver->text().toAscii(),
								cbNFCInterface->isChecked()?
									(const char *)leNFCInterface->text().toAscii():0)) {
			QMessageBox::critical(this, label->text(), "Cannot start main process");
			break;
		}

		if (conf && conf->open(QIODevice::WriteOnly)) {
			os_snprintf(line, sizeof(line), "%s\n",
						(const char *)leWInterface->text().toAscii());
			conf->write(line, strlen(line));
			os_snprintf(line, sizeof(line), "%s\n",
						(const char *)leWDriver->text().toAscii());
			conf->write(line, strlen(line));
			os_snprintf(line, sizeof(line), "%d,%s,%s\n",
						cbIPAddress->isChecked()?1:0,
						(const char *)leIPAddress->text().toAscii(),
						(const char *)leNetMask->text().toAscii());
			conf->write(line, strlen(line));
			os_snprintf(line, sizeof(line), "%d,%s\n",
						cbNFCInterface->isChecked()?1:0,
						(const char *)leNFCInterface->text().toAscii());
			conf->write(line, strlen(line));
			os_snprintf(line, sizeof(line), "%s\n",
						(const char *)leInterface2->text().toAscii());
			conf->write(line, strlen(line));
			os_snprintf(line, sizeof(line), "%d,%s,%s\n",
						cbIPAddress2->isChecked()?1:0,
						(const char *)leIPAddress2->text().toAscii(),
						(const char *)leNetMask2->text().toAscii());
			conf->write(line, strlen(line));

			conf->close();
		}
		if (conf)
			delete conf;

		MainProcess::setWirelessInterface(leWInterface->text().toAscii());
		MainProcess::setWiredInterface(leInterface2->text().toAscii());

		ret = true;
	} while (0);

	if (prc) {
		delete prc;
		prc = 0;
	}

	return ret;
#ifndef CONFIG_NATIVE_WINDOWS
#undef STA_START
#endif // CONFIG_NATIVE_WINDOWS
#undef CONFIG_FILE
}

bool SetupInterface::checkInputs()
{
	bool ret = false;
	QString check = leIPAddress->text();
	QString check2 = leIPAddress2->text();
	QString check3 = leNetMask->text();
	QString check4 = leNetMask2->text();
	int pos = 0;

	do {
		if (!leWInterface->text().length()) {
			leWInterface->setFocus();
			break;
		}

		if (cbIPAddress->isChecked() && 
			(QValidator::Acceptable != validator->validate(check, pos))) {
			leIPAddress->setFocus();
			break;
		}

		if (cbIPAddress->isChecked() && 
			(QValidator::Acceptable != validator3->validate(check3, pos))) {
			leNetMask->setFocus();
			break;
		}

		if (cbNFCInterface->isChecked() &&
			!leNFCInterface->text().length()) {
			leNFCInterface->setFocus();
			break;
		}

		if (!leInterface2->text().length()) {
			leInterface2->setFocus();
			break;
		}

		if (cbIPAddress2->isChecked() && 
			(QValidator::Acceptable != validator2->validate(check2, pos))) {
			leIPAddress2->setFocus();
			break;
		}

		if (cbIPAddress2->isChecked() && 
			(QValidator::Acceptable != validator4->validate(check4, pos))) {
			leNetMask2->setFocus();
			break;
		}

		ret = true;
	} while(0);
	return ret;
}

void SetupInterface::cancel()
{
	wiz->close();
}

void SetupInterface::enabledDhcp()
{
	leIPAddress->setEnabled(cbIPAddress->isChecked());
	leNetMask->setEnabled(cbIPAddress->isChecked());
}

void SetupInterface::enabledDhcp2()
{
	leIPAddress2->setEnabled(cbIPAddress2->isChecked());
	leNetMask2->setEnabled(cbIPAddress2->isChecked());
}

const char *SetupInterface::getWirelessInterface()
{
	return (const char *)leWInterface->text().toAscii();
}

const char *SetupInterface::getWiredInterface()
{
	return (const char *)leInterface2->text().toAscii();
}

bool SetupInterface::end()
{
#ifndef CONFIG_NATIVE_WINDOWS
#define STA_END	"./sta_end"
	bool ret = false;
	QProcess *prc = new QProcess(this);
	char cmd[BUFSIZ];

	do {
		if (!checkInputs()) {
			break;
		}

		snprintf(cmd, sizeof(cmd), "%s %s %s",
			STA_END,
			(const char *)leWInterface->text().toAscii(),
			(const char *)leInterface2->text().toAscii());
		prc->start(cmd);
		prc->waitForFinished(-1);

		ret = true;
	} while (0);

	if (prc) {
		delete prc;
		prc = 0;
	}

	return ret;
#undef STA_END
#else // CONFIG_NATIVE_WINDOWS
	return true;
#endif // CONFIG_NATIVE_WINDOWS
}

#ifdef CONFIG_NATIVE_WINDOWS
void SetupInterface::displayWInterfaceList()
{
	int selected = 0, index;
	win_if_t *next;

	win_if_list_free(&if_list);
	cmbWInterface->clear();

	do {
		if (win_if_enum_devs(&if_list))
			break;

		for (index = 0, next = if_list; next; next = next->next, index++) {
			cmbWInterface->addItem(next->description);
			if (!os_strcmp(MainProcess::getWirelessInterface(), next->name))
				selected = index;
		}

		if (selected != 0)
			cmbWInterface->setCurrentIndex(selected);
	} while (0);
}

void SetupInterface::selectWInterface(int index)
{
	win_if_t *item, *next;

	next = if_list;
	for (int i = 0; i < index && next; i++, next = next->next);

	item = next;
	if (item)
		leWInterface->setText(item->name);
}
#endif // CONFIG_NATIVE_WINDOWS
