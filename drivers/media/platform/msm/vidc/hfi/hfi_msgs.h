/*
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
#ifndef __HFI_MSGS_H__
#define __HFI_MSGS_H__

struct hfi_device;
struct hfi_pkt_hdr;

void hfi_process_watchdog_timeout(struct hfi_device *hfi);
u32 hfi_process_msg_packet(struct hfi_device *hfi, struct hfi_pkt_hdr *hdr);

#endif
