apps/wpa2 software -- Main documentation file

                                    Original Author: Ted Merrill  2007-2008

===========================================================================
Overview
===========================================================================

The apps/wpa2 source directory provides a solution for 
authentication software support for
Atheros access point hardware in both access point and station modes,
including WPS support.
However, the software is quite general and can be used on any linux-based
device with Atheros WLAN hardware.

The "hostap" free software written by Jouni (hostapd, wpa_supplicant) 
is quite good and complete but (as of this writing) does not include WPS
support.
This is thus a clone which includes WPS support; the clone including WPS
support was developed by Sony, and then further modified by Atheros,
particularly to make UPnP support more economical.
Additionally, Atheros has made changes to the configuration file format
in the hope that this will make it easier to use.

Note that this software relies intimately upon Atheros "madwifi" drivers
that have the proper WPS extensions.

Features:
-- Complete(?) WiFi authentication support as provided by hostapd
    and wpa_supplicant
-- Complete(?) WiFi-based WPS support
-- UPnP WPS support (currently for hostapd only, see notes below)
-- Atheros WPS extensions support

Latent Features:
-- UPnP support from wpa_supplicant requires either bringing back
    libupnp (ugh!) or writing a "tiny upnp" code for it
    (as was done for hostapd).
-- Sony code provides near field control (NFC) but we have never disabled
    this and it won't work without some effort at fixing bit rot.

The following document attempts to give a definitive list of supported
and unsupported features, plus configuration methods:
Atheros_WPS_Extensions_Specification.doc

============================================================================
Running the software
============================================================================

The configuration file system for hostapd has been greatly changed by
Atheros. Please read configuration.txt in detail.

The central configuration file is a "topology file",
see examples/topology.conf .
Normally, both wpa_supplicant and hostapd will be run 
with a single topology file as an argument
and will read the configuration files listed therein.
The program "apstart" also sets up the network devices according to
the content of the topology file.

The topology file refers to other files.
A common problem is that the path to the other files is incorrect.

There are examples of all files in the examples directory; these
examples can be used as-is for simple cases.
The examples are self-documenting (contain lots of comments);
these comments should be removed (to save space) from a product.

Prior to running the authentication programs, the network devices
and bridge devices must be set up.
(The drivers need to be loaded first, but that is outside of the scope
of this document).
One way to do that is to run:

    apstart /etc/wpa2/topology.conf

hostapd is invoked as:

    hostapd [-d] [-B] /etc/wpa2/topology.conf &

where:
        -d turns on debug messages (-dd for even more) ... omit if not needed
        -B daemonizes

wpa_supplicant is invoked in the same way.

Communication with the wpa_supplicant and hostapd programs is via
socket files that are normally resident in the /var/run/wpa_supplicant
and /var/run/hostapd directories (respectively).
The wpatalk program serves an example of how to talk via these
socket files.
The protocol involves sending a command line and receiving an acknowledgement;
for more complicated operations, it may be necessary to wait for 
asyncronous acknowledgement.

Once running, you can use wpatalk to send commands to the running
programs, or more exactly to the context within the program that handles
a particular network device (e.g. ath0, ath1 etc.).
wpatalk is invoked as:

    wpatalk ath<n> 'command arg arg'

(you can give multiple commands, BUT
don't forget to quote any command that has arguments, even if 
you have only one command!)
or just
    
    wpatalk ath<n>

for interactive, in which case you enter no more than one command
per input line.
Commands whose names are capitalized ("raw commands") 
are passed through to the appropriate program unmodified.
Commands whose names are lower case receive handling within wpatalk
before being (possibly) passed through (wpatalk detects if
it is talking to a wpa_supplicant or hostapd).

See hcmd.txt for a list of raw commands for hostapd.
See wcmd.txt for a list of raw commands for wpa_supplicant.


============================================================================
Configuring hostapd 
============================================================================

Refer to the comments in the example configuration files for details.

The topology file defines (in a "radio" section) how the underlying
network interface (e.g. "wifi0") is configured with "virtual APs"
which become the active wireless network interface, e.g. "ath0".

The topology file must define a bridge section that connects an active
wireless interface such as "ath0" (defined in a radio section)
with a wired interface such as "eth0".
Any (?) number of interfaces can be bridged together.

The general parameters for an AP radio are defined in a config file
referenced from the radio/ap subsection, e.g.
        config install/etc/wpa2/ap_example_80211g.conf
This file is rarely if ever changed once set up for a particular access
point.

