#!/usr/bin/env python
"""A Python and Qt4 user interface for running the software spectrometer
   (C) 2009 Jan Wagner, Guifre Molera, Metsahovi Radio Observatory
"""

import os
import sys
import subprocess

from PyQt4 import QtGui, QtCore, Qt
from generatedGUI import Ui_Dialog
from iniLib import *

import time
import datetime
from datetime import date

# ---------------------------------------------------------------------------------------------------------------------------------
# ---- GLOBALS/SHARED
# ---------------------------------------------------------------------------------------------------------------------------------

keys = dict()
guikeys = dict()

# ---------------------------------------------------------------------------------------------------------------------------------
# ---- HELPERS
# ---------------------------------------------------------------------------------------------------------------------------------

def chkboxToStr(ckb):
	if ckb.isChecked():
		return 'Yes'
	return 'No'

def strToChkbox(chkbox,str):
	if (str.lower() == 'yes'):
		chkbox.setChecked(1)
	else:
		chkbox.setChecked(0)

def strToTbox(txtbox,str):
	txtbox.setText(QtGui.QApplication.translate("Dialog", str, None, QtGui.QApplication.UnicodeUTF8))

def strToCbox(combo,str):
	sidx = combo.findText(str, QtCore.Qt.MatchFixedString)
	combo.setCurrentIndex(sidx)

# ---------------------------------------------------------------------------------------------------------------------------------
# ---- MAIN GUI
# ---------------------------------------------------------------------------------------------------------------------------------

