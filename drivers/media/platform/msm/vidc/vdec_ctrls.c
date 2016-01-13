/*
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
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
#define DEBUG
#include <linux/cpumask.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <media/msm-v4l2-controls.h>
#include <media/v4l2-ctrls.h>

#include "common.h"

static const char * const mpeg_video_stream_format[] = {
	"NAL Format Start Codes",
	"NAL Format One NAL Per Buffer",
	"NAL Format One Byte Length",
	"NAL Format Two Byte Length",
	"NAL Format Four Byte Length",
	NULL
};
static const char * const mpeg_video_output_order[] = {
	"Display Order",
	"Decode Order",
	NULL
};

#if 0
static const char * const h263_level[] = {
	"1.0",
	"2.0",
	"3.0",
	"4.0",
	"4.5",
	"5.0",
	"6.0",
	"7.0",
};

static const char * const h263_profile[] = {
	"Baseline",
	"H320 Coding",
	"Backward Compatible",
	"ISWV2",
	"ISWV3",
	"High Compression",
	"Internet",
	"Interlace",
	"High Latency",
};

static const char * const vp8_profile_level[] = {
	"Unused",
	"0.0",
	"1.0",
	"2.0",
	"3.0",
};

static const char * const mpeg2_profile[] = {
	"Simple",
	"Main",
	"422",
	"SNR scalable",
	"Spatial scalable",
	"High",
};

static const char * const mpeg2_level[] = {
	"Level 0",
	"Level 1",
	"Level 2",
	"Level 3",
};
#endif

static struct vidc_ctrl vdec_ctrls[] = {
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT,
		.name = "NAL Format",
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES,
		.max = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_FOUR_BYTE_LENGTH,
		.def = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_ONE_NAL_PER_BUFFER) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_ONE_BYTE_LENGTH) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_TWO_BYTE_LENGTH) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_FOUR_BYTE_LENGTH)),
		.qmenu = mpeg_video_stream_format,
	}, {
		.id = V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER,
		.name = "Output Order",
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY,
		.max = V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DECODE,
		.def = V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY) |
			(1 << V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DECODE)),
		.qmenu = mpeg_video_output_order,
	}, {
		.id = V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE,
		.name = "Sync Frame Decode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE,
		.max = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_ENABLE,
		.def = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE,
		.step = 1,
	}, {
		.id = V4L2_CID_MPEG_VIDC_VIDEO_FRAME_ASSEMBLY,
		.name = "Video frame assembly",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = V4L2_MPEG_VIDC_FRAME_ASSEMBLY_DISABLE,
		.max = V4L2_MPEG_VIDC_FRAME_ASSEMBLY_ENABLE,
		.def =  V4L2_MPEG_VIDC_FRAME_ASSEMBLY_DISABLE,
		.step = 1,
	}, {
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,
		.max = V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_CODING_EFFICIENCY,
		.def = V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_MPEG4_LEVEL_0,
		.max = V4L2_MPEG_VIDEO_MPEG4_LEVEL_5,
		.def = V4L2_MPEG_VIDEO_MPEG4_LEVEL_0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.max = V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH,
		.def = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.max = V4L2_MPEG_VIDEO_H264_LEVEL_5_2,
		.def = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.id = V4L2_CID_MPEG_VIDEO_VPX_PROFILE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 3,
		.step = 1,
		.def = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.id = V4L2_CID_MPEG_VIDEO_DECODER_MPEG4_DEBLOCK_FILTER,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.def = 0,
	},
};

#define NUM_CTRLS	ARRAY_SIZE(vdec_ctrls)

#define IS_PRIV_CTRL(idx)	( \
		(V4L2_CTRL_ID2CLASS(idx) == V4L2_CTRL_CLASS_MPEG) && \
		V4L2_CTRL_DRIVER_PRIV(idx))

static int vdec_v4l2_to_hal(int id, int value)
{
	switch (id) {
		/* H264 */
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
			return HAL_H264_PROFILE_BASELINE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
			return HAL_H264_PROFILE_CONSTRAINED_BASE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
			return HAL_H264_PROFILE_MAIN;
		case V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED:
			return HAL_H264_PROFILE_EXTENDED;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
			return HAL_H264_PROFILE_HIGH;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10:
			return HAL_H264_PROFILE_HIGH10;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422:
			return HAL_H264_PROFILE_HIGH422;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE:
			return HAL_H264_PROFILE_HIGH444;
		default:
			return -EINVAL;
		}
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_0:
			return HAL_H264_LEVEL_1;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1B:
			return HAL_H264_LEVEL_1b;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_1:
			return HAL_H264_LEVEL_11;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_2:
			return HAL_H264_LEVEL_12;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_3:
			return HAL_H264_LEVEL_13;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_0:
			return HAL_H264_LEVEL_2;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_1:
			return HAL_H264_LEVEL_21;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_2:
			return HAL_H264_LEVEL_22;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_0:
			return HAL_H264_LEVEL_3;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_1:
			return HAL_H264_LEVEL_31;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_2:
			return HAL_H264_LEVEL_32;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_0:
			return HAL_H264_LEVEL_4;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_1:
			return HAL_H264_LEVEL_41;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_2:
			return HAL_H264_LEVEL_42;
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_0:
			return HAL_H264_LEVEL_5;
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_1:
			return HAL_H264_LEVEL_51;
		default:
			return -EINVAL;
		}
	}

	return -EINVAL;
}

