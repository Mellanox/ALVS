ifdef DEBUG
    SUFFIX := _debug
else
    SUFFIX :=
endif

# All Target
all: dp cp

install: all
	tar -czvf alvs$(SUFFIX).tar.gz bin -C install .

install-clean:
	rm -f alvs$(SUFFIX).tar.gz

dp:
	mkdir -p build/src/dp
	mkdir -p build/src/common
	mkdir -p bin
	make -f dp.mk make_dp

cp:
	mkdir -p build/src/cp
	mkdir -p build/src/common
	mkdir -p bin
	make -f cp.mk make_cp

dp-clean:
	make -f dp.mk dp-clean

cp-clean:
	make -f cp.mk cp-clean

clean: dp-clean cp-clean
	rm -rf bin	
	rm -rf build
	
