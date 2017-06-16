.. _BuildFromSource:

Building from source
====================

To build qmtunnel, you need Qt >= 4.8 and OpenSSL <= 1.0.2 installed.

.. note:: If you wish, you can build them from source too:

          * Qt: https://www.qt.io/download-open-source/
          * OpenSSL: https://www.openssl.org/source/

RedHat/CentOS (7)
*****************

1. Install C++ development tools, Qt and OpenSSL::

    sudo yum group install "Development Tools"
    sudo yum install qt5-qtbase-devel openssl-devel

2. Download sources::

    git clone https://github.com/karikhn/qmtunnel.git

3. Build::

    cd qmtunnel/src/server
    qmake-qt5
    make
    cd ../gui
    qmake-qt5
    make

4. Binaries will be created in qmtunnel/bin directory.


Ubuntu (14.04 LTS)
******************

1. Install C++ development tools, Qt and OpenSSL::

    sudo apt-get install build-essential git qtbase5-dev libssl-dev

2. Download sources::

    git clone https://github.com/karikhn/qmtunnel.git

3. Build::

    cd qmtunnel/src/server
    qmake -qt=5
    make
    cd ../gui
    qmake -qt=5
    make

4. Binaries will be created in qmtunnel/bin directory.

Windows XP and later (32 bit)
*****************************

1. Install Qt 5.6 with MinGW:

   http://download.qt.io/official_releases/qt/5.6/5.6.2/qt-opensource-windows-x86-mingw492-5.6.2.exe

2. Install OpenSSL 1.0.2:

   http://slproweb.com/download/Win32OpenSSL-1_0_2L.exe

3. Update PATH environment variable to include:

   * C:\\Qt\\Qt5.6.2\\5.6\\mingw49_32\\bin
   * C:\\Qt\\Qt5.6.2\\Tools\\mingw492_32\\bin

4. Get the latest qmtunnel sources from GitHub:

   [link goes here]

5. Cd to qmtunnel directory and run::

    cd src\gui
    qmake
    mingw32-make
    cd ..\server
    qmake
    mingw32-make

6. Copy the following files to bin directory where qmtunnel-\*.exe is located:

   * C:\\OpenSSL-Win32\\bin\\libeay32.dll
   * C:\\OpenSSL-Win32\\bin\\ssleay32.dll
   * C:\\OpenSSL-Win32\\bin\\msvcr120.dll
   * C:\\Qt\\Qt5.6.2\\5.6\\mingw49_32\\bin\\Qt5Core.dll
   * C:\\Qt\\Qt5.6.2\\5.6\\mingw49_32\\bin\\Qt5Gui.dll
   * C:\\Qt\\Qt5.6.2\\5.6\\mingw49_32\\bin\\Qt5Network.dll
   * C:\\Qt\\Qt5.6.2\\5.6\\mingw49_32\\bin\\Qt5Widgets.dll
   * C:\\Qt\\Qt5.6.2\\5.6\\mingw49_32\\bin\\libgcc_s_dw2-1.dll
   * C:\\Qt\\Qt5.6.2\\5.6\\mingw49_32\\bin\\libstdc++-6.dll
   * C:\\Qt\\Qt5.6.2\\5.6\\mingw49_32\\bin\\libwinpthread-1.dll
   * Directory C:\\Qt\\Qt5.6.2\\5.6\\mingw49_32\\plugins\\platforms
     (actually only need platforms\\qwindows.dll)


NEED HELP?
**********
E-mail: support@qmtunnel.com

.. note:: Please be aware that individual-case direct technical support is delivered on a commercial basis.

