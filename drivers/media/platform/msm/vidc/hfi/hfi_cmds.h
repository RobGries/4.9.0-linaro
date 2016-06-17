/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 * Copyright 2016 Linaro Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __HFI_CMDS_H__
#define __HFI_CMDS_H__

#include "hfi_helper.h"
#include "hfi.h"
#include "hfi_api.h"

#define call_hfi_pkt_op(hfi, op, args...)			\
	(((hfi) && (hfi)->pkt_ops && (hfi)->pkt_ops->op) ?	\
	((hfi)->pkt_ops->op(args)) : -EINVAL)

struct hfi_packetization_ops {
	void (*sys_init)(struct hfi_sys_init_pkt *pkt, u32 arch_type);
	void (*sys_pc_prep)(struct hfi_sys_pc_prep_pkt *pkt);
	void (*sys_idle_indicator)(struct hfi_sys_set_property_pkt *pkt,
				   u32 enable);
	void (*sys_power_control)(struct hfi_sys_set_property_pkt *pkt,
				  u32 enable);
	int (*sys_set_resource)(struct hfi_sys_set_resource_pkt *pkt,
				struct hal_resource_hdr *resource_hdr,
				void *resource_value);
	int (*sys_release_resource)(struct hfi_sys_release_resource_pkt *pkt,
				    struct hal_resource_hdr *resource_hdr);
	void (*sys_debug_config)(struct hfi_sys_set_property_pkt *pkt, u32 mode,
				 u32 config);
	void (*sys_coverage_config)(struct hfi_sys_set_property_pkt *pkt,
				    u32 mode);
	void (*sys_ping)(struct hfi_sys_ping_pkt *pkt, u32 cookie);
	void (*sys_image_version)(struct hfi_sys_get_property_pkt *pkt);
	void (*ssr_cmd)(enum hal_ssr_trigger_type type,
		        struct hfi_sys_test_ssr_pkt *pkt);
	int (*session_init)(struct hfi_session_init_pkt *pkt,
			    struct hfi_device_inst *inst,
			    u32 session_domain, u32 session_codec);
	void (*session_cmd)(struct hfi_session_pkt *pkt, u32 pkt_type,
			    struct hfi_device_inst *inst);
	int (*session_set_buffers)(struct hfi_session_set_buffers_pkt *pkt,
				   struct hfi_device_inst *inst,
				   struct hal_buffer_addr_info *bai);
	int (*session_release_buffers)(
		struct hfi_session_release_buffer_pkt *pkt,
		struct hfi_device_inst *inst,
		struct hal_buffer_addr_info *bai);
	int (*session_etb_decoder)(
		struct hfi_session_empty_buffer_compressed_pkt *pkt,
		struct hfi_device_inst *inst,
		struct hal_frame_data *input_frame);
	int (*session_etb_encoder)(
		struct hfi_session_empty_buffer_uncompressed_plane0_pkt *pkt,
		struct hfi_device_inst *inst,
		struct hal_frame_data *input_frame);
	int (*session_ftb)(struct hfi_session_fill_buffer_pkt *pkt,
		struct hfi_device_inst *inst,
		struct hal_frame_data *output_frame);
	int (*session_parse_seq_header)(
		struct hfi_session_parse_sequence_header_pkt *pkt,
		struct hfi_device_inst *inst, struct hal_seq_hdr *seq_hdr);
	int (*session_get_seq_hdr)(
		struct hfi_session_get_sequence_header_pkt *pkt,
		struct hfi_device_inst *inst, struct hal_seq_hdr *seq_hdr);
	int (*session_flush)(struct hfi_session_flush_pkt *pkt,
		struct hfi_device_inst *inst, enum hal_flush flush_mode);
	int (*session_get_property)(
		struct hfi_session_get_property_pkt *pkt,
		struct hfi_device_inst *inst, enum hal_property ptype);
	int (*session_set_property)(struct hfi_session_set_property_pkt *pkt,
				    struct hfi_device_inst *inst,
				    enum hal_property ptype, void *pdata);
};

const struct hfi_packetization_ops *
hfi_get_pkt_ops(enum hfi_packetization_type);

#endif
