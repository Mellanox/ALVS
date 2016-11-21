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

#define IP_HEADER_LEN 20
#define ETH_HEADER_LEN 14
#define UDP_HEADER_LEN 8
#define SYNC_HEADER_LEN 8
#define SYNC_CONN_LEN 36
#define MULTICAST_DST_IP 0xe0000051
#define SOURCE_IP 0x12345678
#define ALVS_STATE_SYNC_HEADROOM 64
#define EZFRAME_BUF_DATA_SIZE 256
#define EZFRAME_CHECKSUM_VALUE 0x8765
#define ETHERTYPE_IP 0x0800
#define MY_LOGIC_0_MAC {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}
#define ALVS_STATE_SYNC_DST_MAC {0x01, 0x00, 0x5e, 0x00, 0x00, 0x51}

extern struct dut_namespace_alvs_cmem cmem_alvs;
extern struct dut_namespace_cmem_nw_info cmem_nw;
extern dut_namespace_uint8_t frame_data[EZFRAME_BUF_DATA_SIZE];

//#####################################################################################
//# My functions mockups
//#####################################################################################

void my_ezframe_update_ipv4_checksum(struct dut_namespace_iphdr *ipv4) {
	printf("Calling my_ezframe_update_ipv4_checksum mocking function...\n");
	ipv4->check = EZFRAME_CHECKSUM_VALUE;
}

dut_namespace_uint32_t my_nw_interface_lookup(dut_namespace_int32_t logical_id) {
	printf("Calling my_nw_interface_lookup mocking function...\n");
	if (logical_id == 0)
		cmem_nw.interface_result.mac_address.ether_addr_octet = MY_LOGIC_0_MAC;
	else
		cmem_nw.interface_result.mac_address.ether_addr_octet = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	return 0;
}

void * my_ezdp_mem_copy (void *dst, const void *src, dut_namespace_uint32_t size) {
	printf("Calling my_ezdp_mem_copy mocking function...\n");
	return memcpy(dst, src, size);
}

//#####################################################################################
//# Checkers
//#####################################################################################

void expect_eth_hdr(dut_namespace_ether_header *eth_header) {
	dut_namespace_uint8_t dst_eth_addr[6] = ALVS_STATE_SYNC_DST_MAC;
	dut_namespace_uint8_t src_eth_addr[6] = MY_LOGIC_0_MAC;
	/*eth header info*/
	EXPECT_EQ(dst_eth_addr[0], eth_header->ether_dhost[0]);
	EXPECT_EQ(src_eth_addr[0], eth_header->ether_shost[0]);
	EXPECT_EQ(ETHERTYPE_IP, eth_header->ether_type);
}

void expect_net_hdr(dut_namespace_net_hdr *net_hdr_info, int conn_count) {
	/*net header info*/
	EXPECT_EQ(UDP_HEADER_LEN + SYNC_HEADER_LEN + SYNC_CONN_LEN*conn_count, net_hdr_info->udp.len);
	EXPECT_EQ(IP_HEADER_LEN + UDP_HEADER_LEN + SYNC_HEADER_LEN + SYNC_CONN_LEN*conn_count, net_hdr_info->ipv4.tot_len);
	EXPECT_EQ(EZFRAME_CHECKSUM_VALUE, net_hdr_info->ipv4.check);
	EXPECT_EQ(8848, net_hdr_info->udp.source);
	EXPECT_EQ(8848, net_hdr_info->udp.dest);
	EXPECT_EQ(0, net_hdr_info->udp.check);
	EXPECT_EQ(4, net_hdr_info->ipv4.version);
	EXPECT_EQ(IP_HEADER_LEN/4, net_hdr_info->ipv4.ihl);
	EXPECT_EQ(0, net_hdr_info->ipv4.tos);
	EXPECT_EQ(0, net_hdr_info->ipv4.id);
	EXPECT_EQ(0x4000, net_hdr_info->ipv4.frag_off);/*IP_DF*/
	EXPECT_EQ(1, net_hdr_info->ipv4.ttl);
	EXPECT_EQ(17, net_hdr_info->ipv4.protocol); /*IPPROTO_UDP*/
	EXPECT_EQ(SOURCE_IP, net_hdr_info->ipv4.saddr);
	EXPECT_EQ(MULTICAST_DST_IP, net_hdr_info->ipv4.daddr);
}

void expect_sync_hdr(dut_namespace_alvs_state_sync_header *sync_hdr, int conn_count, int sync_id) {
	/*sync header*/
	EXPECT_EQ(SYNC_HEADER_LEN + SYNC_CONN_LEN*conn_count, sync_hdr->size);
	EXPECT_EQ(conn_count, sync_hdr->conn_count);
	EXPECT_EQ(sync_id, sync_hdr->syncid);
	EXPECT_EQ(1, sync_hdr->version);
	EXPECT_EQ(0, sync_hdr->spare);
}

void expect_sync_conn(dut_namespace_alvs_state_sync_conn *sync_conn, int bound, int state) {
	/*sync conn*/
	EXPECT_EQ(0, sync_conn->type); /*IPV4*/
	EXPECT_EQ(6, sync_conn->protocol);/*TCP*/
	EXPECT_EQ(0, sync_conn->version);
	EXPECT_EQ(36, sync_conn->size);
	EXPECT_EQ(0x120, sync_conn->flags);
	EXPECT_EQ(state, sync_conn->state);
	if (state == 7) {
		EXPECT_EQ(16, sync_conn->timeout);
	} else {
		EXPECT_EQ(960, sync_conn->timeout);
	}
	EXPECT_EQ(1234, sync_conn->client_port);
	EXPECT_EQ(80, sync_conn->virtual_port);

	EXPECT_EQ(0, sync_conn->fwmark);

	EXPECT_EQ(0xc0a80001, sync_conn->client_addr);
	EXPECT_EQ(0xc0a80101, sync_conn->virtual_addr);
	if (bound) {
		EXPECT_EQ(88, sync_conn->server_port);
		EXPECT_EQ(0xc0a80002, sync_conn->server_addr);
	} else {
		EXPECT_EQ(89, sync_conn->server_port);
		EXPECT_EQ(0xc0a80003, sync_conn->server_addr);
	}
}
//#####################################################################################
//# Define new test class with setup and teardown
//#####################################################################################

