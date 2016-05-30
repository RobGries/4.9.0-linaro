/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_VIDC_COMMON_H_
#define _MSM_VIDC_COMMON_H_

#include <linux/kernel.h>
#include <linux/list.h>
#include <media/videobuf2-v4l2.h>
#include <media/msm-v4l2-controls.h>

#include "internal.h"

struct vidc_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
	dma_addr_t dma_addr;
	struct buffer_info bi;
};

#define to_vidc_buffer(buf)	container_of(buf, struct vidc_buffer, vb)

struct vb2_v4l2_buffer *
vidc_get_vb2buffer(struct vidc_inst *inst, dma_addr_t addr);
int vidc_vb2_buf_init(struct vb2_buffer *vb);
int vidc_vb2_buf_prepare(struct vb2_buffer *vb);
void vidc_vb2_buf_queue(struct vb2_buffer *vb);
void vidc_vb2_stop_streaming(struct vb2_queue *q);
int vidc_vb2_start_streaming(struct vidc_inst *inst);
int vidc_session_flush(struct vidc_inst *inst, u32 flags);
int vidc_get_bufreqs(struct vidc_inst *inst);
struct hal_buffer_requirements *vidc_get_buff_req_buffer(struct vidc_inst *inst,
							 u32 buffer_type);
/* TODO: delete above buffer requirements api's and use below one */
int vidc_bufrequirements(struct vidc_inst *inst, enum hal_buffer_type type,
			 struct hal_buffer_requirements *out);
enum hal_extradata_id vidc_extradata_index(enum v4l2_mpeg_vidc_extradata index);
enum hal_buffer_layout_type
vidc_buffer_layout(enum v4l2_mpeg_vidc_video_mvc_layout index);
int vidc_set_color_format(struct vidc_inst *inst, enum hal_buffer_type type,
			  u32 pixfmt);
int vidc_set_session_buf(struct vb2_buffer *vb);
int vidc_rel_session_bufs(struct vidc_inst *inst);

#endif
