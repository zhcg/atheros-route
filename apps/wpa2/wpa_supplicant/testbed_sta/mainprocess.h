/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: mainprocess.h
//  Description: main process control header
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

#ifndef MAINPROCESS_H
#define MAINPROCESS_H

#include <QProcess>
#include <QMutex>
#include <QThread>
#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
#include <QSocketNotifier>
#elif defined(CONFIG_CTRL_IFACE_NAMED_PIPE)
#include <QTimer>
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
#include "os.h"

struct wpa_ctrl;

#define CTRL_REQ_EAP_WPS_COMP		"EAP_WPS_COMP"
#define CTRL_REQ_EAP_WPS_FAIL		"EAP_WPS_FAIL"
#define CTRL_REQ_EAP_WPS_PASSWORD	"EAP_WPS_PASSWORD"

#define CTRL_REQ_UPNP_COMP			"UPNP_COMP"
#define CTRL_REQ_UPNP_FAIL			"UPNP_FAIL"
#define CTRL_REQ_UPNP_PASSWORD		"UPNP_PASSWORD"

#define CTRL_REQ_NFC_READ_TIMEOUT	"NFC_READ_TIMEOUT"
#define CTRL_REQ_NFC_WRITE_TIMEOUT	"NFC_WRITE_TIMEOUT"
#define CTRL_REQ_NFC_FAIL_READ		"NFC_FAIL_READ"
#define CTRL_REQ_NFC_COMP_READ		"NFC_COMP_READ"
#define CTRL_REQ_NFC_COMP_WRITE		"NFC_COMP_WRITE"
#define CTRL_REQ_NFC_ADD_NEW_AP		"NFC_ADD_NEW_AP"


class MainProcess
{
public:
	MainProcess();
	~MainProcess();

	enum MODE {
		MODE_ENR = 0,
		MODE_REG_CONFAP,
		MODE_REG_REGAP,
		MODE_REG_REGSTA,
		MODE_MANUAL
	};

	enum METHOD {
		METHOD_NONE = 0,
		METHOD_NFC,
		METHOD_PIN,
		METHOD_PBC
	};

	enum AREA {
		AREA_BOTH_INBAND_UPNP = 0,
		AREA_INBAND_ONLY,
		AREA_UPNP_ONLY,
	};

	static bool start(const char *wi, const char *driver, const char *nfc);
	static void terminate(bool enforced = false);

private:
	static int ctrlRequest(char *cmd, char *res, size_t *len);

public:
	static bool connectMonitor(QObject *receiver, const char *method);
	static bool reconnectMonitor();
	static bool disconnectMonitor(QObject *receiver, const char *method);

	static bool ctrlPending();
	static bool receive(char *msg, size_t *len);
	static bool getCtrlRequelst(char *buf, size_t len, int *priority, char *req, char *msg);

	static bool scanRequest();
	static bool getScanResults(char *result, size_t *len);
	static int addNetwork();
	static bool removeNetwork(int index);
	static bool setNetworkParam(int index, char *field, char *value, bool quote);
	static bool getNetworkParam(int index, char *field, char *value, bool quote);

	static bool connectNetwork(int index);
	static bool disableNetwork(int index);

	static bool setRegMode(int regmode);
	static bool setWpsPassword(const char *pwd);
	static bool setWpsConfiguration(int index);
	static bool clearWpsPassword();

	static bool setUpnpInterface(const char *ifname);
	static bool isEnabledUpnp();
	static bool setEnabledUpnp(bool enabled);
	static bool upnpScanRequest();
	static bool getUpnpScanResults(char *result, size_t *len);
	static bool sendUpnpGetDevInfo(const char *control_url);
	static bool sendSelectedRegistrar(const char *control_url, bool enabled);

	static bool writeNfcConfig(int index);
	static bool readNfcConfig();
	static bool writeNfcPassword(int index);
	static bool readNfcPassword(int index);
	static bool cancelScanNfcToken();

	static void generatePIN(char pwd[9]);
	static bool validatePIN(const char pwd[9]);

	static bool startPbc();
	static bool stopPbc();

	static bool setDebugOut(QObject *receiver, const char *method);
	static QString readDebugMsg();

	static void setWirelessInterface(const char *ifname) {
		if (wirelessInterface) {
			free(wirelessInterface);
			wirelessInterface = 0;
		}
		if (ifname)
			wirelessInterface = strdup(ifname);
	};
	static char *getWirelessInterface() { return wirelessInterface; };
	static void setWiredInterface(const char *ifname) {
		if (wiredInterface) {
			free(wiredInterface);
			wiredInterface = 0;
		}
		if (ifname)
			wiredInterface = strdup(ifname);
	};
	static char *getWiredInterface() { return wiredInterface; };

	static void setMode(MODE _mode) { mode = _mode; };
	static MODE getMode() { return mode; };

	static void setMethod(METHOD  _method) { method = _method; };
	static METHOD getMethod() { return method; };

	static void setArea(AREA  _area) { area = _area; };
	static AREA getArea() { return area; };

	static void setNetworkIndex(int index) { networkIndex = index; };
	static int getNetworkIndex() { return networkIndex; };

	static void setControlUrl(const char *control_url) {
		if (controlUrl) {
			free(controlUrl);
			controlUrl = 0;
		}
		if (control_url)
			controlUrl = strdup(control_url);
	}
	static char *getControlUrl() { return controlUrl; };

private slots:
	void receiveMsgs();

private:
	static QProcess	*mainProcess;

	static struct wpa_ctrl *monitor;
	static struct wpa_ctrl *ctrl;

	static char *iface;

	static QMutex *mtx;

	static char *wirelessInterface;
	static char *wiredInterface;

	static MODE mode;
	static METHOD method;
	static AREA area;

	static int networkIndex;
	static char *controlUrl;
	static bool enabledUpnp;

#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
	static QSocketNotifier *msgNotifier;
#elif defined(CONFIG_CTRL_IFACE_NAMED_PIPE)
	static QTimer *msgNotifier;
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
};

class UtilSleep : public QThread
{
public:
	UtilSleep() {};
	~UtilSleep() {};

	static void sSleep(unsigned long sec) { QThread::sleep(sec); };
	static void mSleep(unsigned long msec) { QThread::msleep(msec); };
	static void uSleep(unsigned long usec) { QThread::usleep(usec); };
};

#endif // MAINPROCESS_H