The per-BSS (per active network interface, such as "ath0") parameters
are more frequently changed.
Note that the fields "interface" and "bridge" are present for historical
reasons but actually ignored.
The field "ctrl_interface" must be "/var/run/hostapd" in order to work
with the default setting for wpatalk.
In order for WPS to work, regardless of the authentication mode,
the following fields must always be set:
        eap_server=1

To configure the AP to be totally open and unencrypted you need the
following values:
        ieee8021x=0
        auth_algs=1
        wpa=0
        wpa_key_mgmt=
(or equivalently, the field "wpa_key_mgmt" may be commented out).
(and you need to comment out wep key info?).

To configure the AP to use WEP-PSK ("WEP-open"):
        ieee8021x=0
        wpa=0
        wpa_key_mgmt=
        auth_algs=1
        wep_default_key=0
(wep_default_key can be 0, 1, 2 or 3 but 0 is most common)
        wep_key0="abcde"
(Use appropropriate wep key! Use wep_key<n> for wep_default_key=<n>).
(Wep keys must be quoted strings of 5 or 13 chars, or hex numerals
of 10 or 26 characters; other wep key sizes are NOT supported).

To configure the AP to use WPA-TKIP-PSK:
        ieee8021x=1
        auth_algs=1
        wpa=1
        wpa_key_mgmt=WPA-PSK
        wpa_pairwise=TKIP
and also set wpa_psk or wpa_passphrase (may need to remove wep key info?).

To configure the AP to use WPA2-CCMP-PSK:
        ieee8021x=1
        auth_algs=1
        wpa=2
        wpa_key_mgmt=WPA-PSK
        wpa_pairwise=CCMP
and also set wpa_psk or wpa_passphrase (may need to remove wep key info?).

There are many other combinations supported by hostapd...


============================================================================
Using Wifi Protected Setup (WPS)
============================================================================

See above for configuration requirements for hostapd.
Note that wps_disabled=0 is required (and wps_upnp_disabled=0 to 
allow UPnP sessions).

The magical and very confusing AP (hostapd) parameter "wps_configured"
---------------------------------------------------------------------------
The "wps_configured" hostapd parameter is mandated by the WPS specification.
It's factory default may be 0 or 1 (false or true) but as a practical
matter it is expected (by WPS test suites) that it will be 0.
It is normally set to zero only on a "set factory defaults" operation,
although it could be set by the user at any time if they want.
It is set to 1 (true) whenever our AP has been configured by any means
including via WPS and via manual configuration of any parameters.

With wps_configured=0, if a station requests (as an "enrollee") to
be configured from our AP via WPS, it is not given the current
configuration of our AP; instead, our AP invents a new configuration
and gives it to the station.  If the session successfully terminates,
our AP adopts this new configuration as it's own.
If this seems really strange, it is... but it is the standard.
The new configuration will be one of WPA-PSK, WPA2-PSK, or mixed; plus
TKIP, CCMP or mixed according to the original configuration.
If the original configuration was not one of these, then the new configuration
will be forced to be WPA2-PSK/CCMP.
Following this reconfiguration, wps_configured will be set to 1.

Alternately, with wps_configured=0, a station can assert itself as a registrar
and configure our AP automatically with no user intervention on our AP
side, provided that our AP is configured with a non-empty wps_default_pin
value (which must of course have been provided to the station by the user).
[If no default pin was provided to our AP, then the user will have to
inititate a wpatalk "configme" session.]
Following this reconfiguration, wps_configured will be set to 1.

Once an access point is marked configured ("wps_configured" config filed), 
it cannot be reconfigured without user intervention 
(either "configme" command, or set "wps_configured" to 0 and RECONFIGURE).
In addition, configuring the access point via WPS will set the
"wps_configured" field to 1 (and cause a RECONFIGURE).

If you like your current configuration, DO NOT set or leave 
wps_configured=0 !


WPS sessions, registars, enrollees.
-------------------------------------------------------------------
UPnP sessions (see below) involve an external registrar, an access
point, and a station, and are inititated by the external registrar.
Otherwise, all WPS sessions are between an access point and a station
and are initiated by the station.
The station decides if it wants to function as "registrar" or "enrollee"
(and the access point then assumes the other role),
which however means less than it sounds...
which ever side plays "enrollee" sends it existing configuration in an
M7 message, and the side playing "registrar" can actually adopt that
configuration as it's own (and possibly terminate the session) or
can send over a new configuration (or even the same configuration)
back to the "enrollee" in an M8 message.
So the side playing "registrar" may actually be the one that gets
configured...


