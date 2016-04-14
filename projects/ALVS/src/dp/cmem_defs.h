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
   	union
   	{
   		char	service_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct alvs_services_result), sizeof(struct alvs_service_key))];
   		char	arp_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct alvs_arp_result), sizeof(struct alvs_arp_key))];
   	};
}__packed alvs_cmem;

extern alvs_cmem  cmem __cmem_var;

/***********************************************************************//**
 * \struct  alvs_shared_cmem
 * \brief
 **************************************************************************/

typedef struct
{
	ezdp_hash_struct_desc_t	services_struct_desc;
	ezdp_hash_struct_desc_t	arp_struct_desc;
}__packed alvs_shared_cmem;

extern alvs_shared_cmem  shared_cmem __cmem_shared_var;

