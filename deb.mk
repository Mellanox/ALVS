# All Target
all: alvs

alvs: 
	make CONFIG_ALVS=1 -f pack.mk

alvs-clean:
	make CONFIG_ALVS=1 -f pack.mk deb-distclean

clean: alvs-clean
