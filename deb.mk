ifdef DEBUG
    SUFFIX := _debug
else
    SUFFIX :=
endif

CURRENT_DIR    := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PACKAGE_FOLEDR := $(CURRENT_DIR)/package

deb: deb-distclean
	cd $(PACKAGE_FOLEDR); dpkg-buildpackage -b
ifdef DEBUG
	make -f deb.mk deb-debug
endif

deb-debug:
	# rename package in case of debug
	cd $(CURRENT_DIR)
	$(eval DEB_FILE_NAME=$(shell ls *.deb | cut -d"." -f1-3))
	mv $(DEB_FILE_NAME).deb $(DEB_FILE_NAME)$(SUFFIX).deb
	mv $(DEB_FILE_NAME).changes $(DEB_FILE_NAME)$(SUFFIX).changes

deb-clean:
	rm -rf $(PACKAGE_FOLEDR)/debian/alvs
	rm -f $(PACKAGE_FOLDER)/debian/files
	rm -f $(PACKAGE_FOLDER)/build-stamp
	
deb-distclean: deb-clean
	rm -rf *.deb
	rm -rf *.changes