#if 0
static struct v4l2_ctrl *
get_ctrl_from_cluster(int id, struct v4l2_ctrl **cluster, int ncontrols)
{
	int c;

	for (c = 0; c < ncontrols; ++c)
		if (cluster[c]->id == id)
			return cluster[c];

	return NULL;
}

static int is_ctrl_valid_for_codec(struct vidc_inst *inst,
				   struct v4l2_ctrl *ctrl)
{
	struct device *dev = inst->core->dev;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_MVC_BUFFER_LAYOUT:
		if (inst->fmt_out->pixfmt != V4L2_PIX_FMT_H264_MVC) {
			dev_err(dev, "control %x only valid for MVC\n",
				ctrl->id);
			ret = -ENOTSUPP;
			break;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		if (inst->fmt_out->pixfmt == V4L2_PIX_FMT_H264_MVC &&
			ctrl->val != V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH) {
			dev_err(dev, "Profile %x not supported for MVC\n",
				ctrl->val);
			ret = -ENOTSUPP;
			break;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		if (inst->fmt_out->pixfmt == V4L2_PIX_FMT_H264_MVC &&
			ctrl->val >= V4L2_MPEG_VIDEO_H264_LEVEL_5_2) {
			dev_err(dev, "Level %x not supported for MVC\n",
				ctrl->val);
			ret = -ENOTSUPP;
			break;
		}
		break;
	default:
		break;
	}

	return ret;
}

/* Small helper macro for quickly getting a control and err checking */
#define TRY_GET_CTRL(__ctrl_id) ({ \
		struct v4l2_ctrl *__temp; \
		__temp = get_ctrl_from_cluster( \
			__ctrl_id, \
			ctrl->cluster, ctrl->ncontrols); \
		if (!__temp) { \
			/* Clusters are hardcoded, if we can't find */ \
			/* something then things are massively screwed up */ \
			BUG_ON(1); \
		} \
		__temp; \
	})

