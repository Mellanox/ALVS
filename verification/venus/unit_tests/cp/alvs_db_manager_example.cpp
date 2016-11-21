#include "../../output/cp/alvs_db_manager/mock/dut_mock.h"

#include <stdio.h>
#include <gtest/gtest.h>


using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;

//#####################################################################################
//# my mocked functions:
//#####################################################################################



//#####################################################################################
//# Define new test class with setup and teardown
//#####################################################################################

class PrePostTest : public ::testing::Test {
    protected:
        virtual void SetUp() 
		{
			
		}

		virtual void TearDown() 
		{
			
		}

};

//#####################################################################################
//# Tests start here:
//#####################################################################################

/*
 * This is an example cp unit test for function: alvs_msg_parser in src/cp/alvs_db_manager.c
 * This test will cover ipvs daemon get/start/delete code
 */
TEST_F(PrePostTest, get_daemon_without_internal_error)
{
	struct dut_namespace_genl_cmd cmd;
	int rc;

    EXPECT_CALL(mocko, vns_alvs_db_log_daemon())
		.WillOnce(Return((dut_namespace_alvs_db_rc)ALVS_DB_OK));
	enableMock_vns_alvs_db_log_daemon();      
    
	// IPVS_CMD_GET_DAEMON = 11
	cmd.c_id = 11;
    rc = vns_alvs_db_manager_c_alvs_msg_parser(NULL, &cmd, NULL, NULL);

    disableMock_vns_alvs_db_log_daemon();  
	// NL_OK = 0
	EXPECT_EQ(0, rc);
}

TEST_F(PrePostTest, get_daemon_with_internal_error)
{
	struct dut_namespace_genl_cmd cmd;

    EXPECT_CALL(mocko, vns_alvs_db_log_daemon())
		.WillOnce(Return((dut_namespace_alvs_db_rc)ALVS_DB_INTERNAL_ERROR));
	EXPECT_CALL(mocko, vns_alvs_db_manager_exit_with_error())
		.WillOnce(Return());
	enableMock_vns_alvs_db_log_daemon();      
    enableMock_vns_alvs_db_manager_exit_with_error();

	// IPVS_CMD_GET_DAEMON = 11
	cmd.c_id = 11;
    vns_alvs_db_manager_c_alvs_msg_parser(NULL, &cmd, NULL, NULL);

    disableMock_vns_alvs_db_log_daemon();  
	disableMock_vns_alvs_db_manager_exit_with_error();
}

TEST_F(PrePostTest, new_daemon_with_parse_daemon_error)
{
	struct dut_namespace_genl_cmd cmd;
	struct dut_namespace_genl_info info;
	int rc;

    EXPECT_CALL(mocko, vns_alvs_db_manager_c_alvs_genl_parse_daemon_from_msghdr(_,_))
		.WillOnce(Return(-1));
	enableMock_vns_alvs_db_manager_c_alvs_genl_parse_daemon_from_msghdr();    
    
	// IPVS_CMD_NEW_DAEMON = 9
	cmd.c_id = 9;
    rc = vns_alvs_db_manager_c_alvs_msg_parser(NULL, &cmd, &info, NULL);

    disableMock_vns_alvs_db_manager_c_alvs_genl_parse_daemon_from_msghdr();  
	// NL_SKIP = 1
	EXPECT_EQ(1, rc);
}

TEST_F(PrePostTest, new_daemon_with_start_daemon_error)
{
	struct dut_namespace_genl_cmd cmd;
	struct dut_namespace_genl_info info;

    EXPECT_CALL(mocko, vns_alvs_db_manager_c_alvs_genl_parse_daemon_from_msghdr(_,_))
		.WillOnce(Return(0));
	EXPECT_CALL(mocko, vns_alvs_db_start_daemon(_))
		.WillOnce(Return((dut_namespace_alvs_db_rc)ALVS_DB_INTERNAL_ERROR));	
	EXPECT_CALL(mocko, vns_alvs_db_manager_exit_with_error())
		.WillOnce(Return());

	enableMock_vns_alvs_db_manager_c_alvs_genl_parse_daemon_from_msghdr();  
	enableMock_vns_alvs_db_start_daemon();
	enableMock_vns_alvs_db_manager_exit_with_error();
    
	// IPVS_CMD_NEW_DAEMON = 9
	cmd.c_id = 9;
    vns_alvs_db_manager_c_alvs_msg_parser(NULL, &cmd, &info, NULL);

    disableMock_vns_alvs_db_manager_c_alvs_genl_parse_daemon_from_msghdr();
	disableMock_vns_alvs_db_start_daemon();
	disableMock_vns_alvs_db_manager_exit_with_error();

}

TEST_F(PrePostTest, new_daemon_with_stop_daemon)
{
	struct dut_namespace_genl_cmd cmd;
	struct dut_namespace_genl_info info;
	struct dut_namespace_ip_vs_service_user svc;
	int rc;

    EXPECT_CALL(mocko, vns_alvs_db_manager_c_alvs_genl_parse_daemon_from_msghdr(_,_))
		.WillOnce(Return(0));
	EXPECT_CALL(mocko, vns_alvs_db_stop_daemon(_))
		.WillOnce(Return((dut_namespace_alvs_db_rc)ALVS_DB_OK));	

	enableMock_vns_alvs_db_manager_c_alvs_genl_parse_daemon_from_msghdr();  
	enableMock_vns_alvs_db_stop_daemon();
    
	// IPVS_CMD_DEL_DAEMON = 10
	cmd.c_id = 10;
    rc = vns_alvs_db_manager_c_alvs_msg_parser(NULL, &cmd, &info, NULL);

    disableMock_vns_alvs_db_manager_c_alvs_genl_parse_daemon_from_msghdr();
	disableMock_vns_alvs_db_stop_daemon();

	// NL_OK = 0
	EXPECT_EQ(0, rc);
}


