#include "../../output/dp/alvs_packet_processing/mock/dut_mock.h"

#include <stdio.h>
#include <gtest/gtest.h>


using ::testing::Return;
using ::testing::SaveArg;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::_;

//#####################################################################################
//# my mocked functions:
//#####################################################################################

extern struct dut_namespace_alvs_cmem cmem;

void my_write_log(int priority, char* str, int length, void* syslog_wa, int syslog_wa_size) {
//	printf("Calling my_write_log mocking function...\n");
//	printf("write_log second arg is: %s\n", str);
}

enum dut_namespace_alvs_sched_server_result my_alvs_get_server_info(dut_namespace_uint8_t service_index, dut_namespace_uint16_t scheduling_index) {
	static int i = 0;
	printf("%d: Calling alvs_get_server_info mocking function and return ALVS_SCHED_SERVER_EMPTY...\n",i);
	i++;
	return (enum dut_namespace_alvs_sched_server_result)ALVS_SCHED_SERVER_EMPTY;
}

//#####################################################################################
//# Define new test class with setup and teardown
//#####################################################################################

class PrePostTest : public ::testing::Test {
    protected:
        virtual void SetUp() 
	{
		printf("******** test SetUp ********\n");

		printf("Mocking ezdp_atomic_read_and_inc32_sum_addr to return 0 always...\n");
		EXPECT_CALL(mocko, vns_alvs_packet_processing_c__ezdp_atomic_read_and_inc32_sum_addr(_,_)).WillOnce(Return(0));
		enableMock_vns_alvs_packet_processing_c__ezdp_atomic_read_and_inc32_sum_addr();

		printf("Mocking ezdp_mod to return 0 always...\n");
		EXPECT_CALL(mocko, vns_alvs_packet_processing_c_ezdp_mod(_,_,_,_)).WillRepeatedly(Return(0));
		enableMock_vns_alvs_packet_processing_c_ezdp_mod();

		printf("Mocking write_log with my_write_log function...\n");
		EXPECT_CALL(mocko, vns_write_log(_,_,_,_,_)).WillRepeatedly(Invoke(my_write_log));
		enableMock_vns_write_log();
		
		printf("******** END test SetUp ********\n");
	}

	virtual void TearDown() 
	{
		printf("******** test TearDown ********\n");

		printf("Disable ezdp_atomic_read_and_inc32_sum_addr mocking...\n");
		disableMock_vns_alvs_packet_processing_c__ezdp_atomic_read_and_inc32_sum_addr();

		printf("Disable ezdp_mod mocking...\n");
		disableMock_vns_alvs_packet_processing_c_ezdp_mod();

		printf("Disable write_log mocking...\n");
		disableMock_vns_write_log();

		printf("******** END test TearDown ********\n");
	}

};

//#####################################################################################
//# Tests start here:
//#####################################################################################

/*
 * This test will handle removing a server while creating a connection, this mean alvs_get_server_info will
 * be called ALVS_RR_SCHED_RETRIES=10 times and will return ALVS_SCHED_SERVER_EMPTY and then 
 * alvs_discard_and_stats will be called with arg = ALVS_ERROR_SCHEDULING_FAIL and alvs_rr_schedule_connection will return false
 */