static int try_set_ctrl(struct vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	struct device *dev = inst->core->dev;
	struct hfi_device *hfi = &inst->core->hfi;
	struct hal_nal_stream_format_supported stream_format;
	struct hal_enable_picture enable_picture;
	struct hal_enable hal_property;
	struct hal_extradata_enable extra;
	struct hal_multi_stream multi_stream;
	struct hfi_scs_threshold scs_threshold;
	struct hal_mvc_buffer_layout layout;
	struct hal_profile_level profile_level;
	struct hal_framesize frame_sz;
	struct v4l2_ctrl *temp_ctrl = NULL;
	enum hal_property prop_id = 0;
	u32 property_val = 0;
	void *pdata = NULL;
	int ret = 0;

	ret = is_ctrl_valid_for_codec(inst, ctrl);
	if (ret)
		return ret;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT:
		prop_id = HAL_PARAM_NAL_STREAM_FORMAT_SELECT;
		stream_format.nal_stream_format_supported = 1 << ctrl->val;
		pdata = &stream_format;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER:
		prop_id = HAL_PARAM_VDEC_OUTPUT_ORDER;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE:
		prop_id = HAL_PARAM_VDEC_PICTURE_TYPE_DECODE;
		enable_picture.picture_type = ctrl->val;
		pdata = &enable_picture;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_KEEP_ASPECT_RATIO:
		prop_id = HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_POST_LOOP_DEBLOCKER_MODE:
		prop_id = HAL_CONFIG_VDEC_POST_LOOP_DEBLOCKER;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_DIVX_FORMAT:
		prop_id = HAL_PARAM_DIVX_FORMAT;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MB_ERROR_MAP_REPORTING:
		prop_id = HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE:
		switch (ctrl->val) {
		case V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE:
			inst->flags &= ~VIDC_THUMBNAIL;
			break;
		case V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_ENABLE:
			inst->flags |= VIDC_THUMBNAIL;
			break;
		}

		prop_id = HAL_PARAM_VDEC_SYNC_FRAME_DECODE;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		prop_id = HAL_PARAM_INDEX_EXTRADATA;
		extra.index = vidc_extradata_index(ctrl->val);
		extra.enable = 1;
		pdata = &extra;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_ASSEMBLY:
		prop_id = HAL_PARAM_VDEC_FRAME_ASSEMBLY;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE:
		if (ctrl->val && !(inst->capability.pixelprocess_caps &
		    HAL_VIDEO_DECODER_MULTI_STREAM_CAPABILITY)) {
			dev_err(dev, "Downscaling not supported: %#x\n",
				ctrl->id);
			ret = -ENOTSUPP;
			break;
		}
		switch (ctrl->val) {
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY:
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT;
			multi_stream.enable = true;
			pdata = &multi_stream;
			prop_id = HAL_PARAM_VDEC_MULTI_STREAM;

			ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst,
							    prop_id, pdata);
			if (ret) {
				dev_err(dev, "enabling OUTPUT port (%d)\n",
					ret);
				break;
			}
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT2;
			multi_stream.enable = false;
			pdata = &multi_stream;

			ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst,
							    prop_id, pdata);
			if (ret)
				dev_err(dev, "disabling OUTPUT2 port (%d)\n",
					ret);
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_SECONDARY:
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT2;
			multi_stream.enable = true;
			pdata = &multi_stream;
			prop_id = HAL_PARAM_VDEC_MULTI_STREAM;

			ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst,
							    prop_id, pdata);
			if (ret) {
				dev_err(dev, "enabling OUTPUT2 port (%d)\n",
					ret);
					break;
			}
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT;
			multi_stream.enable = false;
			pdata = &multi_stream;

			ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst,
							    prop_id, pdata);
			if (ret) {
				dev_err(dev, "disabling OUTPUT port (%d)\n",
					ret);
				break;
			}

			frame_sz.buffer_type = HAL_BUFFER_OUTPUT2;
			frame_sz.width = inst->width;
			frame_sz.height = inst->height;
			pdata = &frame_sz;
			prop_id = HAL_PARAM_FRAME_SIZE;

			dev_dbg(dev, "buftype: %d width: %d, height: %d\n",
				frame_sz.buffer_type, frame_sz.width,
				frame_sz.height);

			ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst,
							    prop_id, pdata);
			if (ret)
				dev_err(dev, "setting OUTPUT2 size (%d)\n",
					ret);
			break;
		default:
			dev_err(dev, "unsupported multi stream setting\n");
			ret = -ENOTSUPP;
			break;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SCS_THRESHOLD:
		prop_id = HAL_PARAM_VDEC_SCS_THRESHOLD;
		scs_threshold.threshold_value = ctrl->val;
		pdata = &scs_threshold;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MVC_BUFFER_LAYOUT:
		prop_id = HAL_PARAM_MVC_BUFFER_LAYOUT;
		layout.layout_type = vidc_buffer_layout(ctrl->val);
		layout.bright_view_first = 0;
		layout.ngap = 0;
		pdata = &layout;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR:
		prop_id = HAL_PARAM_VDEC_CONCEAL_COLOR;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_LEVEL);
		prop_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = vdec_v4l2_to_hal(ctrl->id, ctrl->val);
		profile_level.level =
			vdec_v4l2_to_hal(V4L2_CID_MPEG_VIDEO_H264_LEVEL,
					 temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_PROFILE);
		prop_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = vdec_v4l2_to_hal(ctrl->id, ctrl->val);
		profile_level.profile =
			vdec_v4l2_to_hal(V4L2_CID_MPEG_VIDEO_H264_PROFILE,
					 temp_ctrl->val);
		pdata = &profile_level;
		break;
	default:
		break;
	}

	if (!ret && prop_id) {
		dev_dbg(dev,
			"Control: HAL property=%x, ctrl: id=%x, value=%x\n",
			prop_id, ctrl->id, ctrl->val);

		ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst,
						    prop_id, pdata);
	}

	return ret;
}

