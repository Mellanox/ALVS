CP_BIN := bin/alvs_cp

CP_C_SRCS := 
CP_OBJS := 
CP_C_DEPS := 

CP_INC := -Isrc -I$(EZDK_BASE)/dpe/dp/include -I$(EZDK_BASE)/cpe/env/include -I$(EZDK_BASE)/cpe/dev/include -I$(EZDK_BASE)/cpe/cp/include -I$(EZDK_BASE)/cpe/agt/agt-cp/include -I$(EZDK_BASE)/cpe/agt/agt/include

CP_C_SRCS = $(shell ls src/*.c)
CP_OBJS =$(CP_C_SRCS:.c=.o)
CP_C_DEPS =$(CP_C_SRCS:.c=.d)
CP_LIBS := -l:EZagt_linux_x86_64.a -l:EZagt-cp_linux_x86_64.a -l:EZcp_linux_x86_64.a -l:EZdev_linux_x86_64.a -l:EZenv_linux_x86_64.a -l:libjsonrpcc.a -l:libev.a


# Tool invocations
alvs_cp: $(CP_OBJS) $(USER_CP_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -L$(EZDK_BASE)/cpe/agt/agt/lib/ -L$(EZDK_BASE)/cpe/agt/agt-cp/lib -L$(EZDK_BASE)/cpe/cp/lib/ -L$(EZDK_BASE)/cpe/dev/lib -L$(EZDK_BASE)/cpe/env/lib -L$(EZDK_BASE)/cpe/jsonrpc-c/install/linux_x86_64/lib -L$(EZDK_BASE)/cpe/libev/install/linux_x86_64/lib/ -pthread -lm $(CP_INC) -o "$(CP_BIN)" $(CP_OBJS) $(USER_CP_OBJS) $(CP_LIBS)
	@echo 'Finished building target: $@'
	@echo ' '


%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc $(CP_INC) -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

# Other Targets
cp-clean:
	-$(RM) $(CP_OBJS) $(CP_C_DEPS) $(CP_BIN)
	-@echo ' '

.PHONY: alvs_cp cp-clean 
.SECONDARY:
