ifdef DEBUG
    export CP_DEBUG=yes
    export DP_DEBUG=yes
    export CLI_APP_DEBUG=yes
    PREFIX := /debug
else
    PREFIX := 
endif

# All Target
all: alvs atc

alvs: alvs_dp alvs_cp alvs_cli

atc: atc_dp atc_cp atc_cli

alvs_dp:
	# Prepare objects folder
	mkdir -p build/alvs/src/dp
	mkdir -p bin$(PREFIX)
	
	# call specific make 
	CONFIG_ALVS=1 make -f dp.mk make_dp

alvs_cli:
	# Prepare objects folder
	mkdir -p build/alvs/src/common
	mkdir -p build/alvs/src/cli
	mkdir -p bin$(PREFIX)
	
	# call specific make
	CONFIG_ALVS=1 make -f cli.mk make_cli

alvs_cp:
	# Prepare objects folder
	mkdir -p build/alvs/src/common
	mkdir -p build/alvs/src/cp
	mkdir -p bin$(PREFIX)
	
	# call specific make
	CONFIG_ALVS=1 make -f cp.mk make_cp


atc_dp:
	# Prepare objects folder
	mkdir -p build/atc/src/dp
	mkdir -p bin$(PREFIX)

	# call specific make
	make CONFIG_ATC=1 -f dp.mk make_dp

atc_cli:
	# Prepare objects folder
	mkdir -p build/atc/src/common
	mkdir -p build/atc/src/cli
	mkdir -p bin$(PREFIX)
	
	# call specific make
	CONFIG_ATC=1 make -f cli.mk make_cli

atc_cp:
	# Prepare objects folder
	mkdir -p build/atc/src/common
	mkdir -p build/atc/src/cp
	mkdir -p bin$(PREFIX)

	# call specific make
	make CONFIG_ATC=1 -f cp.mk make_cp


dp-clean:
	make -f dp.mk dp-clean


cp-clean:
	make -f cp.mk cp-clean
	
cli-clean:
	make -f cli.mk cli-clean

clean: dp-clean cp-clean cli-clean
	rm -rf bin	
	rm -rf build
	
deb:
	make -f deb.mk
