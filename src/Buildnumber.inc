#
# Copied from the Linux Journal from
# "Add an Auto-Incrementing Build-Number to Your Build Process", 2008, July 8th, Mitch Frazier
#
# http://www.linuxjournal.com/content/add-auto-incrementing-build-number-your-build-process

# Create an auto-incrementing build number.
BUILD_NUMBER_LDFLAGS  = -Xlinker --defsym -Xlinker __BUILD_DATE=$$(date +'%Y%m%d')
BUILD_NUMBER_LDFLAGS += -Xlinker --defsym -Xlinker __BUILD_NUMBER=$$(cat $(BUILD_NUMBER_FILE))

# Build number file.  Increment if any object file changes.
$(BUILD_NUMBER_FILE): swspectrometer.o
	@if ! test -f $(BUILD_NUMBER_FILE); then echo 0 > $(BUILD_NUMBER_FILE); fi
	@echo $$(($$(cat $(BUILD_NUMBER_FILE)) + 1)) > $(BUILD_NUMBER_FILE)
