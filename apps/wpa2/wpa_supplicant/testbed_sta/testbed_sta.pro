LANGUAGE	= C++

CONFIG	+= qt warn_on release

DEFINES += CONFIG_CTRL_IFACE WPS_OPT_UPNP WPS_OPT_NFC

win32 {
  # cross compilation to win32 with vs2005
  TEMPLATE = vcapp
  LIBS += ws2_32.lib \dev\WpdPack\lib\wpcap.lib
  DEFINES += CONFIG_NATIVE_WINDOWS CONFIG_CTRL_IFACE_NAMED_PIPE USE_WINIFLIST
  SOURCES += ../win_if_list.c
  HEADERS += win_if_list.h
  INCLUDEPATH += \dev\WpdPack\include
} else:win32-g++ {
  # cross compilation to win32
  TEMPLATE = app
  LIBS += ws2_32.lib \dev\WpdPack\lib\wpcap.lib
  DEFINES += CONFIG_NATIVE_WINDOWS CONFIG_CTRL_IFACE_NAMED_PIPE USE_WINIFLIST
  SOURCES += ../win_if_list.c
  HEADERS += win_if_list.h
  INCLUDEPATH += \dev\WpdPack\include
} else {
  TEMPLATE = app
  DEFINES += CONFIG_CTRL_IFACE_UNIX
}

INCLUDEPATH	+= . .. ../../common

HEADERS	+= \
	testbedsta.h \
	about.h \
	mainprocess.h \
	pagetemplate.h \
	setupinterface.h \
	selectmode.h \
	netconfig.h \
	selectmethod.h \
	writenfcconfig.h \
	readnfcconfig.h \
	selectap.h \
	inputpin.h \
	displaypin.h \
	wpsauthentication.h \
	wpspbc.h \
	debugwindow.h \
	wpamsg.h \

SOURCES	+= \
	main.cpp \
	testbedsta.cpp \
	mainprocess.cpp \
	setupinterface.cpp \
	selectmode.cpp \
	netconfig.cpp \
	selectmethod.cpp \
	writenfcconfig.cpp \
	readnfcconfig.cpp \
	selectap.cpp \
	inputpin.cpp \
	displaypin.cpp \
	wpsauthentication.cpp \
	wpspbc.cpp \
	../../common/wpa_ctrl.c \

FORMS	= \
	testbedsta.ui \
	about.ui \
	setupinterface.ui \
	selectmode.ui \
	netconfig.ui \
	selectmethod.ui \
	writenfcconfig.ui \
	readnfcconfig.ui \
	selectap.ui \
	inputpin.ui \
	displaypin.ui \
	wpsauthentication.ui \
	wpspbc.ui \
	debugwindow.ui \


unix {
  UI_DIR = .ui
  MOC_DIR = .moc
  OBJECTS_DIR = .obj
}

