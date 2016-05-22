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

#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <media/msm-v4l2-controls.h>

#include "smem.h"
#include "common.h"
#include "load.h"

/* helper functions */
#if 0
static void __fill_flags(struct hal_frame_data *frame_data, __u32 vb_flags)
{
	u32 *flags = &frame_data->flags;

	if (vb_flags & V4L2_QCOM_BUF_FLAG_EOS)
		*flags = HAL_BUFFERFLAG_EOS;

	if (vb_flags & V4L2_BUF_FLAG_LAST)
		*flags = HAL_BUFFERFLAG_EOS;

	if (vb_flags & V4L2_MSM_BUF_FLAG_YUV_601_709_CLAMP)
		*flags |= HAL_BUFFERFLAG_YUV_601_709_CSC_CLAMP;

	if (vb_flags & V4L2_QCOM_BUF_FLAG_CODECCONFIG)
		*flags |= HAL_BUFFERFLAG_CODECCONFIG;

	if (vb_flags & V4L2_QCOM_BUF_FLAG_DECODEONLY)
		*flags |= HAL_BUFFERFLAG_DECODEONLY;

	if (vb_flags & V4L2_QCOM_BUF_TS_DISCONTINUITY)
		*flags |= HAL_BUFFERFLAG_TS_DISCONTINUITY;

	if (vb_flags & V4L2_QCOM_BUF_TS_ERROR)
		*flags |= HAL_BUFFERFLAG_TS_ERROR;

	if (vb_flags & V4L2_QCOM_BUF_TIMESTAMP_INVALID)
		frame_data->timestamp = LLONG_MAX;
}
#endif

int vidc_set_session_buf(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vb2_queue *q = vb->vb2_queue;
	struct vidc_inst *inst = vb2_get_drv_priv(q);
	struct vidc_core *core = inst->core;
	struct device *dev = core->dev;
	struct hfi_device *hfi = &core->hfi;
	struct vidc_buffer *buf = to_vidc_buffer(vbuf);
	struct hal_frame_data fdata;
	s64 time_usec;
	int ret;

	time_usec = timeval_to_ns(&vbuf->timestamp);
	do_div(time_usec, NSEC_PER_USEC);

	memset(&fdata, 0 , sizeof(fdata));

	fdata.alloc_len = vb2_plane_size(vb, 0);
	fdata.device_addr = buf->dma_addr;
	fdata.timestamp = time_usec;
	fdata.flags = 0;
	fdata.clnt_data = buf->dma_addr;

	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fdata.buffer_type = HAL_BUFFER_INPUT;
		fdata.filled_len = vb2_get_plane_payload(vb, 0);
		fdata.offset = vb->planes[0].data_offset;

		if (vbuf->flags & V4L2_BUF_FLAG_LAST || !fdata.filled_len)
			fdata.flags |= HAL_BUFFERFLAG_EOS;

		ret = vidc_hfi_session_etb(hfi, inst->hfi_inst, &fdata);
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fdata.buffer_type = HAL_BUFFER_OUTPUT;
		fdata.filled_len = 0;
		fdata.offset = 0;

		ret = vidc_hfi_session_ftb(hfi, inst->hfi_inst, &fdata);
	} else {
		ret = -EINVAL;
	}

	if (ret) {
		dev_err(dev, "failed to set session buffer (%d)\n", ret);
		return ret;
	}

	dev_dbg(dev, "%s: type:%d, addr:%x, alloc_len:%u, filled_len:%u, offset:%u\n",
		__func__, q->type, fdata.device_addr, fdata.alloc_len,
		fdata.filled_len, fdata.offset);

	return 0;
}