PINs and push buttons
--------------------------------------------------------------------
To provide some security for WPS, both enrollee and registrar must
share the same PIN ("personal information number").
In most cases, a WPS PIN is an 8 decimal digit numeral where the last
digit is a checksum of the first 7.
[Offically, WPS supports many other forms of PINs, but actual support
for these is dubious].
It doesn't really matter how or where the PIN is invented so long as both
sides share the same PIN, but as a practical matter one side may insist
that either it provide the PIN or that the other side provides the PIN.
The user gets to copy the PIN over to the other side.

As a special (but less secure case), the PIN 00000000 may be used by
both sides. This PIN is typically generated when the user presses a
WPS push button on both sides (or selects a menu item for "push button").


Manually started WPS session
------------------------------------------------------------------
Using either our hostapd or our wpa_supplicant programs, a manual session
can be started by pushing a button (where supported) or using the
wpatalk command or equivalent.
The wpatalk command is explained in more detail below.
Manually started WPS sessions always have a timeout, by default 2 minutes,
after which the session is cancelled.


UPnP WPS sessions with external registrar (hostapd)
-------------------------------------------------------------------
Unless disabled, hostapd will allow anyone with network access to
become a external registrar of a WPS session via UPnP.
This initiates a WPS session with a timeout of 5 minutes.
(The session can be also cancelled by appropriate wpatalk commands).
UPnP is a IP-based protocol, so it only works if the registrar can
communicate via IP (both UDP and TCP) with our access point...
which can be either via wired or wireless.
Once the WPS session is started, our AP acts as a proxy between
the external registrar and a client station (the client station communicates
via WiFi packets as for a regular WPS session).


