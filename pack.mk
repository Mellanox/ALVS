ifdef DEBUG
	# use suffix to rename the package
	SUFFIX := _debug
	# use prefix to locate relevant bins(debug/non-debug)
	PREFIX := debug\/
else
	SUFFIX :=
	PREFIX :=
endif

ifdef CONFIG_ALVS
	APP_NAME := alvs
endif

CURRENT_DIR    := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PACKAGE_FOLEDR := $(CURRENT_DIR)/package
APP_FOLEDR := $(PACKAGE_FOLEDR)/$(APP_NAME)
DEB_FOLDER := $(APP_FOLEDR)/debian
APP_INSTALL := $(DEB_FOLDER)/install
deb-app:deb-distclean deb-bins-loc deb-build deb-pack-conv

deb-bins-loc:
	# update install file with the relevant bins location
	sed -E "s/\/bin\/(debug\/)*/\/bin\/$(PREFIX)/" < $(APP_INSTALL) > $(APP_INSTALL).tmp
	mv -f $(APP_INSTALL).tmp $(APP_INSTALL)

deb-build: 
	cd $(APP_FOLEDR); dpkg-buildpackage -b

deb-clean:
	rm -rf $(DEB_FOLDER)/$(APP_NAME)
	rm -f $(DEB_FOLDER)/files
	
deb-distclean: deb-clean
	rm -rf $(APP_NAME)*.deb
	rm -rf $(PACKAGE_FOLEDR)/$(APP_NAME)*.changes

deb-pack-conv:
	# rename package in case of debug
	$(eval DEB_FILE_NAME=$(shell ls $(PACKAGE_FOLEDR)/$(APP_NAME)*.deb))
	rename 's/\.deb/$(SUFFIX)\.deb/' $(DEB_FILE_NAME)
	mv -f $(PACKAGE_FOLEDR)/$(APP_NAME)*.deb $(CURRENT_DIR)