class PrePostTest : public ::testing::Test {
    protected:
        virtual void SetUp() 
	{
		printf("******** test SetUp ********\n");

		printf("Set cmem...\n");
		memset(frame_data, 0, EZFRAME_BUF_DATA_SIZE);
		/*conn info result*/
		cmem_alvs.conn_info_result.conn_class_key.protocol = 6;/*TCP*/
		cmem_alvs.conn_info_result.conn_flags = 0x120;
		cmem_alvs.conn_info_result.conn_state = (dut_namespace_alvs_tcp_conn_state)7;/*CLOSE_WAIT*/
		cmem_alvs.conn_info_result.conn_class_key.client_port = 1234;
		cmem_alvs.conn_info_result.conn_class_key.virtual_port = 80;
		cmem_alvs.conn_info_result.conn_class_key.client_ip = 0xc0a80001;
		cmem_alvs.conn_info_result.conn_class_key.virtual_ip = 0xc0a80101;
		cmem_alvs.conn_info_result.bound = true;
		cmem_alvs.conn_info_result.server_addr = 0xc0a80003;
		cmem_alvs.conn_info_result.server_port = 89;
		/*server info result*/
		cmem_alvs.server_info_result.server_ip = 0xc0a80002;
		cmem_alvs.server_info_result.server_port = 88;
		/*conn sync state*/
		cmem_alvs.conn_sync_state.conn_sync_status = (enum dut_namespace_alvs_conn_sync_status)1;/*NO_NEED*/
		cmem_alvs.conn_sync_state.amount_buffers = 0;
		cmem_alvs.conn_sync_state.current_base = NULL;
		cmem_alvs.conn_sync_state.current_len = 0;
		cmem_alvs.conn_sync_state.conn_count = 0;

		printf("Mocking ezframe_update_ipv4_checksum...\n");
		EXPECT_CALL(mocko, vns_alvs_packet_processing_c_ezframe_update_ipv4_checksum(_)).WillRepeatedly(Invoke(my_ezframe_update_ipv4_checksum));
		enableMock_vns_alvs_packet_processing_c_ezframe_update_ipv4_checksum();
		EXPECT_CALL(mocko, vns_main_c_ezframe_update_ipv4_checksum(_)).WillRepeatedly(Invoke(my_ezframe_update_ipv4_checksum));
		enableMock_vns_main_c_ezframe_update_ipv4_checksum();
//
//		printf("Mocking ezdp_mod to return 0 always...\n");
//		EXPECT_CALL(mocko, vns_alvs_packet_processing_c_ezdp_mod(_,_,_,_)).WillRepeatedly(Return(0));
//		enableMock_vns_alvs_packet_processing_c_ezdp_mod();
//
		printf("Mocking nw_interface_lookup with my_nw_interface_lookup function...\n");
		EXPECT_CALL(mocko, vns_alvs_packet_processing_c_nw_interface_lookup(_)).WillRepeatedly(Invoke(my_nw_interface_lookup));
		enableMock_vns_alvs_packet_processing_c_nw_interface_lookup();
		EXPECT_CALL(mocko, vns_main_c_nw_interface_lookup(_)).WillRepeatedly(Invoke(my_nw_interface_lookup));
		enableMock_vns_main_c_nw_interface_lookup();

		printf("Mocking write_log function...\n");
		EXPECT_CALL(mocko, vns_write_log(_,_,_,_,_)).WillRepeatedly(Return());
		enableMock_vns_write_log();

		printf("Mocking ezdp_mem_copy with my_ezdp_mem_copy function...\n");
		EXPECT_CALL(mocko, vns_alvs_packet_processing_c_ezdp_mem_copy(_,_,_)).WillRepeatedly(Invoke(my_ezdp_mem_copy));
		enableMock_vns_alvs_packet_processing_c_ezdp_mem_copy();
		EXPECT_CALL(mocko, vns_main_c_ezdp_mem_copy(_,_,_)).WillRepeatedly(Invoke(my_ezdp_mem_copy));
		enableMock_vns_main_c_ezdp_mem_copy();

		printf("Mocking _ezframe_append_buf function...\n");
		EXPECT_CALL(mocko, vns_main_c__ezframe_append_buf(_,_,_,_)).WillRepeatedly(Return(0));
		enableMock_vns_main_c__ezframe_append_buf();


		printf("******** END test SetUp ********\n");
	}

	virtual void TearDown() 
	{
		printf("******** test TearDown ********\n");

		printf("Disable ezframe_update_ipv4_checksum mocking...\n");
		disableMock_vns_alvs_packet_processing_c_ezframe_update_ipv4_checksum();
		disableMock_vns_main_c_ezframe_update_ipv4_checksum();
//
//		printf("Disable ezdp_mod mocking...\n");
//		disableMock_vns_alvs_packet_processing_c_ezdp_mod();
//
		printf("Disable write_log mocking...\n");
		disableMock_vns_write_log();

		printf("Disable nw_interface_lookup mocking...\n");
		disableMock_vns_alvs_packet_processing_c_nw_interface_lookup();
		disableMock_vns_main_c_nw_interface_lookup();

		printf("Disable ezdp_mem_copy mocking...\n");
		disableMock_vns_alvs_packet_processing_c_ezdp_mem_copy();
		disableMock_vns_main_c_ezdp_mem_copy();

		printf("Disable _ezframe_append_buf mocking...\n");
		disableMock_vns_main_c__ezframe_append_buf();

		printf("******** END test TearDown ********\n");
	}

};

//#####################################################################################
//# Tests start here:
//#####################################################################################

/*test network header update with 23 sync connections*/
TEST_F(PrePostTest, update_net_hdr_len)
{
	int conn_count = 23;
	struct dut_namespace_net_hdr net_hdr_info;
	int sync_len = SYNC_HEADER_LEN + SYNC_CONN_LEN*conn_count;
	int udp_len = UDP_HEADER_LEN;
	int ip_len = IP_HEADER_LEN;

	printf("Starting test update_net_hdr_len...\n");

	printf("Calling alvs_state_sync_update_net_hdr_len with %d connections\n", conn_count);
	vns_alvs_packet_processing_c_alvs_state_sync_update_net_hdr_len(&net_hdr_info, conn_count);

	printf("verify fields...\n");
	EXPECT_EQ(udp_len + sync_len, net_hdr_info.udp.len);
	EXPECT_EQ(ip_len + udp_len + sync_len, net_hdr_info.ipv4.tot_len);
	EXPECT_EQ(EZFRAME_CHECKSUM_VALUE, net_hdr_info.ipv4.check);
}

/*test setting network header*/
TEST_F(PrePostTest, set_net_hdr)
{
	int conn_count = 23;
	struct dut_namespace_net_hdr net_hdr_info;

	printf("Starting test update_net_hdr_len...\n");

	printf("Calling alvs_state_sync_set_net_hdr with %d connections and 0x%x ip\n", conn_count, SOURCE_IP);
	vns_alvs_packet_processing_c_alvs_state_sync_set_net_hdr(&net_hdr_info, conn_count, SOURCE_IP);

	printf("verify fields...\n");
	expect_net_hdr(&net_hdr_info, conn_count);
}

/*test setting ethernet header*/
TEST_F(PrePostTest, set_eth_hdr)
{
	dut_namespace_uint8_t dst_eth_addr[6] = ALVS_STATE_SYNC_DST_MAC;
	dut_namespace_uint8_t src_eth_addr[6] = MY_LOGIC_0_MAC;
	struct dut_namespace_ether_header eth_header;

	printf("Starting test set_eth_hdr...\n");

	printf("Calling alvs_state_sync_set_eth_hdr\n");
	vns_alvs_packet_processing_c_alvs_state_sync_set_eth_hdr(&eth_header);

	printf("verify fields...\n");
	expect_eth_hdr(&eth_header);
}

