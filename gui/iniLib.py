#!/usr/bin/env python

verbose=0

def readIniFile(filename):
	entries = dict()
	try:
		fd = open(filename)
	except:
		print 'The file ' + filename + ' could not be opened'
		return
	while 1:
		line = fd.readline()
		if not line:
			break
		line.expandtabs(1)
		line = line.lstrip().rstrip()
		line = line.replace('  ', ' ')
		line = line.replace('  ', ' ')
		line = line.replace(' ', '')
		if len(line)<=1:
			continue

		# [Segment]
		segmentIdx = line.find('[')
		if line[0] == '[':
			if verbose==1:
				print 'INI contained segment: ', line
			continue

		# #Comment
		# something=other #a comment on something
		commentIdx = line.find('#')
		if commentIdx >= 0:
			if commentIdx == 0:
				continue
			else:
				(line,ignored) = line.rsplit('#')
		if len(line)<=1:
			continue

		# interpret
		(key,value) = line.rsplit('=')
		if len(key)>0:
			entries[str(key)] = str(value);
		if verbose==1:
			print line
	return entries

def writeIniFile(dictentries, segmentname, filename):
	try:
		fd = open(filename, 'wt')
	except:
		print 'The output file ' + str(filename) + ' could not be opened!'
		return
	line = '[' + segmentname + ']\n'
	fd.write(line)
	for key in dictentries:
#		line = key + '=' + dictentries[key] + '\n'
		line = "%s = %s \n" % (key,dictentries[key])
		fd.write(line)

def checkValidINI(keylist, required):
        success = 0
        for r in required:
                if r not in keylist:
                        print 'INI does not contain a setting for ' + r
                        success = -1
        return success
