/*
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 Linaro Ltd.
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
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-sg.h>

#include "core.h"
#include "helpers.h"
#include "vdec.h"

static u32 get_framesize_uncompressed(unsigned int plane, u32 width, u32 height)
{
	u32 y_stride, uv_stride, y_plane;
	u32 y_sclines, uv_sclines, uv_plane;
	u32 size;

	y_stride = ALIGN(width, 128);
	uv_stride = ALIGN(width, 128);
	y_sclines = ALIGN(height, 32);
	uv_sclines = ALIGN(((height + 1) >> 1), 16);

	y_plane = y_stride * y_sclines;
	uv_plane = uv_stride * uv_sclines + SZ_4K;
	size = y_plane + uv_plane + SZ_8K;

	return ALIGN(size, SZ_4K);
}

static u32 get_framesize_compressed(unsigned int width, unsigned int height)
{
	return ((width * height * 3 / 2) / 2) + 128;
}

static const struct venus_format vdec_formats[] = {
	{
		.pixfmt = V4L2_PIX_FMT_NV12,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_MPEG4,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_MPEG2,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_H263,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_VC1_ANNEX_G,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_VC1_ANNEX_L,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_H264,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_VP8,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_XVID,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	},
};

static const struct venus_format *find_format(u32 pixfmt, u32 type)
{
	const struct venus_format *fmt = vdec_formats;
	unsigned int size = ARRAY_SIZE(vdec_formats);
	unsigned int i;

	for (i = 0; i < size; i++) {
		if (fmt[i].pixfmt == pixfmt)
			break;
	}

	if (i == size || fmt[i].type != type)
		return NULL;

	return &fmt[i];
}

static const struct venus_format *find_format_by_index(int index, u32 type)
{
	const struct venus_format *fmt = vdec_formats;
	unsigned int size = ARRAY_SIZE(vdec_formats);
	int i, k = 0;

	if (index < 0 || index > size)
		return NULL;

	for (i = 0; i < size; i++) {
		if (fmt[i].type != type)
			continue;
		if (k == index)
			break;
		k++;
	}

	if (i == size)
		return NULL;

	return &fmt[i];
}

static const struct venus_format *
vdec_try_fmt_common(struct venus_inst *inst, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pixmp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = pixmp->plane_fmt;
	const struct venus_format *fmt;
	unsigned int p;

	memset(pfmt[0].reserved, 0, sizeof(pfmt[0].reserved));
	memset(pixmp->reserved, 0, sizeof(pixmp->reserved));

	fmt = find_format(pixmp->pixelformat, f->type);
	if (!fmt) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			pixmp->pixelformat = V4L2_PIX_FMT_NV12;
		else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			pixmp->pixelformat = V4L2_PIX_FMT_H264;
		else
			return NULL;
		fmt = find_format(pixmp->pixelformat, f->type);
		pixmp->width = 1280;
		pixmp->height = 720;
	}

	pixmp->width = clamp(pixmp->width, inst->cap_width.min,
			     inst->cap_width.max);
	pixmp->height = clamp(pixmp->height, inst->cap_height.min,
			      inst->cap_height.max);

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		pixmp->height = ALIGN(pixmp->height, 32);

	if (pixmp->field == V4L2_FIELD_ANY)
		pixmp->field = V4L2_FIELD_NONE;
	pixmp->num_planes = fmt->num_planes;
	pixmp->flags = 0;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		for (p = 0; p < pixmp->num_planes; p++) {
			pfmt[p].sizeimage =
				get_framesize_uncompressed(p, pixmp->width,
							   pixmp->height);
			pfmt[p].bytesperline = ALIGN(pixmp->width, 128);
		}
	} else {
		pfmt[0].sizeimage = get_framesize_compressed(pixmp->width,
							     pixmp->height);
		pfmt[0].bytesperline = 0;
	}

	return fmt;
}

static int vdec_try_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct venus_inst *inst = to_inst(file);

	vdec_try_fmt_common(inst, f);

	return 0;
}

static int vdec_g_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct venus_inst *inst = to_inst(file);
	const struct venus_format *fmt = NULL;
	struct v4l2_pix_format_mplane *pixmp = &f->fmt.pix_mp;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		fmt = inst->fmt_cap;
	else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		fmt = inst->fmt_out;

	if (inst->reconfig) {
		struct v4l2_format format = {};

		inst->out_width = inst->reconfig_width;
		inst->out_height = inst->reconfig_height;
		inst->reconfig = false;

		format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		format.fmt.pix_mp.pixelformat = inst->fmt_cap->pixfmt;
		format.fmt.pix_mp.width = inst->out_width;
		format.fmt.pix_mp.height = inst->out_height;

		vdec_try_fmt_common(inst, &format);

		inst->width = format.fmt.pix_mp.width;
		inst->height = format.fmt.pix_mp.height;
	}

	pixmp->pixelformat = fmt->pixfmt;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pixmp->width = inst->width;
		pixmp->height = inst->height;
		pixmp->colorspace = inst->colorspace;
		pixmp->ycbcr_enc = inst->ycbcr_enc;
		pixmp->quantization = inst->quantization;
		pixmp->xfer_func = inst->xfer_func;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		pixmp->width = inst->out_width;
		pixmp->height = inst->out_height;
	}

	vdec_try_fmt_common(inst, f);

	return 0;
}

static int vdec_s_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct venus_inst *inst = to_inst(file);
	struct v4l2_pix_format_mplane *pixmp = &f->fmt.pix_mp;
	struct v4l2_pix_format_mplane orig_pixmp;
	const struct venus_format *fmt;
	struct v4l2_format format;
	u32 pixfmt_out = 0, pixfmt_cap = 0;

	orig_pixmp = *pixmp;

	fmt = vdec_try_fmt_common(inst, f);

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		pixfmt_out = pixmp->pixelformat;
		pixfmt_cap = inst->fmt_cap->pixfmt;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pixfmt_cap = pixmp->pixelformat;
		pixfmt_out = inst->fmt_out->pixfmt;
	}

	memset(&format, 0, sizeof(format));

	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	format.fmt.pix_mp.pixelformat = pixfmt_out;
	format.fmt.pix_mp.width = orig_pixmp.width;
	format.fmt.pix_mp.height = orig_pixmp.height;
	vdec_try_fmt_common(inst, &format);

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		inst->out_width = format.fmt.pix_mp.width;
		inst->out_height = format.fmt.pix_mp.height;
		inst->colorspace = pixmp->colorspace;
		inst->ycbcr_enc = pixmp->ycbcr_enc;
		inst->quantization = pixmp->quantization;
		inst->xfer_func = pixmp->xfer_func;
	}

	memset(&format, 0, sizeof(format));

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	format.fmt.pix_mp.pixelformat = pixfmt_cap;
	format.fmt.pix_mp.width = orig_pixmp.width;
	format.fmt.pix_mp.height = orig_pixmp.height;
	vdec_try_fmt_common(inst, &format);

	inst->width = format.fmt.pix_mp.width;
	inst->height = format.fmt.pix_mp.height;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		inst->fmt_out = fmt;
	else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		inst->fmt_cap = fmt;

	return 0;
}

static int
vdec_g_selection(struct file *file, void *fh, struct v4l2_selection *s)
{
	struct venus_inst *inst = to_inst(file);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP:
		if (s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
			return -EINVAL;
		s->r.width = inst->out_width;
		s->r.height = inst->out_height;
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_PADDED:
		if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		s->r.width = inst->width;
		s->r.height = inst->height;
		break;
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE:
		if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		s->r.width = inst->out_width;
		s->r.height = inst->out_height;
		break;
	default:
		return -EINVAL;
	}

	s->r.top = 0;
	s->r.left = 0;

	return 0;
}

static int
vdec_s_selection(struct file *file, void *fh, struct v4l2_selection *s)
{
	struct venus_inst *inst = to_inst(file);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP:
		return -EINVAL;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE_PADDED:
		return -EINVAL;
	case V4L2_SEL_TGT_COMPOSE:
		if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		s->r.width = inst->out_width;
		s->r.height = inst->out_height;
		break;
	default:
		return -EINVAL;
	}

	s->r.top = 0;
	s->r.left = 0;

	return 0;
}

static int
vdec_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	strlcpy(cap->driver, "qcom-venus", sizeof(cap->driver));
	strlcpy(cap->card, "Qualcomm Venus video decoder", sizeof(cap->card));
	strlcpy(cap->bus_info, "platform:qcom-venus", sizeof(cap->bus_info));

	cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int vdec_enum_fmt(struct file *file, void *fh, struct v4l2_fmtdesc *f)
{
	const struct venus_format *fmt;

	memset(f->reserved, 0, sizeof(f->reserved));

	fmt = find_format_by_index(f->index, f->type);
	if (!fmt)
		return -EINVAL;

	f->pixelformat = fmt->pixfmt;

	return 0;
}

static int vdec_s_parm(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct venus_inst *inst = to_inst(file);
	struct v4l2_captureparm *cap = &a->parm.capture;
	struct v4l2_fract *timeperframe = &cap->timeperframe;
	u64 us_per_frame, fps;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
	    a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	memset(cap->reserved, 0, sizeof(cap->reserved));
	if (!timeperframe->denominator)
		timeperframe->denominator = inst->timeperframe.denominator;
	if (!timeperframe->numerator)
		timeperframe->numerator = inst->timeperframe.numerator;
	cap->readbuffers = 0;
	cap->extendedmode = 0;
	cap->capability = V4L2_CAP_TIMEPERFRAME;
	us_per_frame = timeperframe->numerator * (u64)USEC_PER_SEC;
	do_div(us_per_frame, timeperframe->denominator);

	if (!us_per_frame)
		return -EINVAL;

	fps = (u64)USEC_PER_SEC;
	do_div(fps, us_per_frame);

	inst->fps = fps;
	inst->timeperframe = *timeperframe;

	return 0;
}

static int vdec_enum_framesizes(struct file *file, void *fh,
				struct v4l2_frmsizeenum *fsize)
{
	struct venus_inst *inst = to_inst(file);
	const struct venus_format *fmt;

	fmt = find_format(fsize->pixel_format,
			  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (!fmt) {
		fmt = find_format(fsize->pixel_format,
				  V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (!fmt)
			return -EINVAL;
	}

	if (fsize->index)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;

	fsize->stepwise.min_width = inst->cap_width.min;
	fsize->stepwise.max_width = inst->cap_width.max;
	fsize->stepwise.step_width = inst->cap_width.step_size;
	fsize->stepwise.min_height = inst->cap_height.min;
	fsize->stepwise.max_height = inst->cap_height.max;
	fsize->stepwise.step_height = inst->cap_height.step_size;

	return 0;
}

static int vdec_subscribe_event(struct v4l2_fh *fh,
				const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subscribe_event(fh, sub);
	default:
		return -EINVAL;
	}
}

static const struct v4l2_ioctl_ops vdec_ioctl_ops = {
	.vidioc_querycap = vdec_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = vdec_enum_fmt,
	.vidioc_enum_fmt_vid_out_mplane = vdec_enum_fmt,
	.vidioc_s_fmt_vid_cap_mplane = vdec_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = vdec_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane = vdec_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = vdec_g_fmt,
	.vidioc_try_fmt_vid_cap_mplane = vdec_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = vdec_try_fmt,
	.vidioc_g_selection = vdec_g_selection,
	.vidioc_s_selection = vdec_s_selection,
	.vidioc_reqbufs = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf = v4l2_m2m_ioctl_querybuf,
	.vidioc_create_bufs = v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf = v4l2_m2m_ioctl_prepare_buf,
	.vidioc_qbuf = v4l2_m2m_ioctl_qbuf,
	.vidioc_expbuf = v4l2_m2m_ioctl_expbuf,
	.vidioc_dqbuf = v4l2_m2m_ioctl_dqbuf,
	.vidioc_streamon = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff = v4l2_m2m_ioctl_streamoff,
	.vidioc_s_parm = vdec_s_parm,
	.vidioc_enum_framesizes = vdec_enum_framesizes,
	.vidioc_subscribe_event = vdec_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static int vdec_set_properties(struct venus_inst *inst)
{
	struct vdec_controls *ctr = &inst->controls.dec;
	struct venus_core *core = inst->core;
	struct hfi_enable en = { .enable = 1 };
	u32 ptype;
	int ret;

	ptype = HFI_PROPERTY_PARAM_VDEC_CONTINUE_DATA_TRANSFER;
	ret = hfi_session_set_property(inst, ptype, &en);
	if (ret)
		return ret;

	if (core->res->hfi_version == HFI_VERSION_3XX) {
		struct hfi_buffer_alloc_mode mode;

		ptype = HFI_PROPERTY_PARAM_BUFFER_ALLOC_MODE;
		mode.type = HFI_BUFFER_OUTPUT;
		mode.mode = HFI_BUFFER_MODE_DYNAMIC;

		ret = hfi_session_set_property(inst, ptype, &mode);
		if (ret)
			return ret;
	}

	if (ctr->post_loop_deb_mode) {
		ptype = HFI_PROPERTY_CONFIG_VDEC_POST_LOOP_DEBLOCKER;
		en.enable = 1;
		ret = hfi_session_set_property(inst, ptype, &en);
		if (ret)
			return ret;
	}

	return 0;
}

static int vdec_init_session(struct venus_inst *inst)
{
	int ret;

	ret = hfi_session_init(inst, inst->fmt_out->pixfmt);
	if (ret)
		return ret;

	ret = helper_set_input_resolution(inst, inst->out_width,
					  inst->out_height);
	if (ret)
		goto deinit;

	ret = helper_set_color_format(inst, inst->fmt_cap->pixfmt);
	if (ret)
		goto deinit;

	return 0;
deinit:
	hfi_session_deinit(inst);
	return ret;
}

static int vdec_cap_num_buffers(struct venus_inst *inst, unsigned int *num)
{
	struct venus_core *core = inst->core;
	struct hfi_buffer_requirements bufreq;
	struct device *dev = core->dev;
	int ret, ret2;

	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		return ret;

	ret = vdec_init_session(inst);
	if (ret)
		goto put_sync;

	ret = helper_get_bufreq(inst, HFI_BUFFER_OUTPUT, &bufreq);

	*num = bufreq.count_actual;

	hfi_session_deinit(inst);

put_sync:
	ret2 = pm_runtime_put_sync(dev);

	return ret ? ret : ret2;
}

static int vdec_queue_setup(struct vb2_queue *q, const void *parg,
			    unsigned int *num_buffers, unsigned int *num_planes,
			    unsigned int sizes[], void *alloc_ctxs[])
{
	struct venus_inst *inst = vb2_get_drv_priv(q);
	const struct v4l2_format *fmt = parg;
	const struct v4l2_pix_format_mplane *pixmp;
	unsigned int p, num;
	int ret = 0;

	if (fmt) {
		pixmp = &fmt->fmt.pix_mp;
		if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
		    (pixmp->width > inst->out_width ||
		    pixmp->height > inst->out_height ||
		    pixmp->plane_fmt[0].sizeimage > inst->input_buf_size))
			return -EINVAL;
		else if (pixmp->width > inst->width ||
			 pixmp->height > inst->height ||
			 pixmp->plane_fmt[0].sizeimage < inst->output_buf_size)
			return -EINVAL;
	}

	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*num_planes = inst->fmt_out->num_planes;
		sizes[0] = get_framesize_compressed(inst->out_width,
						    inst->out_height);
		inst->input_buf_size = sizes[0];
		inst->num_input_bufs = *num_buffers;
		alloc_ctxs[0] = inst->alloc_ctx_out;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		*num_planes = inst->fmt_cap->num_planes;

		ret = vdec_cap_num_buffers(inst, &num);
		if (ret)
			break;

		*num_buffers = max(*num_buffers, num);

		for (p = 0; p < *num_planes; p++)
			sizes[p] = get_framesize_uncompressed(p, inst->width,
							      inst->height);

		inst->num_output_bufs = *num_buffers;
		inst->output_buf_size = sizes[0];
		alloc_ctxs[0] = inst->alloc_ctx_cap;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int vdec_verify_conf(struct venus_inst *inst)
{
	struct hfi_buffer_requirements bufreq;
	int ret;

	if (!inst->num_input_bufs || !inst->num_output_bufs)
		return -EINVAL;

	ret = helper_get_bufreq(inst, HFI_BUFFER_OUTPUT, &bufreq);
	if (ret)
		return ret;

	if (inst->num_output_bufs < bufreq.count_actual ||
	    inst->num_output_bufs < bufreq.count_min)
		return -EINVAL;

	ret = helper_get_bufreq(inst, HFI_BUFFER_INPUT, &bufreq);
	if (ret)
		return ret;

	if (inst->num_input_bufs < bufreq.count_min)
		return -EINVAL;

	return 0;
}

static int vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct venus_inst *inst = vb2_get_drv_priv(q);
	struct venus_core *core = inst->core;
	struct device *dev = core->dev;
	u32 ptype;
	int ret;

	mutex_lock(&inst->lock);

	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		inst->streamon_out = 1;
	else
		inst->streamon_cap = 1;

	if (!(inst->streamon_out & inst->streamon_cap)) {
		mutex_unlock(&inst->lock);
		return 0;
	}

	inst->reconfig = false;
	inst->sequence = 0;
	inst->codec_cfg = false;

	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		return ret;

	ret = vdec_init_session(inst);
	if (ret)
		goto put_sync;

	ret = vdec_set_properties(inst);
	if (ret)
		goto deinit_sess;

	if (core->res->hfi_version == HFI_VERSION_3XX) {
		struct hfi_buffer_size_actual buf_sz;

		ptype = HFI_PROPERTY_PARAM_BUFFER_SIZE_ACTUAL;
		buf_sz.type = HFI_BUFFER_OUTPUT;
		buf_sz.size = inst->output_buf_size;

		ret = hfi_session_set_property(inst, ptype, &buf_sz);
		if (ret)
			goto deinit_sess;
	}

	ret = vdec_verify_conf(inst);
	if (ret)
		goto deinit_sess;

	ret = helper_set_num_bufs(inst, inst->num_input_bufs,
				  inst->num_output_bufs);
	if (ret)
		goto deinit_sess;

	ret = helper_vb2_start_streaming(inst);
	if (ret)
		goto deinit_sess;

	mutex_unlock(&inst->lock);

	return 0;

deinit_sess:
	hfi_session_deinit(inst);
put_sync:
	pm_runtime_put_sync(dev);
	helper_buffers_done(inst, VB2_BUF_STATE_QUEUED);
	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		inst->streamon_out = 0;
	else
		inst->streamon_cap = 0;
	mutex_unlock(&inst->lock);
	return ret;
}

static const struct vb2_ops vdec_vb2_ops = {
	.queue_setup = vdec_queue_setup,
	.buf_init = helper_vb2_buf_init,
	.buf_prepare = helper_vb2_buf_prepare,
	.start_streaming = vdec_start_streaming,
	.stop_streaming = helper_vb2_stop_streaming,
	.buf_queue = helper_vb2_buf_queue,
};

static void vdec_buf_done(struct venus_inst *inst, unsigned int buf_type,
			  u32 tag, u32 bytesused, u32 data_offset, u32 flags,
			  u64 timestamp_us)
{
	struct vb2_v4l2_buffer *vbuf;
	struct vb2_buffer *vb;
	unsigned int type;

	if (buf_type == HFI_BUFFER_INPUT)
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	vbuf = helper_find_buf(inst, type, tag);
	if (!vbuf)
		return;

	vbuf->flags = flags;

	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		vb = &vbuf->vb2_buf;
		vb->planes[0].bytesused =
			max_t(unsigned int, inst->output_buf_size, bytesused);
		vb->planes[0].data_offset = data_offset;
		vbuf->timestamp = ns_to_timeval(timestamp_us * NSEC_PER_USEC);
		vbuf->sequence = inst->sequence++;

		if (vbuf->flags & V4L2_BUF_FLAG_LAST) {
			const struct v4l2_event ev = { .type = V4L2_EVENT_EOS };

			v4l2_event_queue_fh(&inst->fh, &ev);
		}
	}

	v4l2_m2m_buf_done(vbuf, VB2_BUF_STATE_DONE);
}

static void vdec_event_notify(struct venus_inst *inst, u32 event,
			      struct hfi_event_data *data)
{
	struct venus_core *core = inst->core;
	struct device *dev = core->dev;
	static const struct v4l2_event ev = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION };

	switch (event) {
	case EVT_SESSION_ERROR:
		inst->session_error = true;
		dev_err(dev, "dec: event session error %x\n", inst->error);
		break;
	case EVT_SYS_EVENT_CHANGE:
		switch (data->event_type) {
		case HFI_EVENT_DATA_SEQUENCE_CHANGED_SUFFICIENT_BUF_RESOURCES:
			hfi_session_continue(inst);
			dev_dbg(dev, "event sufficient resources\n");
			break;
		case HFI_EVENT_DATA_SEQUENCE_CHANGED_INSUFFICIENT_BUF_RESOURCES:
			inst->reconfig_height = data->height;
			inst->reconfig_width = data->width;
			inst->reconfig = true;

			v4l2_event_queue_fh(&inst->fh, &ev);

			dev_dbg(dev, "event not sufficient resources (%ux%u)\n",
				data->width, data->height);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

static const struct hfi_inst_ops vdec_hfi_ops = {
	.buf_done = vdec_buf_done,
	.event_notify = vdec_event_notify,
};

static void vdec_inst_init(struct venus_inst *inst)
{
	inst->fmt_out = &vdec_formats[6];
	inst->fmt_cap = &vdec_formats[0];
	inst->width = 1280;
	inst->height = ALIGN(720, 32);
	inst->out_width = 1280;
	inst->out_height = 720;
	inst->fps = 30;
	inst->timeperframe.numerator = 1;
	inst->timeperframe.denominator = 30;

	inst->cap_width.min = 64;
	inst->cap_width.max = 1920;
	inst->cap_width.step_size = 1;
	inst->cap_height.min = 64;
	inst->cap_height.max = ALIGN(1080, 32);
	inst->cap_height.step_size = 1;
	inst->cap_framerate.min = 1;
	inst->cap_framerate.max = 30;
	inst->cap_framerate.step_size = 1;
	inst->cap_mbs_per_frame.min = 16;
	inst->cap_mbs_per_frame.max = 8160;
}

static const struct v4l2_m2m_ops vdec_m2m_ops = {
	.device_run = helper_m2m_device_run,
	.job_abort = helper_m2m_job_abort,
};

static int m2m_queue_init(void *priv, struct vb2_queue *src_vq,
			  struct vb2_queue *dst_vq)
{
	struct venus_inst *inst = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->ops = &vdec_vb2_ops;
	src_vq->mem_ops = &vb2_dma_sg_memops;
	src_vq->drv_priv = inst;
	src_vq->buf_struct_size = sizeof(struct venus_buffer);
	src_vq->allow_zero_bytesused = 1;
	src_vq->min_buffers_needed = 1;
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->ops = &vdec_vb2_ops;
	dst_vq->mem_ops = &vb2_dma_sg_memops;
	dst_vq->drv_priv = inst;
	dst_vq->buf_struct_size = sizeof(struct venus_buffer);
	dst_vq->allow_zero_bytesused = 1;
	dst_vq->min_buffers_needed = 1;
	ret = vb2_queue_init(dst_vq);
	if (ret) {
		vb2_queue_release(src_vq);
		return ret;
	}

	return 0;
}

int vdec_init(struct venus_core *core, struct video_device *dec,
	      const struct v4l2_file_operations *fops)
{
	int ret;

	dec->release = video_device_release;
	dec->fops = fops;
	dec->ioctl_ops = &vdec_ioctl_ops;
	dec->vfl_dir = VFL_DIR_M2M;
	dec->v4l2_dev = &core->v4l2_dev;

	ret = video_register_device(dec, VFL_TYPE_GRABBER, -1);
	if (ret)
		return ret;

	video_set_drvdata(dec, core);

	return 0;
}

void vdec_deinit(struct venus_core *core, struct video_device *dec)
{
	video_unregister_device(dec);
}

int vdec_open(struct venus_inst *inst)
{
	int ret;

	inst->alloc_ctx_cap = vb2_dma_sg_init_ctx(inst->core->dev);
	if (!inst->alloc_ctx_cap)
		return -ENOMEM;

	inst->alloc_ctx_out = vb2_dma_sg_init_ctx(inst->core->dev);
	if (!inst->alloc_ctx_out) {
		vb2_dma_sg_cleanup_ctx(inst->alloc_ctx_cap);
		return -ENOMEM;
	}

	ret = vdec_ctrl_init(inst);
	if (ret)
		goto err_alloc_ctx;

	ret = hfi_session_create(inst, &vdec_hfi_ops);
	if (ret)
		goto err_ctrl_deinit;

	vdec_inst_init(inst);

	/*
	 * create m2m device for every instance, the m2m context scheduling
	 * is made by firmware side so we do not need to care about.
	 */
	inst->m2m_dev = v4l2_m2m_init(&vdec_m2m_ops);
	if (IS_ERR(inst->m2m_dev)) {
		ret = PTR_ERR(inst->m2m_dev);
		goto err_session_destroy;
	}

	inst->m2m_ctx = v4l2_m2m_ctx_init(inst->m2m_dev, inst, m2m_queue_init);
	if (IS_ERR(inst->m2m_ctx)) {
		ret = PTR_ERR(inst->m2m_ctx);
		goto err_m2m_release;
	}

	return 0;

err_m2m_release:
	v4l2_m2m_release(inst->m2m_dev);
err_session_destroy:
	hfi_session_destroy(inst);
err_ctrl_deinit:
	vdec_ctrl_deinit(inst);
err_alloc_ctx:
	vb2_dma_sg_cleanup_ctx(inst->alloc_ctx_cap);
	vb2_dma_sg_cleanup_ctx(inst->alloc_ctx_out);
	return ret;
}

void vdec_close(struct venus_inst *inst)
{
	v4l2_m2m_ctx_release(inst->m2m_ctx);
	v4l2_m2m_release(inst->m2m_dev);
	vdec_ctrl_deinit(inst);
	hfi_session_destroy(inst);
	vb2_dma_sg_cleanup_ctx(inst->alloc_ctx_cap);
	vb2_dma_sg_cleanup_ctx(inst->alloc_ctx_out);
}