/*test sync header update with 23 sync connections*/
TEST_F(PrePostTest, update_sync_hdr_len)
{
	int conn_count = 23;
	struct dut_namespace_alvs_state_sync_header hdr;
	int sync_len = SYNC_HEADER_LEN + SYNC_CONN_LEN*conn_count;

	printf("Starting test update_sync_hdr_len...\n");

	printf("Calling alvs_state_sync_update_sync_hdr_len with %d connections\n", conn_count);
	vns_alvs_packet_processing_c_alvs_state_sync_update_sync_hdr_len(&hdr, conn_count);

	printf("verify fields...\n");
	EXPECT_EQ(sync_len, hdr.size);
	EXPECT_EQ(conn_count, hdr.conn_count);
}

/*test setting sync header*/
TEST_F(PrePostTest, set_sync_hdr)
{
	int conn_count = 23;
	int sync_id = 7;
	struct dut_namespace_alvs_state_sync_header hdr;

	printf("Starting test set_sync_hdr...\n");

	printf("Calling alvs_state_sync_set_sync_hdr with %d connections and %d sync id\n", conn_count, sync_id);
	vns_alvs_packet_processing_c_alvs_state_sync_set_sync_hdr(&hdr, conn_count, sync_id);

	printf("verify fields...\n");
	expect_sync_hdr(&hdr, conn_count, sync_id);
}

/*test setting bounded CLOSE_WAIT sync connection*/
TEST_F(PrePostTest, set_bounded_sync_conn)
{
	struct dut_namespace_alvs_state_sync_conn conn;

	printf("Starting test set_bounded_sync_conn...\n");

	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_util_get_conn_iterations(_)).WillOnce(Return(1));

	printf("Enable mocks...\n");
	enableMock_vns_alvs_packet_processing_c_alvs_util_get_conn_iterations();

	printf("Calling alvs_state_sync_set_sync_conn\n");
	vns_alvs_packet_processing_c_alvs_state_sync_set_sync_conn(&conn);

	printf("Disable mocks...\n");
	disableMock_vns_alvs_packet_processing_c_alvs_util_get_conn_iterations();

	printf("verify fields...\n");
	expect_sync_conn(&conn, 1, 7); /*BOUNDED, CLOSE_WAIT*/
}

/*test setting unbounded ESTABLISHED sync connection*/
TEST_F(PrePostTest, set_unbounded_sync_conn)
{
	struct dut_namespace_alvs_state_sync_conn conn;

	printf("Starting test set_unbounded_sync_conn...\n");

	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_util_get_conn_iterations(_)).WillOnce(Return(60));

	printf("Enable mocks...\n");
	enableMock_vns_alvs_packet_processing_c_alvs_util_get_conn_iterations();

	printf("Update cmem...\n");
	cmem_alvs.conn_info_result.bound = false;
	cmem_alvs.conn_info_result.conn_state = (dut_namespace_alvs_tcp_conn_state)1;/*ESTABLISHED*/

	printf("Calling alvs_state_sync_set_sync_conn\n");
	vns_alvs_packet_processing_c_alvs_state_sync_set_sync_conn(&conn);

	printf("Disable mocks...\n");
	disableMock_vns_alvs_packet_processing_c_alvs_util_get_conn_iterations();

	printf("verify fields...\n");
	expect_sync_conn(&conn, 0, 1); /*UNBOUNDED, ESTABLISHED*/
}

/*test setting first sync frame buffer*/
TEST_F(PrePostTest, set_sync_first)
{
	struct dut_namespace_net_hdr  *net_hdr_info;
	struct dut_namespace_alvs_state_sync_conn *sync_conn;
	struct dut_namespace_alvs_state_sync_header *sync_hdr;
	struct dut_namespace_ether_header *eth_header;
	dut_namespace_uint8_t dst_eth_addr[6] = ALVS_STATE_SYNC_DST_MAC;
	dut_namespace_uint8_t src_eth_addr[6] = MY_LOGIC_0_MAC;
	dut_namespace_in_addr_t source_ip = SOURCE_IP;
	dut_namespace_uint8_t sync_id = 3;
	int total_len = IP_HEADER_LEN + ETH_HEADER_LEN + UDP_HEADER_LEN + SYNC_HEADER_LEN + SYNC_CONN_LEN;
	dut_namespace_uint8_t *frame_base = frame_data + total_len;

	eth_header = (struct dut_namespace_ether_header *)frame_data;
	net_hdr_info = (struct dut_namespace_net_hdr *)((dut_namespace_uint8_t *)eth_header + sizeof(struct dut_namespace_ether_header));
	sync_hdr = (struct dut_namespace_alvs_state_sync_header *)((dut_namespace_uint8_t *)net_hdr_info + sizeof(struct dut_namespace_net_hdr));
	sync_conn = (struct dut_namespace_alvs_state_sync_conn *)((dut_namespace_uint8_t *)sync_hdr + sizeof(struct dut_namespace_alvs_state_sync_header));

	printf("Starting test set_sync_first...\n");

	printf("Calling alvs_state_sync_first\n");
	vns_alvs_packet_processing_c_alvs_state_sync_first(source_ip, sync_id);

	/*verify first buffer state*/
	EXPECT_EQ(1, cmem_alvs.conn_sync_state.amount_buffers);
	EXPECT_EQ(1, cmem_alvs.conn_sync_state.conn_count);
	EXPECT_EQ(total_len, cmem_alvs.conn_sync_state.current_len);
	EXPECT_EQ(frame_base, cmem_alvs.conn_sync_state.current_base);

	/*verify first buffer content*/
	/*eth header info*/
	expect_eth_hdr(eth_header);
	/*net header info*/
	expect_net_hdr(net_hdr_info, 1);
	/*sync header*/
	expect_sync_hdr(sync_hdr, 1, sync_id);
	/*sync conn*/
	expect_sync_conn(sync_conn, 1, 7); /*BOUNDED, CLOSE_WAIT*/
}

/*test setting sending sync frame with single connection
 * with ezframe_new success*/
TEST_F(PrePostTest, send_single_successfully)
{
	dut_namespace_in_addr_t source_ip = SOURCE_IP;
	dut_namespace_uint8_t sync_id = 3;
	dut_namespace_uint8_t len;
	int total_len = IP_HEADER_LEN + ETH_HEADER_LEN + UDP_HEADER_LEN + SYNC_HEADER_LEN + SYNC_CONN_LEN;
	int logical_id = -1;

	printf("Starting test send_single_successfully...\n");

	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_state_sync_first(_,_)).WillOnce(Return());
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_nw_send_frame_to_network(_,_,_)).WillOnce(
		DoAll(SaveArg<2>(&logical_id), Return()));
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_ezframe_new(_,_,_,_,_)).WillOnce(
		DoAll(SaveArg<2>(&len), Return(0)));

	cmem_alvs.conn_sync_state.current_len = total_len;

	printf("Enable mocks...\n");
	enableMock_vns_alvs_packet_processing_c_alvs_state_sync_first();
	enableMock_vns_alvs_packet_processing_c_nw_send_frame_to_network();
	enableMock_vns_alvs_packet_processing_c_ezframe_new();

	printf("Calling alvs_state_sync_send_single\n");
	vns_alvs_packet_processing_c_alvs_state_sync_send_single(source_ip, sync_id);

	printf("Disable mocks...\n");
	disableMock_vns_alvs_packet_processing_c_alvs_state_sync_first();
	disableMock_vns_alvs_packet_processing_c_nw_send_frame_to_network();
	disableMock_vns_alvs_packet_processing_c_ezframe_new();

	EXPECT_EQ(0, logical_id);
	EXPECT_EQ(total_len, len);
}

