# Install at least:
# cython
# pyqt5-dev
# sip-dev

SIPFLAGS = $(shell python3 -c "import PyQt5.QtCore; print(PyQt5.QtCore.PYQT_CONFIGURATION['sip_flags'])")

ALL: wrap_dolphin.cpp sipsip_dolphincmodule.cpp

wrap_dolphin.cpp wrap_dolphin.h: wrap_dolphin.pyx *.pxd
	cython --cplus wrap_dolphin.pyx

sipsip_dolphincmodule.cpp sipAPIsip_dolphin.h: dolphin.sip
	sip -c . -I/usr/share/sip/PyQt5 $(SIPFLAGS) dolphin.sip

