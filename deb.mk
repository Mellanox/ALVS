# All Target
all: alvs atc

alvs: 
	make CONFIG_ALVS=1 -f pack.mk

alvs-clean:
	make CONFIG_ALVS=1 -f pack.mk deb-distclean

atc: 
	make CONFIG_ATC=1 -f pack.mk

atc-clean:
	make CONFIG_ATC=1 -f pack.mk deb-distclean


clean: alvs-clean atc-clean