TEST_F(PrePostTest, remove_server_while_create_conn) 
{

	enum dut_namespace_alvs_error_stats_offsets test_result;
	bool rc;

	printf("Starting test remove_server_while_create_conn...\n");

	printf("Expect calling alvs_get_server_info 10 times with ALVS_SCHED_SERVER_EMPTY return value...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_sched_get_server_info(_,_))
		.Times(10)
		.WillOnce(Invoke(my_alvs_get_server_info)) //1
		.WillOnce(Invoke(my_alvs_get_server_info)) //2
		.WillOnce(Invoke(my_alvs_get_server_info)) //3
		.WillOnce(Invoke(my_alvs_get_server_info)) //4
		.WillOnce(Invoke(my_alvs_get_server_info)) //5
		.WillOnce(Invoke(my_alvs_get_server_info)) //6
		.WillOnce(Invoke(my_alvs_get_server_info)) //7
		.WillOnce(Invoke(my_alvs_get_server_info)) //8
		.WillOnce(Invoke(my_alvs_get_server_info)) //9
		.WillOnce(Invoke(my_alvs_get_server_info)); //10
	printf("Mocking alvs_rr_service_info_lookup to return 0 always...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_sched_service_info_lookup(_)).WillRepeatedly(Return(0));
	printf("Mocking alvs_sched_check_active_servers to return true always...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_sched_check_active_servers()).WillRepeatedly(Return(true));
	printf("Mocking alvs_discard_and_stats and saving its given arg to a my local variable...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_discard_and_stats(_)).WillOnce(DoAll(SaveArg<0>(&test_result), Return()));
	
	enableMock_vns_alvs_packet_processing_c_alvs_sched_get_server_info();
	enableMock_vns_alvs_packet_processing_c_alvs_sched_check_active_servers();
	enableMock_vns_alvs_packet_processing_c_alvs_sched_service_info_lookup();
	enableMock_vns_alvs_packet_processing_c_alvs_discard_and_stats();
	
	printf("Calling alvs_rr_schedule_connection(0)\n");
	rc = vns_alvs_packet_processing_c_alvs_sched_rr_schedule_connection(0);

	disableMock_vns_alvs_packet_processing_c_alvs_sched_get_server_info();
	disableMock_vns_alvs_packet_processing_c_alvs_sched_check_active_servers();
	disableMock_vns_alvs_packet_processing_c_alvs_sched_service_info_lookup();	
	disableMock_vns_alvs_packet_processing_c_alvs_discard_and_stats();
	
	printf("Verify alvs_rr_schedule_connection return with false...\n");
	EXPECT_EQ(false, rc);
	printf("Verify alvs_discard_and_stats called with ALVS_ERROR_SCHEDULING_FAIL...\n");
	EXPECT_EQ(test_result, ALVS_ERROR_SCHEDULING_FAIL);

}


/*
 * This test will handle removing a server with creating a connection and after removing it, to add it again with overloading, 
 * this mean alvs_get_server_info will be called 3 times and will 2*return ALVS_SCHED_SERVER_EMPTY + 1*ALVS_SCHED_SERVER_UNAVAILABLE 
 * then alvs_discard_and_stats will be called with arg = ALVS_ERROR_SERVER_IS_UNAVAILABLE and alvs_rr_schedule_connection will return false
 */
TEST_F(PrePostTest, server_overloading_while_conn_creation) 
{

	dut_namespace_alvs_error_stats_offsets test_result;
	bool rc;

	printf("Starting server_overloading_while_conn_creation...\n");

	printf("Expect calling alvs_get_server_info 3 times with 2*ALVS_SCHED_SERVER_EMPTY + 1*ALVS_SCHED_SERVER_UNAVAILABLE return value...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_sched_get_server_info(_,_))
		.Times(3)
		.WillOnce(Return((enum dut_namespace_alvs_sched_server_result)ALVS_SCHED_SERVER_EMPTY)) //1
		.WillOnce(Return((enum dut_namespace_alvs_sched_server_result)ALVS_SCHED_SERVER_EMPTY)) //2
		.WillOnce(Return((enum dut_namespace_alvs_sched_server_result)ALVS_SCHED_SERVER_UNAVAILABLE)); //3
	printf("Mocking alvs_rr_service_info_lookup to return 0 always...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_sched_service_info_lookup(_)).WillRepeatedly(Return(0));
	printf("Mocking alvs_sched_check_active_servers to return true always...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_sched_check_active_servers()).WillRepeatedly(Return(true));
	printf("Mocking alvs_discard_and_stats and saving its given arg to a my local variable...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_discard_and_stats(_)).WillOnce(DoAll(SaveArg<0>(&test_result), Return()));
	
	enableMock_vns_alvs_packet_processing_c_alvs_sched_get_server_info();
	enableMock_vns_alvs_packet_processing_c_alvs_sched_check_active_servers();
	enableMock_vns_alvs_packet_processing_c_alvs_sched_service_info_lookup();
	enableMock_vns_alvs_packet_processing_c_alvs_discard_and_stats();
	
	printf("Calling alvs_rr_schedule_connection(0)\n");
	rc = vns_alvs_packet_processing_c_alvs_sched_rr_schedule_connection(0);

	disableMock_vns_alvs_packet_processing_c_alvs_sched_get_server_info();
	disableMock_vns_alvs_packet_processing_c_alvs_sched_check_active_servers();
	disableMock_vns_alvs_packet_processing_c_alvs_sched_service_info_lookup();	
	disableMock_vns_alvs_packet_processing_c_alvs_discard_and_stats();
	
	printf("Verify alvs_rr_schedule_connection return with false...\n");
	EXPECT_EQ(false, rc);
	printf("Verify alvs_discard_and_stats called with ALVS_ERROR_SERVER_IS_UNAVAILABLE...\n");
	EXPECT_EQ(test_result, ALVS_ERROR_SERVER_IS_UNAVAILABLE);

}

/*
 * This test will handle removing a server & service while creating a connection, this mean alvs_get_server_info will
 * be called and return ALVS_SCHED_SERVER_EMPTY and alvs_rr_service_info_lookup will return 1 (failure) 
 * then alvs_discard_and_stats will be called with arg = ALVS_ERROR_SERVICE_INFO_LOOKUP and alvs_rr_schedule_connection will return false
 */
TEST_F(PrePostTest, removing_server_service_while_conn_creation) 
{

	dut_namespace_alvs_error_stats_offsets test_result;
	bool rc;

	printf("Starting removing_server_service_while_conn_creation...\n");

	printf("Expect calling alvs_get_server_info 1 times with ALVS_SCHED_SERVER_EMPTY return value...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_sched_get_server_info(_,_)).WillOnce(Return((enum dut_namespace_alvs_sched_server_result)ALVS_SCHED_SERVER_EMPTY));
	printf("Expect calling alvs_rr_service_info_lookup 1 times with 1 (failure) return value...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_sched_service_info_lookup(_)).WillOnce(Return(1));
	printf("Mocking alvs_sched_check_active_servers to return true always...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_sched_check_active_servers()).WillRepeatedly(Return(true));
	printf("Mocking alvs_discard_and_stats and saving its given arg to a my local variable...\n");
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_discard_and_stats(_)).WillOnce(DoAll(SaveArg<0>(&test_result), Return()));
	
	enableMock_vns_alvs_packet_processing_c_alvs_sched_get_server_info();
	enableMock_vns_alvs_packet_processing_c_alvs_sched_check_active_servers();
	enableMock_vns_alvs_packet_processing_c_alvs_sched_service_info_lookup();
	enableMock_vns_alvs_packet_processing_c_alvs_discard_and_stats();
	
	printf("Calling alvs_rr_schedule_connection(0)\n");
	rc = vns_alvs_packet_processing_c_alvs_sched_rr_schedule_connection(0);

	disableMock_vns_alvs_packet_processing_c_alvs_sched_get_server_info();
	disableMock_vns_alvs_packet_processing_c_alvs_sched_check_active_servers();
	disableMock_vns_alvs_packet_processing_c_alvs_sched_service_info_lookup();	
	disableMock_vns_alvs_packet_processing_c_alvs_discard_and_stats();
	
	printf("Verify alvs_rr_schedule_connection return with false...\n");
	EXPECT_EQ(false, rc);
	printf("Verify alvs_discard_and_stats called with ALVS_ERROR_SERVICE_INFO_LOOKUP...\n");
	EXPECT_EQ(test_result, ALVS_ERROR_SERVICE_INFO_LOOKUP);

}

//#####################################################################################

