/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: selectmode.cpp
//  Description: Select mode source
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
#include "selectmode.h"
#include "mainprocess.h"
#include "testbedsta.h"

#include <QMessageBox>


SelectMode::SelectMode(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedSta *>(wizard);
	setupUi(this);

	connect(pbConfigAp, SIGNAL(clicked()), SLOT(selConfigAp())); 
	connect(pbRegAp, SIGNAL(clicked()), SLOT(selRegAp())); 
	connect(pbRegSta, SIGNAL(clicked()), SLOT(selRegSta())); 
	connect(pbGetConfig, SIGNAL(clicked()), SLOT(selGetConfig())); 
	connect(pbSetConfig, SIGNAL(clicked()), SLOT(selSetConfig())); 
}

SelectMode::~SelectMode()
{
}

bool SelectMode::pre_back()
{
	MainProcess::setMode(MainProcess::MODE_ENR);

	if (0 > MainProcess::getNetworkIndex()) {
		pbConfigAp->setVisible(true);
		lblConfigAp->setVisible(true);
		pbRegAp->setVisible(false);
		lblRegAp->setVisible(false);
		pbRegSta->setVisible(false);
		lblRegSta->setVisible(false);
		pbGetConfig->setVisible(true);
		lblGetConfig->setVisible(true);
		pbSetConfig->setVisible(true);
		lblSetConfig->setVisible(true);
		pbConfigAp->setFocus();
		wiz->pbBack->setEnabled(true);
	} else {
		pbConfigAp->setVisible(false);
		lblConfigAp->setVisible(false);
		pbRegAp->setVisible(false);
		lblRegAp->setVisible(false);
		pbRegSta->setVisible(true);
		lblRegSta->setVisible(true);
		pbGetConfig->setVisible(false);
		lblGetConfig->setVisible(false);
		pbSetConfig->setVisible(false);
		lblSetConfig->setVisible(false);
		pbRegSta->setFocus();
		wiz->pbBack->setEnabled(false);
	}

	wiz->pbNext->setEnabled(false);
	wiz->pbCancel->setEnabled(true);
	return true;
}

bool SelectMode::pre_next()
{
	MainProcess::setMode(MainProcess::MODE_ENR);

	if (0 > MainProcess::getNetworkIndex()) {
		pbConfigAp->setVisible(true);
		lblConfigAp->setVisible(true);
		pbRegAp->setVisible(false);
		lblRegAp->setVisible(false);
		pbRegSta->setVisible(false);
		lblRegSta->setVisible(false);
		pbGetConfig->setVisible(true);
		lblGetConfig->setVisible(true);
		pbSetConfig->setVisible(true);
		lblSetConfig->setVisible(true);
		pbConfigAp->setFocus();
		wiz->pbBack->setEnabled(true);
	} else {
		pbConfigAp->setVisible(false);
		lblConfigAp->setVisible(false);
		pbRegAp->setVisible(false);
		lblRegAp->setVisible(false);
		pbRegSta->setVisible(true);
		lblRegSta->setVisible(true);
		pbGetConfig->setVisible(false);
		lblGetConfig->setVisible(false);
		pbSetConfig->setVisible(false);
		lblSetConfig->setVisible(false);
		pbRegSta->setFocus();
		wiz->pbBack->setEnabled(false);
	}

	wiz->pbNext->setEnabled(false);
	wiz->pbCancel->setEnabled(true);
	return true;
}

bool SelectMode::post_next()
{
	int regmode;

	switch (MainProcess::getMode()) {
	case MainProcess::MODE_ENR:
	case MainProcess::MODE_MANUAL:
		regmode = 0;
		break;
	case MainProcess::MODE_REG_CONFAP:
		regmode = 1;
		break;
	case MainProcess::MODE_REG_REGAP:
		regmode = 2;
		break;
	case MainProcess::MODE_REG_REGSTA:
		regmode = 3;
		break;
	default:
		return false;
	}

	if (!MainProcess::setRegMode(regmode)) {
		QMessageBox::warning(this, label->text(),
							 "Could not set WPS registrar mode\n");
		return false;
	}

	return true;
}

void SelectMode::cancel()
{
	wiz->close();
}

void SelectMode::selConfigAp()
{
	MainProcess::setMode(MainProcess::MODE_REG_CONFAP);
	MainProcess::setArea(MainProcess::AREA_BOTH_INBAND_UPNP);
	wiz->next();
}

void SelectMode::selRegAp()
{
	MainProcess::setMode(MainProcess::MODE_REG_REGAP);
	MainProcess::setArea(MainProcess::AREA_INBAND_ONLY);
	wiz->next();
}

void SelectMode::selRegSta()
{
	MainProcess::setMode(MainProcess::MODE_REG_REGSTA);
	MainProcess::setArea(MainProcess::AREA_UPNP_ONLY);
	wiz->next();
}

void SelectMode::selGetConfig()
{
	MainProcess::setMode(MainProcess::MODE_ENR);
	MainProcess::setArea(MainProcess::AREA_INBAND_ONLY);
	wiz->next();
}

void SelectMode::selSetConfig()
{
	MainProcess::setMode(MainProcess::MODE_MANUAL);
	wiz->next();
}

