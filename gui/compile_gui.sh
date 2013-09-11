#!/bin/bash
# On Ubunty Jaunty, this requires the 'pyqt-tools' package
# Other packages for Qt4: 
#
# $ sudo aptitude install pyqt4-dev-tools  qt4-designer pyqt-tools
#

pyuic4 -x qt4designer.ui > generatedGUI_Testable.py
pyuic4 qt4designer.ui > generatedGUI.py

