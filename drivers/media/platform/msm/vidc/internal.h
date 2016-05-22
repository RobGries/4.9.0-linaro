/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_VIDC_INTERNAL_H_
#define _MSM_VIDC_INTERNAL_H_

#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>

#include "hfi/hfi_api.h"

#define VIDC_DRV_NAME		"vidc"
#define DEFAULT_HEIGHT		1088
#define DEFAULT_WIDTH		1920
#define MIN_SUPPORTED_WIDTH	64
#define MIN_SUPPORTED_HEIGHT	64
#define MAX_NAME_LENGTH		64

struct vidc_list {
	struct list_head list;
	struct mutex lock;
};

struct vidc_format {
	u32 pixfmt;
	int num_planes;
	u32 type;
};

struct vidc_drv {
	struct mutex lock;
	struct list_head cores;
	int num_cores;
};

struct vidc_core {
	struct list_head list;
	struct mutex lock;
	struct hfi_device hfi;
	struct video_device vdev_dec;
	struct video_device vdev_enc;
	struct v4l2_device v4l2_dev;
	struct list_head instances;
	enum vidc_hal_type hfi_type;
	struct vidc_resources res;
	struct rproc *rproc;
	bool rproc_booted;
	struct device *dev;
};

struct vdec_controls {
	u32 nal_format;
	u32 output_order;
	u32 post_loop_deb_mode;
	u32 sync_frame_decode;
	u32 frame_assembly;
	u32 profile;
	u32 level;
};

struct venc_controls {
	u16 gop_size;
	u32 idr_period;
	u32 num_p_frames;
	u32 num_b_frames;
	u32 bitrate_mode;
	u32 bitrate;
	u32 bitrate_peak;

	u32 h264_i_period;
	u32 h264_entropy_mode;
	u32 h264_i_qp;
	u32 h264_p_qp;
	u32 h264_b_qp;
	u32 h264_min_qp;
	u32 h264_max_qp;
	u32 h264_loop_filter_mode;
	u32 h264_loop_filter_alpha;
	u32 h264_loop_filter_beta;
	u32 h264_vui_timing_info; /* enable by default ?*/
	u32 h264_au_delimiter; /* generate access unit delimiter */
	u32 h264_vui_bitstream_restrict;
	u32 h264_nal_svc; /* H264/AVC SVC NAL extension */

	u32 mpeg4_time_resolution; /* MPEG4_TIME_RESOLUTION */

	u32 vp8_min_qp;
	u32 vp8_max_qp;
	u32 vpx_error_resilience;

	/* VC1 PERF CFG */
	u32 iframe_xrange;
	u32 iframe_yrange;
	u32 pframe_xrange;
	u32 pframe_yrange;
	u32 bframe_xrange;
	u32 bframe_yrange;

	u32 multi_slice_mode;
	u32 multi_slice_max_bytes;
	u32 multi_slice_max_mb;
	u32 multi_slice_gob;
	u32 slice_delivery_mode;
	u32 header_mode;
	u32 preserve_text_quality;

	/* initial  quantization */
	u32 enable_inital_qp;
	u32 iframe_qp;
	u32 pframe_qp;
	u32 bframe_qp;

	bool deinterlace;
	bool request_seq_header;

	/* LTR */
	u32 use_ltr_frame;
	u32 mark_ltr_frame;
	u32 ltr_mode;
	u32 ltr_count;

	u32 hier_p_num_layers;
	u32 hybrid_hierp_mode;
	u32 perf_mode;

	u32 profile;
	u32 level;
};

#define VIDC_FMT_CAP	BIT(0)
#define VIDC_FMT_OUT	BIT(1)
#define IS_FMTS_SETTLED(f)	(f & VIDC_FMT_CAP && f & VIDC_FMT_OUT)

struct vidc_inst {
	struct list_head list;
	struct mutex lock;
	struct vidc_core *core;

	struct vidc_list scratchbufs;
	struct vidc_list persistbufs;
	struct vidc_list registeredbufs;

	struct list_head bufqueue;
	struct mutex bufqueue_lock;

	int streamoff;
	struct buffer_requirements buff_req;
	struct smem_client *mem_client;
	struct vb2_queue bufq_out;
	struct vb2_queue bufq_cap;
	void *vb2_ctx_cap;
	void *vb2_ctx_out;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl **ctrls;
	struct v4l2_ctrl **cluster;
	union {
		struct vdec_controls dec;
		struct venc_controls enc;
	} controls;
	struct v4l2_fh fh;

	struct hfi_device_inst *hfi_inst;

	/* session fields */
	enum session_type session_type;
	u32 width;
	u32 height;
	u32 out_width;
	u32 out_height;
	u64 fps;
	struct v4l2_fract timeperframe;
	const struct vidc_format *fmt_out;
	const struct vidc_format *fmt_cap;
	u8 fmts_settled;
	unsigned int num_input_bufs;
	unsigned int num_output_bufs;

	struct vidc_core_capability capability;
	bool in_reconfig;
	u32 reconfig_width;
	u32 reconfig_height;

	u64 sequence;

	/* encoder fields */
	atomic_t seq_hdr_reqs;
};

#define ctrl_to_inst(ctrl)	\
	container_of(ctrl->handler, struct vidc_inst, ctrl_handler)

struct vidc_ctrl {
	u32 id;
	char name[MAX_NAME_LENGTH];
	enum v4l2_ctrl_type type;
	s32 min;
	s32 max;
	s32 def;
	u32 step;
	u64 menu_skip_mask;
	u32 flags;
	const char * const *qmenu;
};

struct buffer_info {
	struct list_head list;
	struct hal_buffer_addr_info bai;
};

static inline void INIT_VIDC_LIST(struct vidc_list *mlist)
{
	mutex_init(&mlist->lock);
	INIT_LIST_HEAD(&mlist->list);
}

/* Offset base for buffers on the destination queue - used to distinguish
 * between source and destination buffers when mmapping - they receive the same
 * offsets but for different queues */
#define DST_QUEUE_OFF_BASE	(1 << 30)

extern const struct v4l2_file_operations vidc_fops;

static inline struct vidc_inst *to_inst(struct file *filp)
{
	return container_of(filp->private_data, struct vidc_inst, fh);
}

static inline struct vb2_queue *
vidc_to_vb2q(struct vidc_inst *inst, enum v4l2_buf_type type)
{
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return &inst->bufq_cap;
	else if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return &inst->bufq_out;

	return NULL;
}

#endif
