
#ifndef EZDP_TEST_PERFORMANCE_H_
#define EZDP_TEST_PERFORMANCE_H_

#include <ezdp.h>

#ifdef EZ_SIM

#define START_PERFORMANCE_TRACE		\
	ezasm_enable_performance()


#define STOP_PERFORMANCE_TRACE(str)	\
do {					\
	ezasm_disable_performance();	\
	ezasm_report_performance();	\
	ezasm_reset_performance();	\
	ezasm_debug_print_trace(str);	\
} while (0)


#define STOP_AND_START_PERFORMANCE_TRACE(str)	\
do {						\
	ezasm_disable_performance();	\
	ezasm_report_performance();	\
	ezasm_reset_performance();	\
	ezasm_debug_print_trace(str);	\
	ezasm_enable_performance();	\
} while (0)

#define PRINT_TO_PERFORMANCE_TRACE(str)	\
	ezasm_debug_print_trace(str)

#else

#define START_PERFORMANCE_TRACE
#define STOP_PERFORMANCE_TRACE(str)
#define STOP_AND_START_PERFORMANCE_TRACE(str)
#define PRINT_TO_PERFORMANCE_TRACE(str)


#endif

#endif