int vidc_rel_session_bufs(struct vidc_inst *inst)
{
	struct device *dev = inst->core->dev;
	struct hfi_device *hfi = &inst->core->hfi;
	struct hal_buffer_addr_info *bai;
	struct buffer_info *bi, *tmp;
	int ret = 0;

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(bi, tmp, &inst->registeredbufs.list, list) {
		list_del(&bi->list);
		bai = &bi->bai;
		bai->response_required = 1;
		ret = vidc_hfi_session_release_buffers(hfi, inst->hfi_inst,
							bai);
		if (ret) {
			dev_err(dev, "%s: session release buffers failed\n",
				__func__);
			break;
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);

	return ret;
}

int vidc_get_bufreqs(struct vidc_inst *inst)
{
	struct hfi_device *hfi = &inst->core->hfi;
	struct device *dev = inst->core->dev;
	enum hal_property ptype = HAL_PARAM_GET_BUFFER_REQUIREMENTS;
	union hal_get_property hprop;
	int ret, i;

	ret = vidc_hfi_session_get_property(hfi, inst->hfi_inst, ptype, &hprop);
	if (ret)
		return ret;

	memcpy(&inst->buff_req, &hprop.buf_req, sizeof(inst->buff_req));

	for (i = 0; i < HAL_BUFFER_MAX; i++)
		dev_dbg(dev,
			"buftype: %03x, actual count: %02d, size: %d, "
			"count min: %d, hold count: %d, region size: %d "
			"contiguous: %u, alignment: %u\n",
			inst->buff_req.buffer[i].type,
			inst->buff_req.buffer[i].count_actual,
			inst->buff_req.buffer[i].size,
			inst->buff_req.buffer[i].count_min,
			inst->buff_req.buffer[i].hold_count,
			inst->buff_req.buffer[i].region_size,
			inst->buff_req.buffer[i].contiguous,
			inst->buff_req.buffer[i].alignment);

	dev_dbg(dev, "\n");

	dev_dbg(dev, "Input buffers: %d, Output buffers: %d\n",
		inst->buff_req.buffer[0].count_actual,
		inst->buff_req.buffer[1].count_actual);

	return 0;
}

struct hal_buffer_requirements *
vidc_get_buff_req_buffer(struct vidc_inst *inst, enum hal_buffer_type type)
{
	unsigned int i;

	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		if (inst->buff_req.buffer[i].type == type)
			return &inst->buff_req.buffer[i];
	}

	return NULL;
}

int vidc_bufrequirements(struct vidc_inst *inst, enum hal_buffer_type type,
			 struct hal_buffer_requirements *out)
{
	struct hfi_device *hfi = &inst->core->hfi;
	struct device *dev = hfi->dev;
	enum hal_property ptype = HAL_PARAM_GET_BUFFER_REQUIREMENTS;
	union hal_get_property hprop;
	int ret, i;

	ret = vidc_hfi_session_get_property(hfi, inst->hfi_inst, ptype, &hprop);
	if (ret)
		return ret;

	ret = -EINVAL;

	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		if (hprop.buf_req.buffer[i].type != type)
			continue;

		if (out)
			memcpy(out, &hprop.buf_req.buffer[i], sizeof(*out));
		ret = 0;
		break;
	}

	dev_dbg(dev, "buftype: %03x, actual count: %02d, size: %d, "
		     "count min: %d, hold count: %d, region size: %d\n",
		out->type,
		out->count_actual,
		out->size,
		out->count_min,
		out->hold_count,
		out->region_size);

	return ret;
}

struct vb2_v4l2_buffer *
vidc_get_vb2buffer(struct vidc_inst *inst, dma_addr_t addr)
{
	struct vidc_buffer *buf;
	struct vb2_v4l2_buffer *vb = NULL;

	mutex_lock(&inst->bufqueue_lock);

	list_for_each_entry(buf, &inst->bufqueue, list) {
		if (buf->dma_addr == addr) {
			vb = &buf->vb;
			break;
		}
	}

	if (vb)
		list_del(&buf->list);

	mutex_unlock(&inst->bufqueue_lock);

	return vb;
}

int vidc_session_flush(struct vidc_inst *inst, u32 flags)
{
	struct vidc_core *core = inst->core;
	struct hfi_device_inst *hfi_inst = inst->hfi_inst;
	struct device *dev = core->dev;
	struct hfi_device *hfi = &core->hfi;
	bool ip_flush = false;
	bool op_flush = false;
	int ret =  0;

	ip_flush = flags & V4L2_QCOM_CMD_FLUSH_OUTPUT;
	op_flush = flags & V4L2_QCOM_CMD_FLUSH_CAPTURE;

	if (ip_flush && !op_flush) {
		dev_err(dev, "Input only flush not supported\n");
		return 0;
	}

	if (hfi_inst->state == INST_INVALID || hfi->state == CORE_INVALID) {
		dev_err(dev, "Core %p and inst %p are in bad state\n",
			core, inst);
		return 0;
	}

	if (inst->in_reconfig && !ip_flush && op_flush) {
		ret = call_hfi_op(hfi, session_flush, inst->hfi_inst,
				  HAL_FLUSH_OUTPUT);
	} else {
		/*
		 * If flush is called after queueing buffers but before
		 * streamon driver should flush the pending queue
		 */

		/* Do not send flush in case of session_error */
		if (!(hfi_inst->state == INST_INVALID &&
		      hfi->state != CORE_INVALID))
			ret = call_hfi_op(hfi, session_flush,
					  inst->hfi_inst,
					  HAL_FLUSH_ALL);
	}

	return ret;
}