class MainWindow(QtGui.QMainWindow):

	requiredKeys = [ 'FFTpoints', 'FFTIntegrationTimeSec', 'FFToverlapFactor', 'WindowType', 'BandwidthHz', 'PCalOffsetHz', \
		'SourceFormat', 'SourceSkipSeconds', 'BitsPerSample', 'SourceChannels', 'UseFile1Channel', 'ExtractPCal', 'PlotProgress', \
		'DoCrossPolarization', 'UseFile1Channel', 'UseFile2Channel', 'SinkFormat', 'MaxSourceBufferMB', 'NumCPUCores', \
		'BaseFilename1', 'BaseFilename2' ]
	prevInputINI = '~/.'
	prevInputFile1 = '~/.'
	prevInputFile2 = '~/.'

	def __init__(self,parent=None):
		QtGui.QDialog.__init__(self)

		# Set up the user interface from Designer.
		self.ui = Ui_Dialog()
		self.ui.setupUi(self)

		# Here starts the link buttons functions. The ones below DO work.
		self.connect(self.ui.cmdSelectINI,QtCore.SIGNAL("clicked()"),self.selectLoadIniFile)
		self.connect(self.ui.cmdSelectFile1,QtCore.SIGNAL("clicked()"),self.selectFile1)
		self.connect(self.ui.cmdSelectFile2,QtCore.SIGNAL("clicked()"),self.selectFile2)
		self.connect(self.ui.cmdProcess,QtCore.SIGNAL("clicked()"),self.doProcess)	
		self.connect(self.ui.cmdCancel,QtCore.SIGNAL("clicked()"),self.doCancel)
		self.connect(self.ui.cmdQuit, QtCore.SIGNAL("clicked()"), self,QtCore.SLOT("close()"))

		# Listen for combo box changes
		self.connect(self.ui.cmbSourceFormat, QtCore.SIGNAL("currentIndexChanged(QString)"), self.eventSourceformatChanged)

		# Center on screen
		screen = QtGui.QDesktopWidget().screenGeometry()
		size =  self.geometry()
		self.move((screen.width()-size.width())/2, (screen.height()-size.height())/2)

		# Load our own previous settings
		guikeys = readIniFile('spectrometerGUI.ini')
		if (guikeys == None):
			guikeys = dict()
		if ('LastTemplateINI' in guikeys):
			strToTbox(self.ui.txtINITemplate, guikeys['LastTemplateINI'])
			# self.loadIniFile(guikeys['LastTemplateINI'])
		if ('LastInput1' in guikeys):
			strToTbox(self.ui.txtInputFile1, guikeys['LastInput1'])
		if ('LastOutputINI' in guikeys):
			strToTbox(self.ui.txtOutputINI, guikeys['LastOutputINI'])
			self.loadIniFile(guikeys['LastOutputINI'])

	def selectLoadIniFile(self):	
		# Show dialog for user to select an INI file
		fileName = Qt.QFileDialog.getOpenFileName(self,self.tr("Load template INI"), self.prevInputINI, self.tr("INI (*.ini)"))
		if fileName.isEmpty():
			return
		guikeys['LastTemplateINI'] = fileName
		self.loadIniFile(fileName)

	def loadIniFile(self,fileName):
		self.prevInputINI = fileName
		self.ui.txtINITemplate.setText(QtGui.QApplication.translate("Dialog", fileName, None, QtGui.QApplication.UnicodeUTF8))
		# Read the INI file
		keys = readIniFile(fileName)
		if (keys == None):
			print 'File ' + str(fileName) + ' does not exist or contains no INI keys.'
			return
		result = checkValidINI(keys, self.requiredKeys)
		if (result != 0):
			print 'INI seems to lack keys listed above. Please check your INI file.'
			return
		# Set text fields
		strToTbox(self.ui.txtFFTPoints, keys['FFTpoints'])
		strToTbox(self.ui.txtFFTIntegrationTimeSec, keys['FFTIntegrationTimeSec'])
		strToTbox(self.ui.txtBandwidth, keys['BandwidthHz'])
		strToTbox(self.ui.txtPCalOffset, keys['PCalOffsetHz'])
		strToTbox(self.ui.txtSkipSeconds, keys['SourceSkipSeconds'])
		strToTbox(self.ui.txtOutputFile1, keys['BaseFilename1'])
		# Set spin boxes
		self.ui.spinFFTOverlapFactor.setValue(int(keys['FFToverlapFactor']))
		self.ui.spinChannelFile1.setValue(int(keys['UseFile1Channel']))
		self.ui.spinChannelFile2.setValue(int(keys['UseFile2Channel']))
		self.ui.spinNumCPUCores.setValue(int(keys['NumCPUCores']))
		self.ui.spinMaxBufSize.setValue(int(keys['MaxSourceBufferMB']))
		# Set combo boxes
		strToCbox(self.ui.cmbFFTWindowing, keys['WindowType'])
		strToCbox(self.ui.cmbSourceFormat, keys['SourceFormat'])
		strToCbox(self.ui.cmbBitsPerSample, keys['BitsPerSample'])
		strToCbox(self.ui.cmbSourceChannels, keys['SourceChannels'])
		strToCbox(self.ui.cmbOutputFormat, keys['SinkFormat'])
		# Set checkboxes
		strToChkbox(self.ui.chkPCal, keys['ExtractPCal'])
		strToChkbox(self.ui.chkCrossPol, keys['DoCrossPolarization'])
		strToChkbox(self.ui.chkPlotting, keys['PlotProgress'])

		# Set some "smart" defaults
		strToTbox(self.ui.txtOutputINI, str(self.ui.txtInputFile1.text()) + '.spec.ini')

	def doProcess(self):
		# Get text fields
		keys['FFTpoints'] = self.ui.txtFFTPoints.text()
		keys['FFTIntegrationTimeSec'] = self.ui.txtFFTIntegrationTimeSec.text()
		keys['BandwidthHz'] = self.ui.txtBandwidth.text()
		keys['PCalOffsetHz'] = self.ui.txtPCalOffset.text()
		keys['SourceSkipSeconds'] = self.ui.txtSkipSeconds.text()
		keys['BaseFilename1'] = self.ui.txtOutputFile1.text()
		keys['BaseFilename2'] = 'none' # TODO
		# Get spin boxes
		keys['FFToverlapFactor'] = self.ui.spinFFTOverlapFactor.text()
		keys['UseFile1Channel'] = self.ui.spinChannelFile1.text()
		keys['UseFile2Channel'] = self.ui.spinChannelFile2.text()
		keys['NumCPUCores'] = self.ui.spinNumCPUCores.text()
		keys['MaxSourceBufferMB'] = self.ui.spinMaxBufSize.text()
		# Get combo boxes
		keys['WindowType'] = self.ui.cmbFFTWindowing.currentText()
		keys['SourceFormat'] = self.ui.cmbSourceFormat.currentText()
		keys['BitsPerSample'] = self.ui.cmbBitsPerSample.currentText()
		keys['SourceChannels'] = self.ui.cmbSourceChannels.currentText()
		keys['SinkFormat'] = self.ui.cmbOutputFormat.currentText()
		# Get checkboxes
		keys['ExtractPCal'] = chkboxToStr(self.ui.chkPCal)
		keys['DoCrossPolarization'] = chkboxToStr(self.ui.chkCrossPol)
		keys['PlotProgress'] = chkboxToStr(self.ui.chkPlotting)

		# Write the INI
		result = checkValidINI(keys, self.requiredKeys) # kind of a double-check...
		if (result != 0):
			print 'Key array seems to lack keys above, bug in the spectrometerGUI.py code!'
			return
		iniName = self.ui.txtOutputINI.text()
		if iniName.isEmpty():
			iniName = 'inifile.ini'
		writeIniFile(keys, 'Spectrometer', iniName)

		# Get files to process
		inputName = self.ui.txtInputFile1.text()
		inputName2 = self.ui.txtInputFile2.text()
		if (inputName.isEmpty() or keys['BaseFilename1'].isEmpty()):
			print 'Input or output files not specified!'
			return

		# Write our own INI to memorize old settings
		guikeys['LastOutputINI'] = iniName
		guikeys['LastInput1'] = inputName
		writeIniFile(guikeys, 'SpectrometerGUI', 'spectrometerGUI.ini')
		global child

		# Start processing
		now = datetime.datetime.utcnow()
		scriptname = os.path.basename(str(inputName)) + now.strftime("%Yy%mm%dd-%Hh%Mm%Ss") + '.log'
		command = '/usr/local/bin/swspectrometer ' + iniName + ' ' + inputName
		print 'Command = ' + command + '   scriptname = ' + scriptname
		try:
			# subprocess.Popen(["/usr/local/bin/swspectrometer", iniName, inputName, inputName2])
			# subprocess.Popen(['/usr/bin/script', '-a', '-c', '/bin/echo test', 'script.log'])
			child = subprocess.Popen(['/usr/bin/script', '-a', '-c', command, scriptname])
		except:
			print 'Could not launch /usr/bin/script for /usr/local/bin/swspectrometer!'

	def doCancel(self):
		if not child.poll():
			print 'spawning a killall -SIGINT to swspectrometer'
			os.spawnlp(os.P_WAIT, 'killall', 'killall', '-SIGINT', 'swspectrometer')
		else:
			print 'Swspectrometer is not running\n'

	def selectFile1(self):
		fileName = Qt.QFileDialog.getOpenFileName(self,self.tr("Select input file 1"), self.prevInputFile1, self.tr("All Files (*);;EVN (*.pcevn)"))
		if not fileName.isEmpty():
			strToTbox(self.ui.txtInputFile1, fileName)
		self.prevInputFile1 = fileName
		
	def selectFile2(self):
		fileName = Qt.QFileDialog.getOpenFileName(self,self.tr("Select input file 2"), self.prevInputFile2, self.tr("All Files (*);;EVN (*.pcevn)"))
		if not fileName.isEmpty():
			strToTbox(self.ui.txtInputFile2, fileName)
		self.prevInputFile2 = fileName

	def eventSourceformatChanged(self):
		# New format selected, apply some sane defaults
		nformat = str(self.ui.cmbSourceFormat.currentText())
		if (nformat[0:4].lower() == 'vlba' or nformat[0:4].lower() == 'mkiv'):
			nbits = int(nformat[-1])
			nchannels = int(nformat[-3])
			if (nchannels>0 and nbits>0):
				keys['BitsPerSample'] = str(nbits)
				keys['SourceChannels'] = str(nchannels)
				strToCbox(self.ui.cmbBitsPerSample, keys['BitsPerSample'])
				strToCbox(self.ui.cmbSourceChannels, keys['SourceChannels'])
		if (nformat[0:4].lower() == 'ibob'):
				keys['BitsPerSample'] = str(8)
				keys['SourceChannels'] = str(1)
				strToCbox(self.ui.cmbBitsPerSample, keys['BitsPerSample'])
				strToCbox(self.ui.cmbSourceChannels, keys['SourceChannels'])
		if (nformat[0:5].lower() == 'maxim'):
				# There are two Maxim 12-bit ADC to LVDS Eval Kit formats: 1) 2ch * 12/16bit, 2) 4ch * 8bit
				# We default to the latter one...
				keys['BitsPerSample'] = str(8)
				keys['SourceChannels'] = str(4)
				strToCbox(self.ui.cmbBitsPerSample, keys['BitsPerSample'])
				strToCbox(self.ui.cmbSourceChannels, keys['SourceChannels'])

# ---------------------------------------------------------------------------------------------------------------------------------
# ---- LAUNCH
# ---------------------------------------------------------------------------------------------------------------------------------

"""Launch the gui dialog"""
if __name__ == "__main__":
	app = QtGui.QApplication(sys.argv)
	window = MainWindow()
	ui = Ui_Dialog()
	window.show()
	sys.exit(app.exec_())