/*test setting sending sync frame with single connection
 * with ezframe_new failure*/
TEST_F(PrePostTest, send_single_failure)
{
	dut_namespace_in_addr_t source_ip = SOURCE_IP;
	dut_namespace_uint8_t sync_id = 3;

	printf("Starting test send_single_failure...\n");

	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_alvs_state_sync_first(_,_)).WillOnce(Return());
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_ezframe_new(_,_,_,_,_)).WillOnce(Return(1));
	/*expect no frame ot be sent to network if ezframe_new failed*/
	EXPECT_CALL(mocko, vns_alvs_packet_processing_c_nw_send_frame_to_network(_,_,_)).Times(0);

	printf("Enable mocks...\n");
	enableMock_vns_alvs_packet_processing_c_alvs_state_sync_first();
	enableMock_vns_alvs_packet_processing_c_nw_send_frame_to_network();
	enableMock_vns_alvs_packet_processing_c_ezframe_new();

	printf("Calling alvs_state_sync_send_single\n");
	vns_alvs_packet_processing_c_alvs_state_sync_send_single(source_ip, sync_id);

	printf("Disable mocks...\n");
	disableMock_vns_alvs_packet_processing_c_alvs_state_sync_first();
	disableMock_vns_alvs_packet_processing_c_nw_send_frame_to_network();
	disableMock_vns_alvs_packet_processing_c_ezframe_new();

}

/*testing adding buffer to current sync frame:
 * with first buffer & success
 */
