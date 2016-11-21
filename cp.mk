RM := rm -rf
ENV_BASE := ./

EZDK_BASE := $(shell readlink $(ENV_BASE)/EZdk)
PATH := $(PATH):$(abspath $(EZDK_BASE)/ldk/toolchain/bin)

CP_INC := -I/usr/include/libnl3 -Isrc/common -Isrc/cp -I$(EZDK_BASE)/dpe/dp/include -I$(EZDK_BASE)/cpe/env/include -I$(EZDK_BASE)/cpe/dev/include -I$(EZDK_BASE)/cpe/cp/include -I$(EZDK_BASE)/cpe/agt/agt-cp/include -I$(EZDK_BASE)/cpe/agt/agt/include

CP_C_SRCS = src/cp/error_names.c \
			src/cp/log.c \
			src/cp/nw_api.c \
			src/cp/nw_db.c \
			src/cp/sqlite3.c \
			src/cp/cfg.c \
			src/cp/infrastructure.c \
			src/cp/infrastructure_utils.c \
			src/cp/nw_cp_init.c \
			src/cp/main.c \
			src/cp/nw_db_manager.c
ifdef CONFIG_ALVS
CP_C_SRCS += src/cp/alvs_db.c \
			 src/cp/alvs_db_manager.c \
			 src/cp/alvs_cp_init.c
endif

ifdef CONFIG_ALVS
	APP_NAME := alvs
else
	APP_NAME := nw
endif

CP_OBJS = $(patsubst %.c,build/$(APP_NAME)/%.o,$(CP_C_SRCS)) 
CP_C_DEPS = $(patsubst %.c,build/$(APP_NAME)/%.d,$(CP_C_SRCS))

CP_LIBS := -l:EZagt_linux_x86_64.a -l:EZagt-cp_linux_x86_64.a -l:EZcp_linux_x86_64.a -l:EZdev_linux_x86_64.a -l:EZenv_linux_x86_64.a -l:libjsonrpcc.a -l:libev.a -ldl

CP_C_FLAGS := -DALVS_LITTLE_ENDIAN -Werror -Wall -Wextra

ifdef CP_DEBUG
CP_C_FLAGS += -O0 -g3
SUFFIX := _debug
else
CP_C_FLAGS += -O3 -DNDEBUG
SUFFIX := 
endif

ifdef CONFIG_ALVS
	CP_C_FLAGS += -DCONFIG_ALVS
endif

# set bin path/name
CP_BIN := bin/
ifdef CONFIG_ALVS
	CP_BIN := $(CP_BIN)alvs
else
	CP_BIN := $(CP_BIN)nw
endif
CP_BIN := $(CP_BIN)_daemon
ifdef SIM
	CP_C_FLAGS += -DEZ_SIM
	CP_BIN := $(CP_BIN)_sim
endif
CP_BIN := $(CP_BIN)$(SUFFIX)

# Tool invocations
make_cp: $(CP_OBJS) $(USER_CP_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -lpthread -lnl-3 -lnl-route-3 -lnl-genl-3 -L$(EZDK_BASE)/cpe/agt/agt/lib/ -L$(EZDK_BASE)/cpe/agt/agt-cp/lib -L$(EZDK_BASE)/cpe/cp/lib/ -L$(EZDK_BASE)/cpe/dev/lib -L$(EZDK_BASE)/cpe/env/lib -L$(EZDK_BASE)/cpe/jsonrpc-c/install/linux_x86_64/lib -L$(EZDK_BASE)/cpe/libev/install/linux_x86_64/lib/ -pthread -lm $(CP_INC) -o "$(CP_BIN)" $(CP_OBJS) $(USER_CP_OBJS) $(CP_LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

build/$(APP_NAME)/%.o: %.c
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