static int vdec_op_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct vidc_inst *inst =
		container_of(ctrl->handler, struct vidc_inst, ctrl_handler);
	struct device *dev = inst->core->dev;
	int ret = 0, c = 0;

	for (c = 0; c < ctrl->ncontrols; ++c) {
		if (ctrl->cluster[c]->is_new) {
			ret = try_set_ctrl(inst, ctrl->cluster[c]);
			if (ret)
				break;
		}
	}

	if (ret)
		dev_err(dev, "setting control: %x (%s)",
			ctrl->id, v4l2_ctrl_get_name(ctrl->id));

	return ret;
}
#endif

static int vdec_op_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct vidc_inst *inst = ctrl_to_inst(ctrl);
	struct device *dev = inst->core->dev;
	struct vdec_controls *ctr = &inst->controls.dec;

	dev_dbg(dev, "set: ctrl id %x, value %x\n", ctrl->id, ctrl->val);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT:
		ctr->nal_format = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER:
		ctr->output_order = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_MPEG4_DEBLOCK_FILTER:
		ctr->post_loop_deb_mode = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE:
		ctr->sync_frame_decode = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_ASSEMBLY:
		ctr->frame_assembly = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VPX_PROFILE:
		ctr->profile = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		ctr->level = ctrl->val;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vdec_op_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct vidc_inst *inst = ctrl_to_inst(ctrl);
	struct device *dev = inst->core->dev;
	struct vdec_controls *ctr = &inst->controls.dec;
	struct hfi_device *hfi = &inst->core->hfi;
	union hal_get_property hprop;
	enum hal_property ptype = HAL_PARAM_PROFILE_LEVEL_CURRENT;
	int ret;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VPX_PROFILE:
		ret = vidc_hfi_session_get_property(hfi, inst->hfi_inst, ptype,
						    &hprop);
		if (!ret)
			ctr->profile = hprop.profile_level.profile;
		ctrl->val = ctr->profile;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		ret = vidc_hfi_session_get_property(hfi, inst->hfi_inst, ptype,
						    &hprop);
		if (!ret)
			ctr->level = hprop.profile_level.level;
		ctrl->val = ctr->level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT:
		ctrl->val = ctr->nal_format;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER:
		ctrl->val = ctr->output_order;
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_MPEG4_DEBLOCK_FILTER:
		ctrl->val = ctr->post_loop_deb_mode;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE:
		ctrl->val = ctr->sync_frame_decode;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_ASSEMBLY:
		ctrl->val = ctr->frame_assembly;
		break;
	default:
		return -EINVAL;
	};

	dev_dbg(dev, "get: ctrl id %x, value %x\n", ctrl->id, ctrl->val);

	return 0;
}

