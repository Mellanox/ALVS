/* Copyright (c) 2016 Mellanox Technologies, Ltd. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. Neither the names of the copyright holders nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
*
* Alternatively, this software may be distributed under the terms of the
* GNU General Public License ("GPL") version 2 as published by the Free
* Software Foundation.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

/***************** global CMEM data *************************/



/***********************************************************************//**
 * \struct  alvs_cmem
 * \brief
 **************************************************************************/
typedef struct
{
	ezframe_t							frame 								;
	/**< Frame	*/
	uint8_t								frame_data[EZFRAME_BUF_DATA_SIZE] 	;
	/**< Frame data buffer */
	struct ezdp_decode_mac_result	 	mac_decode_result					;
	/**< Result of Decode MAC */
	struct ezdp_decode_ipv4_result	 	ipv4_decode_result					;
	/**< Result of Decode MAC */
	struct alvs_service_key				service_key							;
	/**< service key */
	struct alvs_arp_key					arp_key								;
	/**< arp key */
	struct  dp_interface_result			interface_result					;
	/**< interface result */
   	union
   	{
   		char	service_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct alvs_service_result), sizeof(struct alvs_service_key))];
   		char	arp_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct alvs_arp_result), sizeof(struct alvs_arp_key))];
   		char    table_work_area[EZDP_TABLE_PRM_WORK_AREA_SIZE];
   	};
}__packed alvs_cmem;

extern alvs_cmem  cmem __cmem_var;

/***********************************************************************//**
 * \struct  alvs_shared_cmem
 * \brief
 **************************************************************************/

typedef struct
{
	ezdp_table_struct_desc_t	interface_struct_desc;
	ezdp_hash_struct_desc_t		services_struct_desc;
	ezdp_hash_struct_desc_t		arp_struct_desc;
	struct ezdp_sum_addr		nw_interface_stats_base_address;
}__packed alvs_shared_cmem;

extern alvs_shared_cmem  shared_cmem __cmem_shared_var;

