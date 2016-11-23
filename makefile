ifdef DEBUG
    export CP_DEBUG=yes
    export DP_DEBUG=yes
    PREFIX := /debug
else
    PREFIX := 
endif

# All Target
all: alvs

alvs: alvs_dp alvs_cp

alvs_dp:
	mkdir -p build/alvs/src/dp
	mkdir -p bin$(PREFIX)
	CONFIG_ALVS=1 make -f dp.mk make_dp


alvs_cp:
	mkdir -p build/alvs/src/cp
	mkdir -p bin$(PREFIX)
	CONFIG_ALVS=1 make -f cp.mk make_cp


dp-clean:
	make -f dp.mk dp-clean


cp-clean:
	make -f cp.mk cp-clean


clean: dp-clean cp-clean
	rm -rf bin	
	rm -rf build