static const struct v4l2_ctrl_ops vdec_ctrl_ops = {
	.s_ctrl = vdec_op_s_ctrl,
	.g_volatile_ctrl = vdec_op_g_volatile_ctrl,
};

int vdec_ctrl_init(struct vidc_inst *inst)
{
	struct device *dev = inst->core->dev;
	struct v4l2_ctrl_config cfg;
	unsigned int i;
	int ret;

	inst->ctrls = kzalloc(sizeof(struct v4l2_ctrl *) * NUM_CTRLS,
			      GFP_KERNEL);
	if (!inst->ctrls)
		return -ENOMEM;

	ret = v4l2_ctrl_handler_init(&inst->ctrl_handler, NUM_CTRLS);
	if (ret) {
		dev_err(dev, "control handler init (%d)\n", ret);
		return ret;
	}

	for (i = 0; i < NUM_CTRLS; i++) {
		struct v4l2_ctrl *ctrl = NULL;

		if (IS_PRIV_CTRL(vdec_ctrls[i].id)) {
			memset(&cfg, 0, sizeof(cfg));
			cfg.def = vdec_ctrls[i].def;
			cfg.flags = 0;
			cfg.id = vdec_ctrls[i].id;
			cfg.max = vdec_ctrls[i].max;
			cfg.min = vdec_ctrls[i].min;
			cfg.menu_skip_mask = vdec_ctrls[i].menu_skip_mask;
			cfg.name = vdec_ctrls[i].name;
			cfg.ops = &vdec_ctrl_ops;
			cfg.step = vdec_ctrls[i].step;
			cfg.type = vdec_ctrls[i].type;
			cfg.qmenu = vdec_ctrls[i].qmenu;

			ctrl = v4l2_ctrl_new_custom(&inst->ctrl_handler, &cfg,
						    NULL);
		} else {
			if (vdec_ctrls[i].type == V4L2_CTRL_TYPE_MENU) {
				ctrl = v4l2_ctrl_new_std_menu(
					&inst->ctrl_handler,
					&vdec_ctrl_ops,
					vdec_ctrls[i].id,
					vdec_ctrls[i].max,
					vdec_ctrls[i].menu_skip_mask,
					vdec_ctrls[i].def);
			} else {
				ctrl = v4l2_ctrl_new_std(&inst->ctrl_handler,
					&vdec_ctrl_ops,
					vdec_ctrls[i].id,
					vdec_ctrls[i].min,
					vdec_ctrls[i].max,
					vdec_ctrls[i].step,
					vdec_ctrls[i].def);
			}
		}

		if (!ctrl) {
			dev_err(dev, "invalid ctrl (id:%x, name:%s)\n",
				vdec_ctrls[i].id, vdec_ctrls[i].name);
			return -EINVAL;
		}

		switch (vdec_ctrls[i].id) {
		case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
		case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		case V4L2_CID_MPEG_VIDEO_VPX_PROFILE:
			ctrl->flags |= vdec_ctrls[i].flags;
			break;
		}

		ret = inst->ctrl_handler.error;
		if (ret) {
			v4l2_ctrl_handler_free(&inst->ctrl_handler);
			dev_err(dev, "adding ctrl (%s) to ctrl handle (%d)\n",
				vdec_ctrls[i].name, ret);
			return ret;
		}

		inst->ctrls[i] = ctrl;
	}

	return ret;
}

void vdec_ctrl_deinit(struct vidc_inst *inst)
{
	kfree(inst->ctrls);
	v4l2_ctrl_handler_free(&inst->ctrl_handler);
}