Default PIN ("label method") for hostapd (AP)
-----------------------------------------------------------------
If hostapd has "wps_default_pin" configured to a valid PIN
number (one example is: 12345670) then hostapd will automatically
provide configuration information to any station who knows the PIN
without any user intervention w.r.t. our AP (the user will presumeably
need to tell the station what the PIN is.
If wps_configured=0, our AP will also accept a new configuration from
a station without user intervention to our AP (see above).

wps_default_pin is NOT used during the time that a manually or UPnP
WPS session is in effect.

Please note that using a default PIN significantly reduces security.


Windows Vista support of access points
---------------------------------------------------------------------
Oddly, the Windows Vista network window shows only access points
with a wired connection and which have wps_configured=0.
The icon for the access point will be labelled with the name from
the wps_dev_name parameter.
By clicking on the icon you can inititate a WPS session to configure
the access point.

To make Vista show the access point, use the following command:
    wpatalk ath<n> 'SETBSS wps_configured=0' 'RECONFIGURE'
This will allow the AP to be seen in the Vista Network window and from
that be configured.


Summary of wpatalk WPS commands
------------------------------------------

Please note that a memory-efficient system would not use wpatalk program
but would instead embedded the communication protocol into the
appropriate software... however, the commands passed via wpatalk
would still be used.

The following command description assumes interactive mode
(wpatalk started with no command, e.g. just: wpatalk ath0).
To pass these on the command line, BE SURE to quote entire command.
E.g.:
    GOOD:  wpatalk ath0 'configthem pin=12345670'
    WON'T WORK:   wpatalk ath0 configthem pin=12345670

configstop -- cancel any ongoing WPS operation
pin -- print out a random PIN number (in case you need one)
configme [pin[=<pin>]] [clean]  -- obtain configuration
    (If we are station, obtained from AP; and vice versa).
    PIN mode is used if pin is given.
    "clean" is understood by wpa_supplicant ONLY to remove any prior network
    definitions (may be a good idea!).
    If the configuration attempt fails then the station or AP is 
    reconfigured (from it's configuration file in case of AP).
configthem [pin[=<pin>]] -- give configuration
    (If we are station, given to AP; and vice versa).
    Our current network configuration is given to the other side;
    if necessary, disable the network configuration to prevent it
    being used on our side.
    PIN mode is used if pin is given. UPnP is used if upnp is given
    (used anyway for APs).

In addition, various raw commands can be used directly...
see hcmd.txt and wcmd.txt .
Refer to help message for wpatalk and to source code in hostapd/ctrl_iface.c
and wpa_supplicant/ctrl_iface.c

For wpa_supplicant the following are particularly useful:

LIST_NETWORKS   -- list all loaded network configurations
REMOVE_NETWORK <n> -- remove given network config
REMOVE_NETWORK all  -- remove all network configs

For hostapd:
SETBSS <tag>=<value>   -- set a bss configuration value as if from config file;
    result is saved to config file by default, but may require RECONFIGURE
    to take full effect.
SETRADIO <tag>=<value> -- set a radio configuration value as if from 
    config file;
    result is saved to config file by default, but may require RECONFIGURE
    to take full effect.

And for both:

STATUS -- print status summary
RECONFIGURE -- re-read configuration files
    (For hostapd, this essentially reboots the program while keeping
    the same process id... everything is disrupted as a result).
QUIT  -- terminate program


WPS Client Examples:
----------------------------------------

To configure client from a WPS server (e.g. AP) by push-button method,
push the button on the AP and then use the command
(assuming wpa_supplicant is talking on ath0):
    wpatalk ath0 'configme clean'
(The "clean" parameter removes any previous network definitions;
omit this to retain previous network definitions).

To configure from a WPS server (e.g. AP) by pin method,
enter a suitable pin on the AP and then use the command:
    wpatalk ath0 'configme clean pin=12345670'
(where 12345670 is replaced by desired PIN).

To provide a configuration to a WPS access point based upon the
current network configuration,
tell the access point what you are doing and then do:
    wpatalk ath0 'configthem'
or
    wpatalk ath0 'configthem pin=12345670'
(where 12345670 is replaced by desired PIN).

The above "configthem" examples rely on that you want to use the
currently operating network configuration.
To use a different configuration, refer to it by number 
(perhaps after listing with LIST_NETWORKS), e.g.
    wpatalk ath0 'configthem nwid=2 pin=12345670'

To invent a new and unique pin, one way (inefficient) way is to
use the command e.g.:
    wpatalk ath0 pin
which will print out a random but valid PIN.


WPS Server Examples:
---------------------------------------

To configure from a registrar via PIN method,
    wpatalk ath0 'configme pin=<pin>'
Please note the quotes.

To provide configuration to a station via push button method,
    wpatalk ath0 'configthem'

To provide configuration to a station via PIN method,
    wpatalk ath0 'configthem pin=12345670'
(where 12345670 is replaced by desired PIN).



===========================================================================
How reloading configuration works
============================================================================

wpa_supplicant has a RECONFIGURE command that re-reads the configuration
file for just the given network interface, without (i think) disrupting the
the control interface or other network interfaces.

hostapd has a method of reloading all configuration files on receipt of
a signal... but this has been commented out #ifdef EAP_WPS in favor of
a hack in main() that essentially restarts the entire program after a
5 second delay, while retaining the same process id...
this will cause disruption of operation but that may be acceptable.
(This is triggered by the RECONFIGURE command which thus does all interfaces,
not just the requesting interface).
If there is a syntax error in the configuration file, the hostapd 
refuses to continue and exits... this would be a problem for embedded
system, which should not so fail?... although some sort of system
of "return to defaults" could save the day...
There is a file "reconfig.c" ... none of this code is actually used...


===========================================================================
Bugs / missing features that might be fixed
===========================================================================

The following are major problems of design and not easily fixed.

-------------------------

hostapd:

tinyupnp may be sending out too many replies to M-SEARCH -- 
these should possibly be quenched somehow.

tinyupnp does an "advertise down" and then "advertise up" at the beginning,
but in the case where the radio link itself is being used to carry UPnP
this means that these messages don't get through.
Not clear how to deal with this, because we don't necessarily know
what bridging is happening.

We may not (do not?) handle receiving multiple instances of network
key via WPS correctly.
wps_set_ap_ssid_configuration() is written to assume there is only one
instance for each information element type, which is wrong in this case.
However this should be good enough to grab the first key...


--------------------

wpa_supplicant:

Repeated "configme"s can get in a situation where registrar rejects,
e.g. by AP61 with:
../../common/RegProtocol/ProtoUtils.cpp(1074):RPROTO: Nonce mismatch
../../common/StateMachine/RegistrarSM.cpp(314):REGSM: Recvd message is not mean
t for this registrar. Ignoring...
restarting wpa_supplicant can be the only fix.
Perhaps gets into this when one "configme" interrupts another...

--------------------

===========================================================================
Bugs / missing features that will >>>NOT<<< be fixed
===========================================================================

(tiny) UPnP solution for wpa_supplicant:
Not written at all for wpa_supplicant; when enabled, UPnP is automatically
disabled for wpa_supplicant.
We could fix this if it is important enough.

-------------------------------------

Ideally wpa_supplicant and hostapd would be a single program, but the
massive overlapped naming makes that a LOT of work... bad news for anyone
who wants to do this in a single-address-space RTOS.
The could be easily co-resident IFF you renamed most of the global
symbols that are unique to each program but the same as that in the
other program...

------------------------------

modified public madwifi driver:

Sometimes get system hangs (particularly since i upgraded madwifi to 0.9.3.3).
Not a clear correlation to cause of hang.
This happens quite quickly with Linux 2.6.18 but only occasionally with
Linux 2.6.22
This does not seem to happen at all if only doing hostapd.

To switch from WEP to open requires reloading driver (remove and re-insert
PC card), otherwise the packets are apparently still being encrypted.

---------------------

libupnp UPnP solution:

This solution is not complete but has been abandoned in favor of
the tiny UPnP solution. Therefore the following should be considered
obsolete:

2008-03-05 hostapd upnp now seems to work properly for PC Linux and
has been ported to work for Atheros AP images, HOWEVER there are strange
memory corruption issues associated with it's use of pthread and
thus it does NOT yet work properly on Atheros APs.
... traced to a bad libpthread that Atheros has had.
IGNORE if using tiny_upnp

When UPNP is enabled (using libupnp) all wireless interfaces (i.e. hostapd
conf files) must be tied to the same upnp interface (i.e. the bridge)
or else hostapd will fail to start.
FIXED with tiny_upnp

Only the first wireless interface is supported via (e.g. wired) upnp.
FIXED with tiny_upnp

UPnP is anyway more code than desired, and uses more resources
(e.g. it starts a bunch of threads!)
FIXED with tiny_upnp

----------------------

NFC code is entirely unsupported and suffers from bit rot.

-------------------

GUI:

The GUI is currently broken (the supported commands in hostapd and
wpa_supplicant have been changed, and the GUI has not been updated yet).

testbed_sta doesn't work with an already running wpa_supplicant...
insists on launching it itself, then killing it on the end.

The scan time of the client is included in the countdown.....

---------------------------------------------

============================================================================
Documentation
============================================================================

Various documentation has been copied into the "original" subdirectory.

General WPS Documentation
------------------------
Wi-Fi Protected Setup Specification 1.0h.pdf  -- basic spec
        Unfortunately this says almost nothing about UPnP
WPS_TestPlan_v1-6_2007-11-08.pdf -- test plan


General UPnP Documentation
-----------------------------
UPnP-DeviceArchitecture-v1.0.pdf -- almost a spec and almost a tutorial

WPS + UPnP Documentation
----------------------------
Don't seem to have any e-copies of the following, get a printed copy:
WFADevice:1 -- Some motivational info
WFAWLANConfig:1  -- pretty obtuse, but gives some clue
WLANAccessPointDevice:1  -- ditto


Sony WPS Software Package Documentation
-----------------------------------------
Perhaps mostly of historical interest at this point.
Note that it provides very little motivational documentation.
Read through:
NFC631KT_Software_for_Wi-Fi_Protected_Setup_Resource_Packages_Description.pdf
and perhaps (although this applies to the bootable CD image):
NFC631KT_Software_for_Wi-Fi_Protected_Setup_Setup_Guide.pdf


============================================================================
Tour of the top level wpa2 directory
============================================================================

Makefile -- used to build "everything" including install
    See top of file for environmental values that modify execution
README.txt -- this file. The primary documentation.
apstart/ -- source code for "apstart" program that may be used to
    set up network devices and bridging based on topology file.
buildhost -- shell script to build software for Linux PC development.
    See also pcenv.sh
buildmips -- hack shell script to build within Atheros AP build system
    without having to build everything. May not work for you.
common/ -- source code for libwpa_common.{a,so} which is common subroutines
    used by both hostapd and wpa_supplicant.
configuration.txt -- description of runtime configuration files
examples/ -- examples of runtime configuration files, copied to
    etc/wpa2/ beneath the installation directory.
gui.txt -- documentation on Sony GUI in case anyone wants to bring that
    back to life.
hcmd.txt -- list of commands that may be sent to hostapd.
hostapd/ -- source code for hostapd, the authentiation server for APs.
madwifi.host/ -- source code for Linux PC host drivers.
    This is used for COMPILATION for the Linux PC host... beware!
original/ -- original tarballs and other files, retained for historical
    interest.
pcenv.sh -- source from your bash shell to set env. vars. for development
    on your Linux PC.
wcmd.txt -- list of commands that may be sent to wpa_supplicant
wireless_tools/ -- utilities for getting info about wireless devices
    on your linux system.  These have been modified to know about WPS.
wpa_supplicant/ -- source code for wpa_supplicant, the
    authentication client agent for WiFi STAs.
wpatalk/ -- source code for wpatalk, a program that allows you to send
    commands to hostapd and wpa_supplicant.


===========================================================================
Developing and Debugging the software -- using Linux PC
===========================================================================

It is highly desireable to develop and debug the software on a Linux PC
because this allows fast turn around time for compilation (no separate
installation required!) and easy use of debugger.

I recommend using a dedicated linux PC for this, because you may experience
network interrupts and even crashes (due to driver issues).
Insert an Atheros WiFi card.
See section below on compatible linux systems.

Install the modified public madwifi driver:
Login as root.
Copy original/madwifi-modified.tgz to a convenient location and
untar it:
        tar xzvf madwifi-modified.tgz
resulting in a directory "madwifi.host".
cd to the "madwifi.host" directory, and do:
        make 
        make install
After any kernel upgrade, switch of files to new computer etc. 
you will need to repeat the above installation of the modified
public madwifi driver.
[If you use some other driver for your Linux PC, then you may 
need to replace the souce code "madwifi.host" subdirectory with
a link to the soure code of that other driver... good luck].

You will generally need various other devices to do testing with,
e.g. Vista PC for testing UPnP, and some sort of WPS client.

For wifi packet sniffing, i recommend having another Linux PC 
with Atheros WiFi card and public madwifi driver.
See below "Using wireshark as a sniffer under Linux".

There are unfortunate interactions with "udev" (udev is commonly
used on Linux PCs to manage device naming peristence).
This may result in e.g. ath0 appearing to be unavailable after
e.g. switching to another wifi card.
To fix these problems:
-- remove the card or hardware, if possible.
-- login as root
-- edit /etc/udev/rules.d/70-persistent-net.rules and remove references
to "ath" devices, 
-- do: /etc/init.d/udev restart

Before compiling or debuging:
cd to this directory
Examine the content of pcenv.sh to make sure you understand it.
In particular, note that it sets
    export BUILD_WPA2_DEBUG=y
which will result in compilation of unoptimized executables which
are easy to debug.
Run:
    . ./pcenv.sh  # assuming using bash ... do not try to execute this file...
If you are csh user, you are welcome to translate this to csh and
submit it...

To compile:
    make clean # if you want to be sure
    ./buildhost  # or just "make"
For updates you can generally skip the "make clean" step.
For faster updates you can cd into the appropriate modules
and do "make install" (does not apply to testbed modules).

Note that this puts files into an "install" subdirectory, and the
pcenv.sh file changed (we hope) your PATH and LD_LIBRARY_PATH to
use files therein.
NOTE that after doing top level make you will have new configuration
files beneath "install" which will need to be fixed,
specifically change every instance of "/etc/..." to "install/etc"
assuming you will be executing from the top level directory.

Follow above instructions for running the software (be sure to
cd to the top level directory first!).

To run under valgrind to check for memory leaks and other errors:
Set:
export BUILD_WPA2_DEBUG=n
export BUILD_WPA2_STATIC_COMMON_LIBRARY=y
... and then do make.
Modify install/etc/wpa2/* so that file references are to
install/etc/... instead of /etc/... 
Run from the top ("wpa2") directory as:
valgrind -v --leak-check=yes hostapd -d install/etc/wpa2/topology.conf \
        2>&1 | tee -a ../junk.log
In order to get the final memory leak report, the program must be
convinced to terminate voluntarily, e.g.:
wpatalk ath0 QUIT



===========================================================================
Developing and Debugging the software -- Atheros AP
===========================================================================

The Makefiles make use of environmental variables that must be set
by the upper layer Makefiles or build scripts in order to override
Linux PC oriented choices with those suitable for the target.
The script "buildmips" attempts to encapsulate this, but is easily
subject to bit rot and so may not work for you.
In any event, "buildmips" does not make a final image you can load,
so you will need to run the official build procedure to get that.

To do debugging easily, you will want to do the build with the environmental
variable set:
    export BUILD_WPA2_DEBUG=y
This compiles without optimizations and staticly links in the wpa_common
library... so it won't work unless your system has enough memory.
Assuming that goes well, you want to use gdbserver on the target side
and gdb on the host side... refer to instructions in the Atheros
build system README files.


============================================================================
Using wireshark as a sniffer under Linux
============================================================================

To display WPS info in wifi packets, you'll need a modern enough
wireshark (0.99.7 does not have it)... try original/wireshark-0.99.7-wps4.tgz
which has been patched.
This requires several "development" packages be installed
including libgtk2.0-dev libglib2.0-dev and libpcap0.8-dev

? it might be a good idea to use a separate machine for sniffing! ?

Install drivers from either latest madwifi or the one in this directory.
(See build instructions above).

Install atheros card as necessary.

Create a virtual ap for monitoring:
wlanconfig ath create wlandev wifi0 wlanmode monitor
... this will print the name of the "ath" it created, e.g. ath0 or ath1 etc.
... although in the presence of udev it gets changed to something else;
... you'll have to do the following to find out which it is:
iwconfig
... which i'll refer to as athY in the following:
ifconfig athY up
... and optionally set channel (hmmm this doesn't seem to stick)
iwconfig athY channel <chan>

You can only sniff one channel... e.g. select the one the ap is using.
iwconfig <interface> channel <channel>
However, i have found that somehow the channel keeps getting reset on me;
the following is an inadequate workaround:
while true; do iwconfig <if> channel <chan>; done

Run wireshark (see above about version!)
Useful filters:
wlan.addr == <macadddr>         -- any packets to/from given wlan mac addr
wlan.bssid == <bssid> 
wlan_mgmt.tag.interpretation == "xxx"  -- filter on SSID xxx


See: http://www.wireshark.org/docs/dfref/w/wlan.html


============================================================================
Using JumpStart
============================================================================

Set the following DWORD value in the registry to enable logging of the 
Jumpstart session:
       HKCU\Software\Atheros\Jumpstart\Debug\EnableTraceLog = 1
(HKCU == HKEY Current User)

The default tracelog folder on XP:
   \Documents and Settings\<User>\Local Settings\Application Data\Atheros\Jumpstart
The default tracelog folder on Vista:
      \Users\<User>\AppData\Local\Atheros\Jumpstart

A file named JswLog.txt will be created in the logging folder. The log
from the previous session will be saved as lastJswLog.txt.


============================================================================
UPnP Monitors
============================================================================

The only UPnP monitor i could find was a shareware windows executable
from http://upnp.hilisoft.com/ ... 
actually, the "Browser" is free and does what i really needed to do,
which is to make sure that our AP is visible via UPnP.
The "Explorer" is 7 day free trial, then they want $149.
Both programs will discover our AP and show the same information that
can be found in our xml files.


===========================================================================
Compatible linux system for development and testing
============================================================================
It is least confusing if the linux computer you will be using has no
more than one Atheros wifi chip in it... you will of course need to have
at least one for testing.
With multiple chips there is confusion about "ath", "wifi" and "br" naming.

On the other hand, a more interesting test controls several Atheros devices,
although these can be virtual devices.

Fedora Core 6 was the original used by Sony.
However, I've been using Kubuntu 7.10 "Gutsy Gibbon" so if you want
to duplicate what i have done, then you should also.
Download the CD from kubuntu.org.
See web page: http://releases.ubuntu.com/kubuntu/7.10/ for information.
Try: wget http://releases.ubuntu.com/kubuntu/7.10/kubuntu-7.10-desktop-i386.iso
or usually better,
btdownloadgui http://releases.ubuntu.com/kubuntu/7.10/kubuntu-7.10-desktop-i386.iso.torrent

In case you want to use some other distribution, keep the following in mind:
Using sony recommended madwifi-0.9.3.1, i had to downgrade kernel
to 2.6.18... but after doing 3way merge to madwifi-0.9.3.3,
kernel 2.6.18 hangs the computer but the 2.6.22 that comes with 
Kubuntu 7.10 works ... except that there are occasional system hangs.
Be prepared to reboot.
Hopefully when we switch to the fusion driver, all will be well.

gcc must be version 4.1.1 or later?!
the gui requires qt4 (4.3.0 or later)

Also need (besides what is in this directory):
The following are debian/ubuntu package names:
* bridge-utils 
* openssh-server (helpful)
* g++ (required for libupnp as well as gui, if you use either)
* Various qt4 packages including perhaps (required for gui):
	* qt4-dev-tools 
	* libqt4-dev (maybe)

============================================================================
Source code histories:
============================================================================

hostapd:
	hostapd-0.5.8
	plus hostapd-0.5.8-sony_r5.7.patch from sony
	plus 3 way merge with hostapd-0.5.9
	plus local changes including:
                -- config files split into per-radio and per-bss
                -- argv command line changed; uses "topology file"
                    now to list the radios, bss's and their config files,
                    plus overrides.
                -- PB and PIN modes of WPS are treated almost the same
                    including 2 minute timeout and WPS IE support.
                -- tiny upnp implementation

wpa_supplicant:
	wpa_supplicant-0.5.8
	plus wpa_supplicant-0.5.8-sony_r5.7.patch from sony
	plus 3 way merge with wpa_supplicant-0.5.9
	plus local changes including:
                -- PB and PIN modes of WPS are treated almost the same
                    including 2 minute timeout, but WPS IE is used
                    for PB only.

wpatalk:
        derived from wpa_supplicant/wpa_cli.c,
        extensively rewritten per local requirements

madwifi.host:
	madwifi-0.9.3.1
	plus madwifi-0.9.3.1-WPS_1.0.patch from sony
	plus 3 way merged with madwifi-0.9.3.3
        saved into original/madwifi-modified.tgz to reduce clutter
        (since you have to login as root to use it and it is easily
        copied and made elsewhere).
        To build it and add drivers for your linux system,
        possibly copy it first, cd into it and do:
            make
            make install
        However, please note that this driver is somewhat unstable and
        may crash your system, more so on some kernel versions than others;
        and may not compile or work with the kernel version of your choice.
            

libupnp:
	libupnp-1.3.1 ? -- not sure that this is right
        plus sony changes
	plus local changes
        saved into original/libupnp-modified.tgz in case we need to revive it.

openssl:
	openssl-0.9.8g plus sony changes
        saved into original/openssl-modified.tgz in case we need to revive it.

WpsNfcLibrary:
        from Sony -- unused

wireless_tools:
        From wireless_tools.29 with local changes for WPS
        (http://www.hpl.hp.com/personal/Jean_Tourrilhes/Linux/wireless_tools.29.tar.gz)

wireshark:
        This is not installed by build program; un-tar from 
        "original" directory and build it yourself, as root:
            ./configure; make; make install
        Source code history is explained in README.atheros in the
        untarred directory... is modified to display WPS info in wifi packets.
        You may need various other packages installed in order to build,
        e.g. libpcap-dev , libgnutls-dev  (depending on distribution).
        At runtime you may need to set LD_LIBRARY_PATH=/usr/local/lib .


============================================================================
Sources of original files
============================================================================

Sources from:
https://www.saice-wpsnfc.bz/index.php
A contact at Sony has verified that this is indeed code from Sony.
wget -r --no-check-certificate https://www.saice-wpsnfc.bz ... but
this takes a long time since it gets the iso image.
So try instead:
wget https://www.saice-wpsnfc.bz/data/NFC631KT_Software_for_Wi-Fi_Protected_Setup_Resource_Packages_Description.pdf
wget https://www.saice-wpsnfc.bz/data/hostapd-0.5.8-sony_r5.7.patch
wget https://www.saice-wpsnfc.bz/data/wpa_supplicant-0.5.8-sony_r5.7.patch
wget https://www.saice-wpsnfc.bz/data/madwifi-0.9.3.1-WPS_1.0.patch
wget https://www.saice-wpsnfc.bz/data/WpsNfcLibrary-1.1.1.tar.gz
wget https://www.saice-wpsnfc.bz/data/NFCKernelDriver-1.0.3.tar.gz

And perhaps (although this applies to the bootable CD image):
wget https://www.saice-wpsnfc.bz/data/NFC631KT_Software_for_Wi-Fi_Protected_Setup_Setup_Guide.pdf

openssl-0.9.8g :
wget http://www.openssl.org/source/openssl-0.9.8g.tar.gz

hostapd, wpa_supplicant:
generally, from http://hostap.epitest.fi/hostapd/
wget http://hostap.epitest.fi/releases/hostapd-0.5.8.tar.gz
wget http://hostap.epitest.fi/releases/wpa_supplicant-0.5.8.tar.gz

madwifi:
generally, madwifi.org
Obtain downloads from sourceforge
e.g.
wget http://superb-west.dl.sourceforge.net/sourceforge/madwifi/madwifi-0.9.3.1.tar.gz


============================================================================
Size
============================================================================

2008-3-20   File sizes for pb42-carrier target:

With WPS but enterprise auth disabled:
/sbin/hostapd -- 452524 (includes upnp)
/sbin/wpa_supplicant -- 317972  (no upnp)
/lib/libwpa_common.so -- 175528 (shared code for above)
/sbin/wpa_talk -- 31756 (for giving dynamic commands)
/lib/libwpa_ctrl.so -- 10728 (used by wpa_talk)

Same as above, but staticly linked with wpa_common library:
/sbin/hostapd -- 603756
/sbin/wpa_supplicant -- 459728
(no libwpa_common.so)

For a minimal case that did not need wpa_supplicant and did not need 
enterprise support, the above hostapd could be used.

With WPS and enterprise auth enabled:
/sbin/hostapd -- 492012 (includes upnp and enterprise auth)
/sbin/wpa_supplicant -- 369568  (enterprise auth, no upnp)
/lib/libwpa_common.so -- 175528 (shared code for above)

Also note optional component
/sbin/apstart -- 14540 (sets up virtual aps and bridging acc. to topology 
file)



============================================================================
============================================================================
