DP_BIN := $(DP_BASE)bin/alvs_dp

DP_C_SRCS := 
DP_OBJS := 
DP_C_DEPS := 


DP_INC := -I$(DP_BASE)src -I$(EZDK_BASE)/dpe/dp/include -I$(EZDK_BASE)/dpe/frame/include -I$(EZDK_BASE)/dpe/sft/include -I$(EZDK_BASE)/dpe/dpi/include

DP_C_SRCS = $(shell ls $(DP_BASE)src/*.c)
DP_OBJS =$(DP_C_SRCS:.c=.o)
DP_C_DEPS =$(DP_C_SRCS:.c=.d)


DP_LIBS := -l:ezdp_linux_arc.a -l:ezframe_linux_arc.a -l:dpi_linux_arc.a -l:sft_linux_arc.a

# Tool invocations
alvs_dp: $(DP_OBJS) 
	@echo 'Building target: $@'
	@echo 'Invoking: ARC GNU C Linker'
	arceb-linux-gcc -L$(EZDK_BASE)/dpe/dp/lib/ -L$(EZDK_BASE)/dpe/frame/lib/ -L$(EZDK_BASE)/dpe/sft/lib/ -L$(EZDK_BASE)/dpe/dpi/lib/ $(DP_INC) -o "$(DP_BIN)" $(DP_OBJS) $(DP_LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARC GNU C Compiler'
	arceb-linux-gcc -DNDEBUG -O2 -Wall -c -fmessage-length=0 -mq-class -mlong-calls -mbitops -munaligned-access -mcmem -mswape -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" $(DP_INC) -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


# Other Targets
dp-clean:
	-$(RM) $(DP_OBJS) $(DP_C_DEPS)  $(DP_BIN)
	-@echo ' '

post-build:
	-print_memory_usage.sh $(DP_BIN)
	-@echo ' '

.PHONY: alvs_dp dp-clean 
.SECONDARY: post-build
