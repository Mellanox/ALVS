RM := rm -rf
ENV_BASE := ../..

EZDK_BASE := $(shell readlink $(ENV_BASE)/EZdk)
PATH := $(PATH):$(abspath $(EZDK_BASE)/ldk/toolchain/bin)

CP_BIN := bin/alvs_cp

CP_C_SRCS := 
CP_OBJS := 
CP_C_DEPS := 

CP_INC := -I/usr/include/libnl3 -Isrc/common -Isrc/cp -I$(EZDK_BASE)/dpe/dp/include -I$(EZDK_BASE)/cpe/env/include -I$(EZDK_BASE)/cpe/dev/include -I$(EZDK_BASE)/cpe/cp/include -I$(EZDK_BASE)/cpe/agt/agt-cp/include -I$(EZDK_BASE)/cpe/agt/agt/include

CP_C_SRCS = $(shell ls src/cp/*.c)
CP_OBJS = $(patsubst %.c,build/%.o,$(CP_C_SRCS)) 
CP_C_DEPS = $(patsubst %.c,build/%.d,$(CP_C_SRCS))

CP_LIBS := -l:EZagt_linux_x86_64.a -l:EZagt-cp_linux_x86_64.a -l:EZcp_linux_x86_64.a -l:EZdev_linux_x86_64.a -l:EZenv_linux_x86_64.a -l:libjsonrpcc.a -l:libev.a

ifndef DEBUG
CP_C_FLAGS := -O3
else
CP_C_FLAGS := -O0 -g3
endif
CP_C_FLAGS += -DLITTLE_ENDIAN

# Tool invocations
make_cp: $(CP_OBJS) $(USER_CP_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -lpthread -lnl-3 -lnl-route-3 -L$(EZDK_BASE)/cpe/agt/agt/lib/ -L$(EZDK_BASE)/cpe/agt/agt-cp/lib -L$(EZDK_BASE)/cpe/cp/lib/ -L$(EZDK_BASE)/cpe/dev/lib -L$(EZDK_BASE)/cpe/env/lib -L$(EZDK_BASE)/cpe/jsonrpc-c/install/linux_x86_64/lib -L$(EZDK_BASE)/cpe/libev/install/linux_x86_64/lib/ -pthread -lm $(CP_INC) -o "$(CP_BIN)" $(CP_OBJS) $(USER_CP_OBJS) $(CP_LIBS)
	@echo 'Finished building target: $@'
	@echo ' '


build/%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc $(CP_INC) $(CP_C_FLAGS) -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

# Other Targets
cp-clean:
	-$(RM) $(CP_OBJS) $(CP_C_DEPS) $(CP_BIN)
	-@echo ' '

.PHONY: make_cp cp-clean 
.SECONDARY:
