RM := rm -rf
ENV_BASE := ./

EZDK_BASE := $(shell readlink $(ENV_BASE)/EZdk)
#PATH := $(PATH):$(abspath $(EZDK_BASE)/ldk/toolchain/bin)

CP_INC := -I/usr/include/libnl3 -Isrc/common -Isrc/cp -I$(EZDK_BASE)/dpe/dp/include -I$(EZDK_BASE)/cpe/env/include -I$(EZDK_BASE)/cpe/dev/include -I$(EZDK_BASE)/cpe/cp/include -I$(EZDK_BASE)/cpe/agt/agt-cp/include -I$(EZDK_BASE)/cpe/agt/agt/include

CP_C_SRCS = src/cp/nw_db_manager.c \
			src/cp/error_names.c \
			src/cp/log.c \
			src/cp/nw_ops.c \
			src/cp/nw_api.c \
			src/cp/nw_db.c \
			src/cp/sqlite3.c \
			src/cp/cfg.c \
			src/cp/infrastructure.c \
			src/cp/infrastructure_utils.c \
			src/cp/nw_init.c \
			src/cp/main.c \
			src/cp/version.c \
			src/cp/cli_manager.c \
			src/common/cli_common.c \
			src/cp/index_pool.c

ifdef CONFIG_ATC
CP_C_SRCS += src/cp/tc_api.c \
			 src/cp/tc_flower.c \
			 src/cp/tc_init.c \
			 src/cp/tc_manager.c \
			 src/cp/tc_cli_manager.c \
			 src/cp/tc_db.c \
			 src/cp/tc_action.c \
			 src/common/tc_common_utils.c \
			 src/common/tc_linux_utils.c
endif

ifdef CONFIG_ALVS
CP_C_SRCS += src/cp/alvs_db.c \
			 src/cp/alvs_db_manager.c \
			 src/cp/alvs_cli_manager.c \
			 src/cp/alvs_init.c
endif

ifdef CONFIG_ATC
	APP_NAME := atc
else ifdef CONFIG_ALVS
	APP_NAME := alvs
else
	APP_NAME := nw
endif

CP_OBJS = $(patsubst %.c,build/$(APP_NAME)/%.o,$(CP_C_SRCS)) 
CP_C_DEPS = $(patsubst %.c,build/$(APP_NAME)/%.d,$(CP_C_SRCS))

CP_LIBS := -l:EZagt_linux_x86_64.a -l:EZagt-cp_linux_x86_64.a -l:EZcp_linux_x86_64.a -l:EZdev_linux_x86_64.a -l:EZenv_linux_x86_64.a -l:libjsonrpcc.a -l:libev.a -ldl -lpthread -lnl-3 -lnl-route-3 -lnl-genl-3 -lm

CP_C_FLAGS := -DNPS_LITTLE_ENDIAN -Werror -Wall -Wextra

ifdef CP_DEBUG
CP_C_FLAGS += -O0 -g3
PREFIX := debug/
else
CP_C_FLAGS += -O3 -DNDEBUG
PREFIX := 
endif

ifdef CONFIG_ALVS
	CP_C_FLAGS += -DCONFIG_ALVS
endif 
ifdef CONFIG_ATC
	CP_C_FLAGS += -DCONFIG_TC
endif

# set bin path/name
CP_BIN := bin/$(PREFIX)

ifdef CONFIG_ATC
	CP_BIN := $(CP_BIN)atc
else ifdef CONFIG_ALVS
	CP_BIN := $(CP_BIN)alvs
else
	CP_BIN := $(CP_BIN)nw
endif

CP_BIN := $(CP_BIN)_daemon
ifdef SIM
	CP_C_FLAGS += -DEZ_SIM
	CP_BIN := $(CP_BIN)_sim
endif

# Tool invocations
make_cp: $(CP_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -L$(EZDK_BASE)/cpe/agt/agt/lib/ -L$(EZDK_BASE)/cpe/agt/agt-cp/lib -L$(EZDK_BASE)/cpe/cp/lib/ -L$(EZDK_BASE)/cpe/dev/lib -L$(EZDK_BASE)/cpe/env/lib -L$(EZDK_BASE)/cpe/jsonrpc-c/install/linux_x86_64/lib -L$(EZDK_BASE)/cpe/libev/install/linux_x86_64/lib/ -pthread $(CP_INC) -o "$(CP_BIN)" $(CP_OBJS) $(CP_LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

build/$(APP_NAME)/%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc $(CP_INC) $(CP_C_FLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

# Other Targets
cp-clean:
	-$(RM) $(CP_OBJS) $(CP_C_DEPS) $(CP_BIN)
	-@echo ' '

.PHONY: make_cp cp-clean 
.SECONDARY:
