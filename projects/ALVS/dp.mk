RM := rm -rf
ENV_BASE := ../..

EZDK_BASE := $(ENV_BASE)/$(shell readlink $(ENV_BASE)/EZdk)
PATH := $(PATH):$(abspath $(EZDK_BASE)ldk/toolchain/bin)

DP_BIN := bin/alvs_dp

DP_C_SRCS := 
DP_OBJS := 
DP_C_DEPS := 


DP_INC := -Isrc/common -Isrc/dp -I$(EZDK_BASE)dpe/dp/include -I$(EZDK_BASE)dpe/frame/include -I$(EZDK_BASE)dpe/sft/include -I$(EZDK_BASE)dpe/dpi/include

DP_C_SRCS = $(shell ls src/dp/*.c)
DP_OBJS = $(patsubst %.c,build/%.o,$(DP_C_SRCS)) 
DP_C_DEPS = $(patsubst %.c,build/%.d,$(DP_C_SRCS))

DP_C_FLAGS := 

ifndef DEBUG
DP_C_FLAGS += -DNDEBUG -O2
else
DP_C_FLAGS += -O1 -g3 -ftree-ter  -gdwarf-2
endif

ifndef SIM
DP_LIBS := -l:ezdp_linux_arc.a -l:ezframe_linux_arc.a -l:dpi_linux_arc.a -l:sft_linux_arc.a
DP_C_FLAGS += 
else
DP_C_FLAGS += -DEZ_SIM
DP_LIBS := -l:ezdp_linux_arc_sim.a -l:ezframe_linux_arc_sim.a -l:dpi_linux_arc_sim.a -l:sft_linux_arc_sim.a
endif

# Tool invocations
make_dp: $(DP_OBJS) 
	@echo 'Building target: $@'
	@echo 'Invoking: ARC GNU C Linker'
	arceb-linux-gcc -L$(EZDK_BASE)dpe/dp/lib/ -L$(EZDK_BASE)dpe/frame/lib/ -L$(EZDK_BASE)dpe/sft/lib/ -L$(EZDK_BASE)dpe/dpi/lib/ $(DP_INC) -o "$(DP_BIN)" $(DP_OBJS) $(DP_LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	-print_memory_usage.sh $(DP_BIN)
#	$(MAKE) --no-print-directory post-build

build/%.o: %.c
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