enum hal_extradata_id vidc_extradata_index(enum v4l2_mpeg_vidc_extradata index)
{
	switch (index) {
	case V4L2_MPEG_VIDC_EXTRADATA_NONE:
		return HAL_EXTRADATA_NONE;
	case V4L2_MPEG_VIDC_EXTRADATA_MB_QUANTIZATION:
		return HAL_EXTRADATA_MB_QUANTIZATION;
	case V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO:
		return HAL_EXTRADATA_INTERLACE_VIDEO;
	case V4L2_MPEG_VIDC_EXTRADATA_VC1_FRAMEDISP:
		return HAL_EXTRADATA_VC1_FRAMEDISP;
	case V4L2_MPEG_VIDC_EXTRADATA_VC1_SEQDISP:
		return HAL_EXTRADATA_VC1_SEQDISP;
	case V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP:
		return HAL_EXTRADATA_TIMESTAMP;
	case V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING:
		return HAL_EXTRADATA_S3D_FRAME_PACKING;
	case V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE:
		return HAL_EXTRADATA_FRAME_RATE;
	case V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW:
		return HAL_EXTRADATA_PANSCAN_WINDOW;
	case V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI:
		return HAL_EXTRADATA_RECOVERY_POINT_SEI;
	case V4L2_MPEG_VIDC_EXTRADATA_MULTISLICE_INFO:
		return HAL_EXTRADATA_MULTISLICE_INFO;
	case V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB:
		return HAL_EXTRADATA_NUM_CONCEALED_MB;
	case V4L2_MPEG_VIDC_EXTRADATA_METADATA_FILLER:
		return HAL_EXTRADATA_METADATA_FILLER;
	case V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO:
		return HAL_EXTRADATA_ASPECT_RATIO;
	case V4L2_MPEG_VIDC_EXTRADATA_INPUT_CROP:
		return HAL_EXTRADATA_INPUT_CROP;
	case V4L2_MPEG_VIDC_EXTRADATA_DIGITAL_ZOOM:
		return HAL_EXTRADATA_DIGITAL_ZOOM;
	case V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP:
		return HAL_EXTRADATA_MPEG2_SEQDISP;
	case V4L2_MPEG_VIDC_EXTRADATA_STREAM_USERDATA:
		return HAL_EXTRADATA_STREAM_USERDATA;
	case V4L2_MPEG_VIDC_EXTRADATA_FRAME_QP:
		return HAL_EXTRADATA_FRAME_QP;
	case V4L2_MPEG_VIDC_EXTRADATA_FRAME_BITS_INFO:
		return HAL_EXTRADATA_FRAME_BITS_INFO;
	case V4L2_MPEG_VIDC_EXTRADATA_LTR:
		return HAL_EXTRADATA_LTR_INFO;
	case V4L2_MPEG_VIDC_EXTRADATA_METADATA_MBI:
		return HAL_EXTRADATA_METADATA_MBI;
	default:
		return V4L2_MPEG_VIDC_EXTRADATA_NONE;
	}

	return 0;
}

enum hal_buffer_layout_type
vidc_buffer_layout(enum v4l2_mpeg_vidc_video_mvc_layout index)
{
	switch (index) {
	case V4L2_MPEG_VIDC_VIDEO_MVC_SEQUENTIAL:
		return HAL_BUFFER_LAYOUT_SEQ;
	case V4L2_MPEG_VIDC_VIDEO_MVC_TOP_BOTTOM:
		return HAL_BUFFER_LAYOUT_TOP_BOTTOM;
	default:
		break;
	}

	return HAL_UNUSED_BUFFER_LAYOUT;
}

int vidc_set_color_format(struct vidc_inst *inst, enum hal_buffer_type type,
			  u32 pixfmt)
{
	struct hal_uncompressed_format_select hal_fmt;
	struct hfi_device *hfi = &inst->core->hfi;
	struct device *dev = inst->core->dev;
	enum hal_property ptype = HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT;
	int ret;

	hal_fmt.buffer_type = type;

	switch (pixfmt) {
	case V4L2_PIX_FMT_NV12:
		hal_fmt.format = HAL_COLOR_FORMAT_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
		hal_fmt.format = HAL_COLOR_FORMAT_NV21;
		break;
	default:
		return -ENOTSUPP;
	}

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype,
					    &hal_fmt);
	if (ret) {
		dev_err(dev, "set uncompressed color format failed (%d)\n",
			ret);
		return ret;
	}

	return 0;
}
