RM := rm -rf
ENV_BASE := ./

EZDK_BASE := $(shell readlink $(ENV_BASE)/EZdk)
PATH := $(PATH):$(abspath $(EZDK_BASE)ldk/toolchain/bin)

DP_INC := -Isrc/common -Isrc/dp -I$(EZDK_BASE)dpe/dp/include -I$(EZDK_BASE)dpe/frame/include

DP_C_SRCS = src/dp/main.c \
			src/dp/anl_log.c \
			src/dp/nw_init.c \
			src/dp/nw_routing.c \
			src/dp/nw_utils.c \
			src/dp/version.c \
			src/dp/log.c
ifdef CONFIG_ALVS
DP_C_SRCS += src/dp/alvs_init.c \
			 src/dp/alvs_packet_processing.c
endif

ifdef CONFIG_ATC
DP_C_SRCS += src/dp/tc_init.c \
             src/dp/tc_packet_processing.c
endif

ifdef CONFIG_ALVS
	APP_NAME := alvs
else ifdef CONFIG_ATC
	APP_NAME := atc
else
	APP_NAME := nw
endif

DP_OBJS = $(patsubst %.c,build/$(APP_NAME)/%.o,$(DP_C_SRCS)) 
DP_C_DEPS = $(patsubst %.c,build/$(APP_NAME)/%.d,$(DP_C_SRCS))

DP_C_FLAGS := -DNPS_BIG_ENDIAN -Werror -Wall -Wextra

ifdef DP_DEBUG
DP_C_FLAGS += -O1 -g3 -ftree-ter -gdwarf-2
PREFIX := debug/
else
DP_C_FLAGS += -DNDEBUG -O2
PREFIX := 
endif

ifdef CONFIG_ALVS
	DP_C_FLAGS += -DCONFIG_ALVS
endif
ifdef CONFIG_ATC
	DP_C_FLAGS += -DCONFIG_TC
endif

ifdef SIM
DP_C_FLAGS += -DEZ_SIM
DP_LIBS := -l:ezdp_linux_arc_sim.a -l:ezframe_linux_arc_sim.a
else
DP_LIBS := -l:ezdp_linux_arc.a -l:ezframe_linux_arc.a
endif

# set bin path/name
DP_BIN := bin/$(PREFIX)$(APP_NAME)_dp
ifdef SIM
	DP_BIN := $(DP_BIN)_sim
endif

# Tool invocations
make_dp: $(DP_OBJS) 
	@echo 'Building target: $@'
	@echo 'Invoking: ARC GNU C Linker'
	arceb-linux-gcc -L$(EZDK_BASE)dpe/dp/lib/ -L$(EZDK_BASE)dpe/frame/lib/ -o "$(DP_BIN)" $(DP_OBJS) $(DP_LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	-print_memory_usage.sh $(DP_BIN)
#	$(MAKE) --no-print-directory post-build

build/$(APP_NAME)/%.o: %.c
	@echo 'EZDK path $(EZDK_BASE) '
	@echo 'Building file: $<'
	@echo 'Invoking: ARC GNU C Compiler'
	arceb-linux-gcc $(DP_C_FLAGS) -Wall -c -fmessage-length=0 -mq-class -mlong-calls -mbitops -munaligned-access -mcmem -mswape -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" $(DP_INC) -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


# Other Targets
dp-clean:
	-$(RM) $(DP_OBJS) $(DP_C_DEPS)  $(DP_BIN)
	-@echo ' '

post-build:
	-print_memory_usage.sh $(DP_BIN)
	-@echo ' '

.PHONY: make_dp dp-clean 
.SECONDARY: post-build