TEST_F(PrePostTest, add_buffer_first_buffer_success)
{
	int rc;
	dut_namespace_uint8_t len;
	int total_len = IP_HEADER_LEN + ETH_HEADER_LEN + UDP_HEADER_LEN + SYNC_HEADER_LEN + SYNC_CONN_LEN;

	printf("Starting test add_buffer_first_buffer_success...\n");
	EXPECT_CALL(mocko, vns_main_c_ezframe_new(_,_,_,_,_)).WillOnce(
		DoAll(SaveArg<2>(&len), Return(0)));
	/*expect no call to append buf, as we're adding the first buffer*/
	EXPECT_CALL(mocko, vns_main_c_ezframe_append_buf(_,_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_free(_,_)).Times(0);

	printf("Enable mocks...\n");
	enableMock_vns_main_c_ezframe_new();
	enableMock_vns_main_c_ezframe_append_buf();
	enableMock_vns_main_c_ezframe_free();

	/*create state for first buffer addition*/
	cmem_alvs.conn_sync_state.current_len = total_len + ALVS_STATE_SYNC_HEADROOM;
	cmem_alvs.conn_sync_state.amount_buffers = 1;
	cmem_alvs.conn_sync_state.current_base = frame_data + cmem_alvs.conn_sync_state.current_len;
	cmem_alvs.conn_sync_state.conn_count = 1;

	printf("Calling alvs_state_sync_add_buffer\n");
	rc = vns_main_c_alvs_state_sync_add_buffer();

	printf("Disable mocks...\n");
	disableMock_vns_main_c_ezframe_new();
	disableMock_vns_main_c_ezframe_append_buf();
	disableMock_vns_main_c_ezframe_free();

	EXPECT_EQ(2, cmem_alvs.conn_sync_state.amount_buffers);
	EXPECT_EQ(0, cmem_alvs.conn_sync_state.current_len);
	EXPECT_EQ(1, cmem_alvs.conn_sync_state.conn_count);
	EXPECT_EQ(frame_data, cmem_alvs.conn_sync_state.current_base);
	EXPECT_EQ(total_len, len);
	EXPECT_EQ(0, rc);
}

/*testing adding buffer to current sync frame:
 * with first buffer & failure
 */
TEST_F(PrePostTest, add_buffer_first_buffer_failure)
{
	int rc;
	dut_namespace_uint8_t len;
	int total_len = IP_HEADER_LEN + ETH_HEADER_LEN + UDP_HEADER_LEN + SYNC_HEADER_LEN + SYNC_CONN_LEN;

	printf("Starting test add_buffer_first_buffer_failure...\n");
	EXPECT_CALL(mocko, vns_main_c_ezframe_new(_,_,_,_,_)).WillOnce(
		DoAll(SaveArg<2>(&len), Return(1)));
	/*expect no call to append buf, as we're adding the first buffer*/
	EXPECT_CALL(mocko, vns_main_c_ezframe_append_buf(_,_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_free(_,_)).Times(0);

	printf("Enable mocks...\n");
	enableMock_vns_main_c_ezframe_new();
	enableMock_vns_main_c_ezframe_append_buf();
	enableMock_vns_main_c_ezframe_free();

	/*create state for first buffer addition*/
	cmem_alvs.conn_sync_state.current_len = total_len + ALVS_STATE_SYNC_HEADROOM;
	cmem_alvs.conn_sync_state.amount_buffers = 1;
	cmem_alvs.conn_sync_state.current_base = frame_data + cmem_alvs.conn_sync_state.current_len;
	cmem_alvs.conn_sync_state.conn_count = 1;

	printf("Calling alvs_state_sync_add_buffer\n");
	rc = vns_main_c_alvs_state_sync_add_buffer();

	printf("Disable mocks...\n");
	disableMock_vns_main_c_ezframe_new();
	disableMock_vns_main_c_ezframe_append_buf();
	disableMock_vns_main_c_ezframe_free();

	EXPECT_EQ(0, cmem_alvs.conn_sync_state.amount_buffers);
	EXPECT_EQ(0, cmem_alvs.conn_sync_state.current_len);
	EXPECT_EQ(0, cmem_alvs.conn_sync_state.conn_count);
	EXPECT_EQ(frame_data, cmem_alvs.conn_sync_state.current_base);
	EXPECT_EQ(total_len, len);
	EXPECT_EQ(1, rc);
}

/*testing adding buffer to current sync frame:
 * with second buffer & success
 */
TEST_F(PrePostTest, add_buffer_second_buffer_success)
{
	int rc;
	dut_namespace_uint8_t len;

	printf("Starting test add_buffer_second_buffer_success...\n");
	EXPECT_CALL(mocko, vns_main_c_ezframe_append_buf(_,_,_,_)).WillOnce(
		DoAll(SaveArg<2>(&len), Return(0)));
	/*expect no call to ezframe_new, as we're adding the second buffer*/
	EXPECT_CALL(mocko, vns_main_c_ezframe_new(_,_,_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_free(_,_)).Times(0);

	printf("Enable mocks...\n");
	enableMock_vns_main_c_ezframe_new();
	enableMock_vns_main_c_ezframe_append_buf();
	enableMock_vns_main_c_ezframe_free();

	/*create state for first buffer addition*/
	cmem_alvs.conn_sync_state.current_len = 250;
	cmem_alvs.conn_sync_state.amount_buffers = 2;
	cmem_alvs.conn_sync_state.current_base = frame_data + cmem_alvs.conn_sync_state.current_len;
	cmem_alvs.conn_sync_state.conn_count = 30;

	printf("Calling alvs_state_sync_add_buffer\n");
	rc = vns_main_c_alvs_state_sync_add_buffer();

	printf("Disable mocks...\n");
	disableMock_vns_main_c_ezframe_new();
	disableMock_vns_main_c_ezframe_append_buf();
	disableMock_vns_main_c_ezframe_free();

	EXPECT_EQ(3, cmem_alvs.conn_sync_state.amount_buffers);
	EXPECT_EQ(0, cmem_alvs.conn_sync_state.current_len);
	EXPECT_EQ(30, cmem_alvs.conn_sync_state.conn_count);
	EXPECT_EQ(frame_data, cmem_alvs.conn_sync_state.current_base);
	EXPECT_EQ(250, len);
	EXPECT_EQ(0, rc);
}

/*testing adding buffer to current sync frame:
 * with second buffer & failure
 */
TEST_F(PrePostTest, add_buffer_second_buffer_failure)
{
	int rc;
	dut_namespace_uint8_t len;

	printf("Starting test add_buffer_second_buffer_failure...\n");
	EXPECT_CALL(mocko, vns_main_c_ezframe_append_buf(_,_,_,_)).WillOnce(
		DoAll(SaveArg<2>(&len), Return(1)));
	/*expect no call to ezframe_new, as we're adding the second buffer*/
	EXPECT_CALL(mocko, vns_main_c_ezframe_new(_,_,_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_free(_,_)).WillOnce(Return());

	printf("Enable mocks...\n");
	enableMock_vns_main_c_ezframe_new();
	enableMock_vns_main_c_ezframe_append_buf();
	enableMock_vns_main_c_ezframe_free();

	/*create state for first buffer addition*/
	cmem_alvs.conn_sync_state.current_len = 250;
	cmem_alvs.conn_sync_state.amount_buffers = 2;
	cmem_alvs.conn_sync_state.current_base = frame_data + cmem_alvs.conn_sync_state.current_len;
	cmem_alvs.conn_sync_state.conn_count = 30;

	printf("Calling alvs_state_sync_add_buffer\n");
	rc = vns_main_c_alvs_state_sync_add_buffer();

	printf("Disable mocks...\n");
	disableMock_vns_main_c_ezframe_new();
	disableMock_vns_main_c_ezframe_append_buf();
	disableMock_vns_main_c_ezframe_free();

	EXPECT_EQ(0, cmem_alvs.conn_sync_state.amount_buffers);
	EXPECT_EQ(0, cmem_alvs.conn_sync_state.current_len);
	EXPECT_EQ(0, cmem_alvs.conn_sync_state.conn_count);
	EXPECT_EQ(frame_data, cmem_alvs.conn_sync_state.current_base);
	EXPECT_EQ(250, len);
	EXPECT_EQ(1, rc);
}

/*testing sending aggregated sync frame
 * with no buffers. should do nothing.
 */
TEST_F(PrePostTest, send_aggr_no_buffers)
{
	int logical_id = -1;

	printf("Starting test send_aggr_no_buffers...\n");
	EXPECT_CALL(mocko, vns_main_c_nw_send_frame_to_network(_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_add_buffer()).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_first_buf(_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_free(_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_store_buf(_,_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_load_buf(_,_,_,_)).Times(0);


	printf("Enable mocks...\n");
	enableMock_vns_main_c_nw_send_frame_to_network();
	enableMock_vns_main_c_alvs_state_sync_add_buffer();
	enableMock_vns_main_c_ezframe_first_buf();
	enableMock_vns_main_c_ezframe_free();
	enableMock_vns_main_c_ezframe_store_buf();
	enableMock_vns_main_c_ezframe_load_buf();

	/*create state*/
	cmem_alvs.conn_sync_state.current_len = 0;
	cmem_alvs.conn_sync_state.amount_buffers = 0;
	cmem_alvs.conn_sync_state.current_base = frame_data + cmem_alvs.conn_sync_state.current_len;
	cmem_alvs.conn_sync_state.conn_count = 0;

	printf("Calling alvs_state_sync_send_aggr\n");
	vns_main_c_alvs_state_sync_send_aggr();

	printf("Disable mocks...\n");
	disableMock_vns_main_c_nw_send_frame_to_network();
	disableMock_vns_main_c_alvs_state_sync_add_buffer();
	disableMock_vns_main_c_ezframe_first_buf();
	disableMock_vns_main_c_ezframe_free();
	disableMock_vns_main_c_ezframe_store_buf();
	disableMock_vns_main_c_ezframe_load_buf();

	EXPECT_EQ(-1, logical_id);
}

/*testing sending aggregated sync frame
 * aggregated frame with a single buffer and single connection that was already added (current_len=0)
 * no need to update headers.
 */
TEST_F(PrePostTest, send_aggr_success_one_buf_added)
{
	int logical_id = -1;

	printf("Starting test send_aggr_success_one_buf_added...\n");
	EXPECT_CALL(mocko, vns_main_c_nw_send_frame_to_network(_,_,_)).WillOnce(
			DoAll(SaveArg<2>(&logical_id), Return()));
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_add_buffer()).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_first_buf(_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_free(_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_store_buf(_,_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_load_buf(_,_,_,_)).Times(0);


	printf("Enable mocks...\n");
	enableMock_vns_main_c_nw_send_frame_to_network();
	enableMock_vns_main_c_alvs_state_sync_add_buffer();
	enableMock_vns_main_c_ezframe_first_buf();
	enableMock_vns_main_c_ezframe_free();
	enableMock_vns_main_c_ezframe_store_buf();
	enableMock_vns_main_c_ezframe_load_buf();

	/*create state*/
	cmem_alvs.conn_sync_state.current_len = 0;
	cmem_alvs.conn_sync_state.amount_buffers = 1;
	cmem_alvs.conn_sync_state.current_base = frame_data + cmem_alvs.conn_sync_state.current_len;
	cmem_alvs.conn_sync_state.conn_count = 1;

	printf("Calling alvs_state_sync_send_aggr\n");
	vns_main_c_alvs_state_sync_send_aggr();

	printf("Disable mocks...\n");
	disableMock_vns_main_c_nw_send_frame_to_network();
	disableMock_vns_main_c_alvs_state_sync_add_buffer();
	disableMock_vns_main_c_ezframe_first_buf();
	disableMock_vns_main_c_ezframe_free();
	disableMock_vns_main_c_ezframe_store_buf();
	disableMock_vns_main_c_ezframe_load_buf();

	EXPECT_EQ(0, logical_id);
}

/*testing sending aggregated sync frame
 * aggregated frame with a single buffer and single connection that wasn't added (current_len=0)
 * no need to update headers.
 */
TEST_F(PrePostTest, send_aggr_success_one_buf_not_added)
{
	int logical_id = -1;

	printf("Starting test send_aggr_success_one_buf_not_added...\n");
	EXPECT_CALL(mocko, vns_main_c_nw_send_frame_to_network(_,_,_)).WillOnce(
			DoAll(SaveArg<2>(&logical_id), Return()));
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_add_buffer()).WillOnce(Return(0));
	EXPECT_CALL(mocko, vns_main_c_ezframe_first_buf(_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_free(_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_store_buf(_,_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_load_buf(_,_,_,_)).Times(0);


	printf("Enable mocks...\n");
	enableMock_vns_main_c_nw_send_frame_to_network();
	enableMock_vns_main_c_alvs_state_sync_add_buffer();
	enableMock_vns_main_c_ezframe_first_buf();
	enableMock_vns_main_c_ezframe_free();
	enableMock_vns_main_c_ezframe_store_buf();
	enableMock_vns_main_c_ezframe_load_buf();

	/*create state*/
	cmem_alvs.conn_sync_state.current_len = 250;
	cmem_alvs.conn_sync_state.amount_buffers = 1;
	cmem_alvs.conn_sync_state.current_base = frame_data + cmem_alvs.conn_sync_state.current_len;
	cmem_alvs.conn_sync_state.conn_count = 1;

	printf("Calling alvs_state_sync_send_aggr\n");
	vns_main_c_alvs_state_sync_send_aggr();

	printf("Disable mocks...\n");
	disableMock_vns_main_c_nw_send_frame_to_network();
	disableMock_vns_main_c_alvs_state_sync_add_buffer();
	disableMock_vns_main_c_ezframe_first_buf();
	disableMock_vns_main_c_ezframe_free();
	disableMock_vns_main_c_ezframe_store_buf();
	disableMock_vns_main_c_ezframe_load_buf();

	EXPECT_EQ(0, logical_id);
}

/*testing sending aggregated sync frame
 * aggregated frame with a single buffer and single connection that wasn't added (current_len=0)
 * no need to update headers.
 * fail to add buffer.
 */
TEST_F(PrePostTest, send_aggr_fail_one_buf_not_added)
{
	int logical_id = -1;

	printf("Starting test send_aggr_fail_one_buf_not_added...\n");
	EXPECT_CALL(mocko, vns_main_c_nw_send_frame_to_network(_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_add_buffer()).WillOnce(Return(1));
	EXPECT_CALL(mocko, vns_main_c_ezframe_first_buf(_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_free(_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_store_buf(_,_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_load_buf(_,_,_,_)).Times(0);


	printf("Enable mocks...\n");
	enableMock_vns_main_c_nw_send_frame_to_network();
	enableMock_vns_main_c_alvs_state_sync_add_buffer();
	enableMock_vns_main_c_ezframe_first_buf();
	enableMock_vns_main_c_ezframe_free();
	enableMock_vns_main_c_ezframe_store_buf();
	enableMock_vns_main_c_ezframe_load_buf();

	/*create state*/
	cmem_alvs.conn_sync_state.current_len = 250;
	cmem_alvs.conn_sync_state.amount_buffers = 1;
	cmem_alvs.conn_sync_state.current_base = frame_data + cmem_alvs.conn_sync_state.current_len;
	cmem_alvs.conn_sync_state.conn_count = 1;

	printf("Calling alvs_state_sync_send_aggr\n");
	vns_main_c_alvs_state_sync_send_aggr();

	printf("Disable mocks...\n");
	disableMock_vns_main_c_nw_send_frame_to_network();
	disableMock_vns_main_c_alvs_state_sync_add_buffer();
	disableMock_vns_main_c_ezframe_first_buf();
	disableMock_vns_main_c_ezframe_free();
	disableMock_vns_main_c_ezframe_store_buf();
	disableMock_vns_main_c_ezframe_load_buf();

	EXPECT_EQ(-1, logical_id);
}

/*testing sending aggregated sync frame
 * aggregated frame with a multiple buffers and conns
 * need to update headers.
 */
TEST_F(PrePostTest, send_aggr_success_multiple_bufs_conns)
{
	dut_namespace_uint8_t *frame_base;
	void *store_buf_frame_base;
	int logical_id = -1;
	int conn_count = 40;
	struct dut_namespace_net_hdr  *net_hdr_info;
	struct dut_namespace_alvs_state_sync_header *sync_hdr;

	net_hdr_info = (struct dut_namespace_net_hdr *)((dut_namespace_uint8_t *)(struct dut_namespace_ether_header *)frame_data + sizeof(struct dut_namespace_ether_header));
	sync_hdr = (struct dut_namespace_alvs_state_sync_header *)((dut_namespace_uint8_t *)net_hdr_info + sizeof(struct dut_namespace_net_hdr));

	printf("Starting test send_aggr_success_multiple_bufs_conns...\n");
	EXPECT_CALL(mocko, vns_main_c_nw_send_frame_to_network(_,_,_)).WillOnce(
			DoAll(SaveArg<2>(&logical_id), Return()));
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_add_buffer()).WillOnce(Return(0));
	EXPECT_CALL(mocko, vns_main_c_ezframe_first_buf(_,_)).WillOnce(Return(0));
	EXPECT_CALL(mocko, vns_main_c_ezframe_free(_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_store_buf(_,_,_,_)).WillOnce(DoAll(SaveArg<1>(&store_buf_frame_base), Return(0)));
	EXPECT_CALL(mocko, vns_main_c_ezframe_load_buf(_,_,_,_)).WillOnce(Return(frame_data));


	printf("Enable mocks...\n");
	enableMock_vns_main_c_nw_send_frame_to_network();
	enableMock_vns_main_c_alvs_state_sync_add_buffer();
	enableMock_vns_main_c_ezframe_first_buf();
	enableMock_vns_main_c_ezframe_free();
	enableMock_vns_main_c_ezframe_store_buf();
	enableMock_vns_main_c_ezframe_load_buf();

	/*create state*/
	cmem_alvs.conn_sync_state.current_len = 250;
	cmem_alvs.conn_sync_state.amount_buffers = 2;
	cmem_alvs.conn_sync_state.current_base = frame_data + cmem_alvs.conn_sync_state.current_len;
	cmem_alvs.conn_sync_state.conn_count = conn_count;

	printf("Calling alvs_state_sync_send_aggr\n");
	vns_main_c_alvs_state_sync_send_aggr();

	printf("Disable mocks...\n");
	disableMock_vns_main_c_nw_send_frame_to_network();
	disableMock_vns_main_c_alvs_state_sync_add_buffer();
	disableMock_vns_main_c_ezframe_first_buf();
	disableMock_vns_main_c_ezframe_free();
	disableMock_vns_main_c_ezframe_store_buf();
	disableMock_vns_main_c_ezframe_load_buf();

	EXPECT_EQ(0, logical_id);
	EXPECT_EQ(frame_data, (dut_namespace_uint8_t *)store_buf_frame_base);
	EXPECT_EQ(conn_count, sync_hdr->conn_count);
	EXPECT_EQ(SYNC_HEADER_LEN + SYNC_CONN_LEN*conn_count, sync_hdr->size);
	EXPECT_EQ(UDP_HEADER_LEN + SYNC_HEADER_LEN + SYNC_CONN_LEN*conn_count, net_hdr_info->udp.len);
	EXPECT_EQ(IP_HEADER_LEN + UDP_HEADER_LEN + SYNC_HEADER_LEN + SYNC_CONN_LEN*conn_count, net_hdr_info->ipv4.tot_len);
	EXPECT_EQ(EZFRAME_CHECKSUM_VALUE, net_hdr_info->ipv4.check);
}

/*testing sending aggregated sync frame
 * aggregated frame with a multiple buffers and conns
 * fail switching to first buffer
 */
TEST_F(PrePostTest, send_aggr_fail_first_buffer)
{
	int logical_id = -1;
	int conn_count = 40;

	printf("Starting test send_aggr_fail_first_buffer...\n");
	EXPECT_CALL(mocko, vns_main_c_nw_send_frame_to_network(_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_add_buffer()).WillOnce(Return(0));
	EXPECT_CALL(mocko, vns_main_c_ezframe_first_buf(_,_)).WillOnce(Return(1));
	EXPECT_CALL(mocko, vns_main_c_ezframe_free(_,_)).WillOnce(Return());
	EXPECT_CALL(mocko, vns_main_c_ezframe_store_buf(_,_,_,_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_ezframe_load_buf(_,_,_,_)).Times(0);


	printf("Enable mocks...\n");
	enableMock_vns_main_c_nw_send_frame_to_network();
	enableMock_vns_main_c_alvs_state_sync_add_buffer();
	enableMock_vns_main_c_ezframe_first_buf();
	enableMock_vns_main_c_ezframe_free();
	enableMock_vns_main_c_ezframe_store_buf();
	enableMock_vns_main_c_ezframe_load_buf();

	/*create state*/
	cmem_alvs.conn_sync_state.current_len = 250;
	cmem_alvs.conn_sync_state.amount_buffers = 2;
	cmem_alvs.conn_sync_state.current_base = frame_data + cmem_alvs.conn_sync_state.current_len;
	cmem_alvs.conn_sync_state.conn_count = conn_count;

	printf("Calling alvs_state_sync_send_aggr\n");
	vns_main_c_alvs_state_sync_send_aggr();

	printf("Disable mocks...\n");
	disableMock_vns_main_c_nw_send_frame_to_network();
	disableMock_vns_main_c_alvs_state_sync_add_buffer();
	disableMock_vns_main_c_ezframe_first_buf();
	disableMock_vns_main_c_ezframe_free();
	disableMock_vns_main_c_ezframe_store_buf();
	disableMock_vns_main_c_ezframe_load_buf();

	EXPECT_EQ(-1, logical_id);
}

/*testing aggregating sync connections.
 * fail on server info lookup for bound connection
 */
TEST_F(PrePostTest, sync_aggr_fail_lookup_server_info)
{
	dut_namespace_uint8_t sync_id = 7;
	dut_namespace_in_addr_t source_ip = SOURCE_IP;
	dut_namespace_uint8_t conn_count = 40;
	dut_namespace_uint8_t current_len = 0;
	dut_namespace_uint8_t *current_base = frame_data;
	unsigned amount_buffers = 2;

	printf("Starting test sync_aggr_fail_lookup_server_info...\n");
	EXPECT_CALL(mocko, vns_main_c_alvs_server_info_lookup(_)).WillOnce(Return(1));
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_add_buffer()).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_set_sync_conn(_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_send_aggr()).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_first(_,_)).Times(0);


	printf("Enable mocks...\n");
	enableMock_vns_main_c_alvs_server_info_lookup();
	enableMock_vns_main_c_alvs_state_sync_add_buffer();
	enableMock_vns_main_c_alvs_state_sync_set_sync_conn();
	enableMock_vns_main_c_alvs_state_sync_send_aggr();
	enableMock_vns_main_c_alvs_state_sync_first();

	/*connection state*/
	cmem_alvs.conn_info_result.bound = true;
	/*create state*/
	cmem_alvs.conn_sync_state.current_len = current_len;
	cmem_alvs.conn_sync_state.amount_buffers = amount_buffers;
	cmem_alvs.conn_sync_state.current_base = current_base;
	cmem_alvs.conn_sync_state.conn_count = conn_count;

	printf("Calling alvs_state_sync_send_aggr\n");
	vns_main_c_alvs_state_sync_aggr(source_ip, sync_id);

	printf("Disable mocks...\n");
	disableMock_vns_main_c_alvs_server_info_lookup();
	disableMock_vns_main_c_alvs_state_sync_add_buffer();
	disableMock_vns_main_c_alvs_state_sync_set_sync_conn();
	disableMock_vns_main_c_alvs_state_sync_send_aggr();
	disableMock_vns_main_c_alvs_state_sync_first();

	EXPECT_EQ(current_len, cmem_alvs.conn_sync_state.current_len);
	EXPECT_EQ(amount_buffers, cmem_alvs.conn_sync_state.amount_buffers);
	EXPECT_EQ(current_base, cmem_alvs.conn_sync_state.current_base);
	EXPECT_EQ(conn_count, cmem_alvs.conn_sync_state.conn_count);
}


/*testing aggregating sync connections.
 * new frame (no current buffers) & unbounded connection
 */
TEST_F(PrePostTest, sync_aggr_success_no_buffers)
{
	struct dut_namespace_net_hdr  *net_hdr_info;
	struct dut_namespace_alvs_state_sync_conn *sync_conn;
	struct dut_namespace_alvs_state_sync_header *sync_hdr;
	struct dut_namespace_ether_header *eth_header;
	dut_namespace_uint8_t sync_id = 7;
	dut_namespace_uint8_t conn_count = 0;
	dut_namespace_uint8_t current_len = 0;
	dut_namespace_uint8_t *current_base = frame_data;
	unsigned amount_buffers = 0;
	int total_len = IP_HEADER_LEN + ETH_HEADER_LEN + UDP_HEADER_LEN + SYNC_HEADER_LEN + SYNC_CONN_LEN;
	dut_namespace_uint8_t *frame_base = frame_data + total_len;

	eth_header = (struct dut_namespace_ether_header *)frame_data;
	net_hdr_info = (struct dut_namespace_net_hdr *)((dut_namespace_uint8_t *)eth_header + sizeof(struct dut_namespace_ether_header));
	sync_hdr = (struct dut_namespace_alvs_state_sync_header *)((dut_namespace_uint8_t *)net_hdr_info + sizeof(struct dut_namespace_net_hdr));
	sync_conn = (struct dut_namespace_alvs_state_sync_conn *)((dut_namespace_uint8_t *)sync_hdr + sizeof(struct dut_namespace_alvs_state_sync_header));

	printf("Starting test sync_aggr_success_no_buffers...\n");
	EXPECT_CALL(mocko, vns_main_c_alvs_server_info_lookup(_)).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_add_buffer()).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_send_aggr()).Times(0);


	printf("Enable mocks...\n");
	enableMock_vns_main_c_alvs_server_info_lookup();
	enableMock_vns_main_c_alvs_state_sync_add_buffer();
	enableMock_vns_main_c_alvs_state_sync_send_aggr();

	/*connection state*/
	cmem_alvs.conn_info_result.bound = false;
	/*create state*/
	cmem_alvs.conn_sync_state.current_len = current_len;
	cmem_alvs.conn_sync_state.amount_buffers = amount_buffers;
	cmem_alvs.conn_sync_state.current_base = current_base;
	cmem_alvs.conn_sync_state.conn_count = conn_count;

	printf("Calling alvs_state_sync_send_aggr\n");
	vns_main_c_alvs_state_sync_aggr(SOURCE_IP, sync_id);

	printf("Disable mocks...\n");
	disableMock_vns_main_c_alvs_server_info_lookup();
	disableMock_vns_main_c_alvs_state_sync_add_buffer();
	disableMock_vns_main_c_alvs_state_sync_send_aggr();

	/*verify state*/
	EXPECT_EQ(ALVS_STATE_SYNC_HEADROOM + total_len, cmem_alvs.conn_sync_state.current_len);
	EXPECT_EQ(1, cmem_alvs.conn_sync_state.amount_buffers);
	EXPECT_EQ(frame_base, cmem_alvs.conn_sync_state.current_base);
	EXPECT_EQ(1, cmem_alvs.conn_sync_state.conn_count);
	/*verify first buffer content*/
	/*eth header info*/
	expect_eth_hdr(eth_header);
	/*net header info*/
	expect_net_hdr(net_hdr_info, 1);
	/*sync header*/
	expect_sync_hdr(sync_hdr, 1, sync_id);
	/*sync conn*/
	expect_sync_conn(sync_conn, 0, 7); /*UNBOUNDED, CLOSE_WAIT*/
}

/*testing aggregating sync connections.
 * current frame & buffer have enough room for another connection.
 */
TEST_F(PrePostTest, sync_aggr_success_same_buffer)
{
	struct dut_namespace_alvs_state_sync_conn *sync_conn;
	dut_namespace_uint8_t sync_id = 7;
	dut_namespace_uint8_t conn_count = 6;
	dut_namespace_uint8_t current_len = 220;
	dut_namespace_uint8_t *current_base = frame_data + current_len;
	unsigned amount_buffers = 2;

	sync_conn = (struct dut_namespace_alvs_state_sync_conn *)current_base;

	printf("Starting test sync_aggr_success_no_buffers...\n");
	EXPECT_CALL(mocko, vns_main_c_alvs_server_info_lookup(_)).WillOnce(Return(0));
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_add_buffer()).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_send_aggr()).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_first(_,_)).Times(0);


	printf("Enable mocks...\n");
	enableMock_vns_main_c_alvs_server_info_lookup();
	enableMock_vns_main_c_alvs_state_sync_add_buffer();
	enableMock_vns_main_c_alvs_state_sync_send_aggr();
	enableMock_vns_main_c_alvs_state_sync_first();

	/*create state*/
	cmem_alvs.conn_sync_state.current_len = current_len;
	cmem_alvs.conn_sync_state.amount_buffers = amount_buffers;
	cmem_alvs.conn_sync_state.current_base = current_base;
	cmem_alvs.conn_sync_state.conn_count = conn_count;

	printf("Calling alvs_state_sync_send_aggr\n");
	vns_main_c_alvs_state_sync_aggr(SOURCE_IP, sync_id);

	printf("Disable mocks...\n");
	disableMock_vns_main_c_alvs_server_info_lookup();
	disableMock_vns_main_c_alvs_state_sync_add_buffer();
	disableMock_vns_main_c_alvs_state_sync_send_aggr();
	disableMock_vns_main_c_alvs_state_sync_first();

	/*verify state*/
	EXPECT_EQ(current_len + sizeof(struct dut_namespace_alvs_state_sync_conn), cmem_alvs.conn_sync_state.current_len);
	EXPECT_EQ(2, cmem_alvs.conn_sync_state.amount_buffers);
	EXPECT_EQ(current_base + sizeof(struct dut_namespace_alvs_state_sync_conn), cmem_alvs.conn_sync_state.current_base);
	EXPECT_EQ(7, cmem_alvs.conn_sync_state.conn_count);
	/*verify added conn*/
	expect_sync_conn(sync_conn, 1, 7); /*BOUNDED, CLOSE_WAIT*/
}

/*testing aggregating sync connections.
 * no room in current buffer, there is room for another buffer.
 */
TEST_F(PrePostTest, sync_aggr_success_another_buffer)
{
	struct dut_namespace_alvs_state_sync_conn *sync_conn;
	dut_namespace_uint8_t sync_id = 7;
	dut_namespace_uint8_t conn_count = 6;
	dut_namespace_uint8_t current_len = 221;
	dut_namespace_uint8_t *current_base = frame_data + current_len;
	unsigned amount_buffers = 2;

	sync_conn = (struct dut_namespace_alvs_state_sync_conn *)frame_data;

	printf("Starting test sync_aggr_success_no_buffers...\n");
	EXPECT_CALL(mocko, vns_main_c_alvs_server_info_lookup(_)).WillOnce(Return(0));
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_send_aggr()).Times(0);
	EXPECT_CALL(mocko, vns_main_c_alvs_state_sync_first(_,_)).Times(0);


	printf("Enable mocks...\n");
	enableMock_vns_main_c_alvs_server_info_lookup();
	enableMock_vns_main_c_alvs_state_sync_send_aggr();
	enableMock_vns_main_c_alvs_state_sync_first();

	/*create state*/
	cmem_alvs.conn_sync_state.current_len = current_len;
	cmem_alvs.conn_sync_state.amount_buffers = amount_buffers;
	cmem_alvs.conn_sync_state.current_base = current_base;
	cmem_alvs.conn_sync_state.conn_count = conn_count;

	printf("Calling alvs_state_sync_send_aggr\n");
	vns_main_c_alvs_state_sync_aggr(SOURCE_IP, sync_id);

	printf("Disable mocks...\n");
	disableMock_vns_main_c_alvs_server_info_lookup();
	disableMock_vns_main_c_alvs_state_sync_send_aggr();
	disableMock_vns_main_c_alvs_state_sync_first();

	/*verify state*/
	EXPECT_EQ(sizeof(struct dut_namespace_alvs_state_sync_conn), cmem_alvs.conn_sync_state.current_len);
	EXPECT_EQ(3, cmem_alvs.conn_sync_state.amount_buffers);
	EXPECT_EQ(frame_data + sizeof(struct dut_namespace_alvs_state_sync_conn), cmem_alvs.conn_sync_state.current_base);
	EXPECT_EQ(7, cmem_alvs.conn_sync_state.conn_count);
	/*verify added conn*/
	expect_sync_conn(sync_conn, 1, 7); /*BOUNDED, CLOSE_WAIT*/
}

/*TODO add sync_aggr_fail_another_buffer
 * same as sync_aggr_success_another_buffer, just with add_buffer failure.
 */

/*TODO add sync_aggr_another_frame
 * case where cannot add another buffer, send frame and start a new one.
 */

//#####################################################################################

