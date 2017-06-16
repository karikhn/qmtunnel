Introduction
============

qmTunnel is a free cross-platform open source tunneling software allowing you to
wrap up and tunnel all types of TCP, UDP or named pipe connections through a set
of tunnel software servers.

You may find qmTunnel useful if you need (all features are optional):

* to secure your connection with SSL/TLS;
* to connect to hosts/networks behind NAT/firewall;
* to compress your traffic;
* to detect silent packet drops and disconnections (by enabling heartbeats);
* to allow short-time disconnections between tunnel hosts with no application
  disconnections;
* to add additional authentication level to tunnel hosts;
* to automatically re-establish the tunnel on disconnections (permanent tunnel);
* to establish tunnel only when needed (on demand).

Basically your application client connects to qmTunnel server instead of connecting
directly to application server. Then qmTunnel server makes further connections to
next qmTunner server and the last qmTunnel server in chain connects to your application
server, transparently (for application client and server) transferring all application
data from application client to the application server (and vice versa) and allowing
to secure and tune the connections between qmTunnel servers.

One of the most simple cases can be described by the following figure:

.. image:: doc/source/_static/schema4.png
   :width: 600px

See documentation for more information and use cases.


Architecture
============

qmTunnel consists of 2 modules:

* **qmTunnel-server** — server module which needs to be started on all tunnel hosts
  (at least two).
  It's possible to run qmtunnel-server as GUI application or as background console
  application (use ``-daemon`` command line parameter).

* **qmTunnel-gui** — GUI which connects to qmtunnel-server instances (including remote
  ones) and allows to configure them and create/edit/monitor tunnels.

qmTunnel is a free open source cross-platform application and runs on Linux, Windows
and possibly (haven't tested yet) MacOS.

To build and run qmTunnel, you only need Qt4/Qt5 and OpenSSL libraries.

You can also download binaries for most popular platforms.


Support
=======

qmtunnel is open-source project, which means it's considered to be supported by the
community.

However if you wish to use it in production environment, commercial support is also
available from the author and maintainer of this project. Contact support@qmtunnel.com
for details. This way you can also support the project.


License
=======

qmtunnel is released under GNU General Public License 3.0, with the additional special
exception to link portions of this program with the OpenSSL library.
See LICENSE file for more details.

Copyright (c) 2017 Nikolay N. Karikh (knn@qmtunnel.com)

.. note:: This software also uses Qt, OpenSSL and JSON libraries:

          Copyright (c) 2017 The Qt Company.

          LEGAL NOTICE: This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit (http://www.openssl.org/)

          Copyright (c) 1998-2017 The OpenSSL Project

          Copyright (c) 1995-1998 Eric A. Young (eay@cryptsoft.com), Tim J. Hudson (tjh@cryptsoft.com)

          Copyright (c) 2009 Dave Gamble

          All rights reserved.


