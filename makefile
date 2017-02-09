.PHONY : prepare distclean help

COMPILE_ROOT_ALVS=/mswg/release/nps/solutions/chroot_env/debian_4.2_root
COMPILE_ROOT_TC=/mswg/release/nps/solutions/chroot_env/debian_4.8_root
ALVS=$(COMPILE_ROOT_ALVS)/home/ALVS
TC=$(COMPILE_ROOT_TC)/home/ALVS
ALVS_SRC=.
TC_SRC=.
EZDK_DIR_SRC=$(shell readlink EZdk)
EZDK_DIR_TC=$(COMPILE_ROOT_TC)$(EZDK_DIR_SRC)
EZDK_DIR_ALVS=$(COMPILE_ROOT_ALVS)$(EZDK_DIR_SRC)
	
all: alvs atc
alvs: alvs_dp alvs_cp alvs_cli
atc: atc_dp atc_cp atc_cli

alvs_dp alvs_cli alvs_cp:
	chroot $(COMPILE_ROOT_ALVS) make -C /home/ALVS -f  make.mk  $@
atc_dp atc_cp atc_cli:
	chroot  $(COMPILE_ROOT_TC)  make -C /home/ALVS -f  make.mk  $@
dp-clean cp-clean cli-clean clean: 
	make -f make.mk $@
deb: deb-alvs deb-tc
deb-alvs:
	chroot $(COMPILE_ROOT_ALVS)  make -C /home/ALVS -f deb.mk alvs
deb-atc:
	chroot $(COMPILE_ROOT_TC)  make -C /home/ALVS -f deb.mk  atc
deb-clean: deb-alvs-clean deb-tc-clean
deb-alvs-clean:
	make -f deb.mk alvs-clean
deb-tc-clean:
	make -f deb.mk atc-clean



prepare:distclean

	@sudo setcap cap_sys_chroot+ep /usr/sbin/chroot 
	@sudo mount --bind $(ALVS_SRC) $(ALVS)
	@sudo mount --bind $(TC_SRC) $(TC)
	@sudo mount --bind $(EZDK_DIR_SRC) $(EZDK_DIR_ALVS)  
	@sudo mount --bind $(EZDK_DIR_SRC) $(EZDK_DIR_TC)  


distclean:

	@sudo umount -l $(ALVS) &> /dev/null ;true 
	@sudo umount -l $(TC) &> /dev/null ;true 
	@sudo umount -l $(EZDK_DIR_ALVS) &> /dev/null ;true 
	@sudo umount -l $(EZDK_DIR_TC) &> /dev/null ;true 


help:

	@echo " *********************************** "
	@echo "Usage: "
	@echo " "
	@echo "make prepare : prepare environemnt  - creating hard links ( requires sudo permission )"
	@echo "make distclean: clean environment - remove hard links ( requires sudo permission )"
	@echo "make help : show this message " 
	@echo "make <any other targets> : call make.mk with that targets "
	@echo "To directly call old makefile without chrooting - use make -f make.mk <targets> " 
	@echo ""
	@echo " *********************************** "	


