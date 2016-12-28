RM := rm -rf
ENV_BASE := ./

EZDK_BASE := $(shell readlink $(ENV_BASE)/EZdk)

CLI_APP_INC := -Isrc/common -Isrc/cli -I$(EZDK_BASE)/dpe/dp/include

CLI_APP_C_SRCS = src/common/cli_common.c \
				 src/cli/main.c
ifdef CONFIG_ALVS
CLI_APP_C_SRCS += src/cli/alvs_cli.c
endif
ifdef CONFIG_TC
CLI_APP_C_SRCS += src/cli/tc_cli.c
endif


ifdef CONFIG_ALVS
	APP_NAME := alvs
endif
ifdef CONFIG_TC
	APP_NAME := tc
endif

CLI_APP_OBJS = $(patsubst %.c,build/$(APP_NAME)/%.o,$(CLI_APP_C_SRCS)) 
CLI_APP_C_DEPS = $(patsubst %.c,build/$(APP_NAME)/%.d,$(CLI_APP_C_SRCS))

CLI_APP_LIBS := -lpopt

CLI_APP_C_FLAGS := -DNPS_LITTLE_ENDIAN -Werror -Wall -Wextra

ifdef CLI_APP_DEBUG
CLI_APP_C_FLAGS += -O0 -g3
PREFIX := debug/
else
CLI_APP_C_FLAGS += -O0 -DNDEBUG
PREFIX := 
endif

ifdef CONFIG_ALVS
	CLI_APP_C_FLAGS += -DCONFIG_ALVS
endif
ifdef CONFIG_TC
	CLI_APP_C_FLAGS += -DCONFIG_TC
endif

# set bin path/name
CLI_APP_BIN := bin/$(PREFIX)$(APP_NAME)


# Tool invocations
make_cli: $(CLI_APP_OBJS) 
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -pthread $(CLI_APP_INC) -o "$(CLI_APP_BIN)" $(CLI_APP_OBJS) $(CLI_APP_LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

build/$(APP_NAME)/%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc $(CLI_APP_INC) $(CLI_APP_C_FLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

# Other Targets
cli-clean:
	-$(RM) $(CLI_APP_OBJS) $(CLI_APP_C_DEPS) $(CLI_APP_BIN)
	-@echo ' '

.PHONY: make_cli cli-clean 
.SECONDARY:
