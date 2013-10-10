Release notes for Hexago's tspc 2.1 - Windows platform
------------------------------------------------------

Requirements
------------

The software was tested with:

- Windows XP


Installing the client
---------------------

The client consists of two distinct software,
which are:

- An ipv6 tunnel driver for windows
- a client software to establish an ipv6 peer
  to peer link with a Migration Broker.

In order to use the client software, you
must first install the tunnel driver from
the Add Hardware wizard in the control panel.

Let the wizard search for new hardware, then
when it timeouts, choose:

- Yes, I have already connected the hardware

Click next and select the last item on the list,

- Add a new hardware device

Click next again and select:

- Install the hardware that i manually select from a list

In the next list, choose 

- Network adapters

Then click on 

- Have disk...

And point the browser that appears to the directory
where you have tunv6.sys and tunv6.inf.


* Very Important! *

To install the tsp client, edit tspc.conf
and make tsp_dir= point to the directory
where you installed it. This is needed
by the client software to find its templates.


Running the client
------------------

The client will not run at all if it cant find
tspc.conf.

The default tspc.conf installed with the client
should be sufficient to get a connection up and
running. However, you might want to edit tspc.conf
to tailor it to your needs. Each option is documented
directly in tspc.conf.

The client has multiple verbosity levels that are
activated with the -v command line option. The more v
you put, the more loquacious it will get. IE, tspc -vvv
is much more explicit than tspc -v.

If you use the nat traversal feature (V6UDPV4),
pay particular attention to the keepalive and 
keepalive_interval options in the configuration file.

Your UDP tunnel will not stay up if you turn keepalive
off or if the keepalive_interval is too short. However,
30 seconds should give sufficient leverage to cover pretty
much anything.












