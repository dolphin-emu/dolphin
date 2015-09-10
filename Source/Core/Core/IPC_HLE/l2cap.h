// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

/*      $NetBSD: l2cap.h,v 1.9 2009/09/13 18:45:11 pooka Exp $  */

/*-
* Copyright (c) 2005 Iain Hibbert.
* Copyright (c) 2006 Itronix Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. The name of Itronix Inc. may not be used to endorse
*    or promote products derived from this software without specific
*    prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
/*-
* Copyright (c) Maksim Yevmenkin <m_evmenkin@yahoo.com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*
* $Id: l2cap.h,v 1.9 2009/09/13 18:45:11 pooka Exp $
* $FreeBSD: src/sys/netgraph/bluetooth/include/l2cap.h,v 1.4 2005/08/31 18:13:23 emax Exp $
*/

/*
* This file contains everything that application needs to know about
* Link Layer Control and Adaptation Protocol (L2CAP). All information
* was obtained from Bluetooth Specification Books (v1.1 and up)
*
* This file can be included by both kernel and userland applications.
*/

#pragma once

#include <stdint.h>

/**************************************************************************
**************************************************************************
**                   Common defines and types (L2CAP)
**************************************************************************
**************************************************************************/

/*
* Channel IDs are assigned per machine. So the total number of channels that
* a machine can have open at the same time is 0xffff - 0x0040 = 0xffbf (65471).
* This number does not depend on number of HCI connections.
*/

#define L2CAP_NULL_CID                  0x0000  /* DO NOT USE THIS CID */
#define L2CAP_SIGNAL_CID                0x0001  /* signaling channel ID */
#define L2CAP_CLT_CID                   0x0002  /* connectionless channel ID */
/* 0x0003 - 0x003f Reserved */
#define L2CAP_FIRST_CID                 0x0040  /* dynamically alloc. (start) */
#define L2CAP_LAST_CID                  0xffff  /* dynamically alloc. (end) */

/* L2CAP MTU */
#define L2CAP_MTU_MINIMUM               48
#define L2CAP_MTU_DEFAULT               672
#define L2CAP_MTU_MAXIMUM               0xffff

/* L2CAP flush and link timeouts */
#define L2CAP_FLUSH_TIMO_DEFAULT        0xffff /* always retransmit */
#define L2CAP_LINK_TIMO_DEFAULT         0xffff

/* L2CAP Command Reject reasons */
#define L2CAP_REJ_NOT_UNDERSTOOD        0x0000
#define L2CAP_REJ_MTU_EXCEEDED          0x0001
#define L2CAP_REJ_INVALID_CID           0x0002
/* 0x0003 - 0xffff - reserved for future use */

/* Protocol/Service Multiplexor (PSM) values */
#define L2CAP_PSM_ANY                   0x0000  /* Any/Invalid PSM */
#define L2CAP_PSM_SDP                   0x0001  /* Service Discovery Protocol */
#define L2CAP_PSM_RFCOMM                0x0003  /* RFCOMM protocol */
#define L2CAP_PSM_TCP                   0x0005  /* Telephony Control Protocol */
#define L2CAP_PSM_TCS                   0x0007  /* TCS cordless */
#define L2CAP_PSM_BNEP                  0x000f  /* Bluetooth Network */
/*      Encapsulation Protocol*/
#define L2CAP_PSM_HID_CNTL              0x0011  /* HID Control */
#define L2CAP_PSM_HID_INTR              0x0013  /* HID Interrupt */
#define L2CAP_PSM_ESDP                  0x0015  /* Extended Service */
/*      Discovery Profile */
#define L2CAP_PSM_AVCTP                 0x0017  /* Audio/Visual Control */
/*      Transport Protocol */
#define L2CAP_PSM_AVDTP                 0x0019  /* Audio/Visual Distribution */
/*      Transport Protocol */
/* 0x0019 - 0x1000 - reserved for future use */

#define L2CAP_PSM_INVALID(psm)          (((psm) & 0x0101) != 0x0001)

/* L2CAP Connection response command result codes */
#define L2CAP_SUCCESS                   0x0000
#define L2CAP_PENDING                   0x0001
#define L2CAP_PSM_NOT_SUPPORTED         0x0002
#define L2CAP_SECURITY_BLOCK            0x0003
#define L2CAP_NO_RESOURCES              0x0004
#define L2CAP_TIMEOUT                   0xeeee
#define L2CAP_UNKNOWN                   0xffff
/* 0x0005 - 0xffff - reserved for future use */

/* L2CAP Connection response status codes */
#define L2CAP_NO_INFO                   0x0000
#define L2CAP_AUTH_PENDING              0x0001
#define L2CAP_AUTZ_PENDING              0x0002
/* 0x0003 - 0xffff - reserved for future use */

/* L2CAP Configuration response result codes */
#define L2CAP_UNACCEPTABLE_PARAMS       0x0001
#define L2CAP_REJECT                    0x0002
#define L2CAP_UNKNOWN_OPTION            0x0003
/* 0x0003 - 0xffff - reserved for future use */

/* L2CAP Configuration options */
#define L2CAP_OPT_CFLAG_BIT             0x0001
#define L2CAP_OPT_CFLAG(flags)          ((flags) & L2CAP_OPT_CFLAG_BIT)
#define L2CAP_OPT_HINT_BIT              0x80
#define L2CAP_OPT_HINT(type)            ((type) & L2CAP_OPT_HINT_BIT)
#define L2CAP_OPT_HINT_MASK             0x7f
#define L2CAP_OPT_MTU                   0x01
#define L2CAP_OPT_MTU_SIZE              sizeof(uint16_t)
#define L2CAP_OPT_FLUSH_TIMO            0x02
#define L2CAP_OPT_FLUSH_TIMO_SIZE       sizeof(uint16_t)
#define L2CAP_OPT_QOS                   0x03
#define L2CAP_OPT_QOS_SIZE              sizeof(l2cap_qos_t)
#define L2CAP_OPT_RFC                   0x04
#define L2CAP_OPT_RFC_SIZE              sizeof(l2cap_rfc_t)
/* 0x05 - 0xff - reserved for future use */

/* L2CAP Information request type codes */
#define L2CAP_CONNLESS_MTU              0x0001
#define L2CAP_EXTENDED_FEATURES         0x0002
/* 0x0003 - 0xffff - reserved for future use */

/* L2CAP Information response codes */
#define L2CAP_NOT_SUPPORTED             0x0001
/* 0x0002 - 0xffff - reserved for future use */

#pragma pack(push, 1)

/* L2CAP Quality of Service option */
typedef struct {
	uint8_t  flags;                 /* reserved for future use */
	uint8_t  service_type;          /* service type */
	uint32_t token_rate;            /* bytes per second */
	uint32_t token_bucket_size;     /* bytes */
	uint32_t peak_bandwidth;        /* bytes per second */
	uint32_t latency;               /* microseconds */
	uint32_t delay_variation;       /* microseconds */
} l2cap_qos_t;

/* L2CAP QoS type */
#define L2CAP_QOS_NO_TRAFFIC    0x00
#define L2CAP_QOS_BEST_EFFORT   0x01       /* (default) */
#define L2CAP_QOS_GUARANTEED    0x02
/* 0x03 - 0xff - reserved for future use */

/* L2CAP Retransmission & Flow Control option */
typedef struct {
	uint8_t mode;              /* RFC mode */
	uint8_t window_size;       /* bytes */
	uint8_t max_transmit;      /* max retransmissions */
	uint16_t        retransmit_timo;   /* milliseconds */
	uint16_t        monitor_timo;      /* milliseconds */
	uint16_t        max_pdu_size;      /* bytes */
} l2cap_rfc_t;

/* L2CAP RFC mode */
#define L2CAP_RFC_BASIC         0x00       /* (default) */
#define L2CAP_RFC_RETRANSMIT    0x01
#define L2CAP_RFC_FLOW          0x02
/* 0x03 - 0xff - reserved for future use */

/**************************************************************************
**************************************************************************
**                 Link level defines, headers and types
**************************************************************************
**************************************************************************/

/* L2CAP header */
typedef struct {
	uint16_t        length; /* payload size */
	uint16_t        dcid;   /* destination channel ID */
} l2cap_hdr_t;

/* L2CAP ConnectionLess Traffic         (dcid == L2CAP_CLT_CID) */
typedef struct {
	uint16_t        psm; /* Protocol/Service Multiplexor */
} l2cap_clt_hdr_t;

#define L2CAP_CLT_MTU_MAXIMUM \
	(L2CAP_MTU_MAXIMUM - sizeof(l2cap_clt_hdr_t))

/* L2CAP Command header                 (dcid == L2CAP_SIGNAL_CID) */
typedef struct {
	uint8_t code;   /* command OpCode */
	uint8_t ident;  /* identifier to match request and response */
	uint16_t        length; /* command parameters length */
} l2cap_cmd_hdr_t;

/* L2CAP Command Reject */
#define L2CAP_COMMAND_REJ                       0x01
typedef struct {
	uint16_t        reason; /* reason to reject command */
	uint16_t        data[2];/* optional data */
} l2cap_cmd_rej_cp;

/* L2CAP Connection Request */
#define L2CAP_CONNECT_REQ                       0x02
typedef struct {
	uint16_t        psm;  /* Protocol/Service Multiplexor (PSM) */
	uint16_t        scid; /* source channel ID */
} l2cap_con_req_cp;

/* L2CAP Connection Response */
#define L2CAP_CONNECT_RSP                       0x03
typedef struct {
	uint16_t        dcid;   /* destination channel ID */
	uint16_t        scid;   /* source channel ID */
	uint16_t        result; /* 0x00 - success */
	uint16_t        status; /* more info if result != 0x00 */
} l2cap_con_rsp_cp;

/* L2CAP Configuration Request */
#define L2CAP_CONFIG_REQ                        0x04
typedef struct {
	uint16_t        dcid;  /* destination channel ID */
	uint16_t        flags; /* flags */
	/*      uint8_t options[] --  options */
} l2cap_cfg_req_cp;

/* L2CAP Configuration Response */
#define L2CAP_CONFIG_RSP                        0x05
typedef struct {
	uint16_t        scid;   /* source channel ID */
	uint16_t        flags;  /* flags */
	uint16_t        result; /* 0x00 - success */
	/*      uint8_t options[] -- options */
} l2cap_cfg_rsp_cp;

/* L2CAP configuration option */
typedef struct {
	uint8_t type;
	uint8_t length;
	/*      uint8_t value[] -- option value (depends on type) */
} l2cap_cfg_opt_t;

/* L2CAP configuration option value */
typedef union {
	uint16_t                mtu;            /* L2CAP_OPT_MTU */
	uint16_t                flush_timo;     /* L2CAP_OPT_FLUSH_TIMO */
	l2cap_qos_t             qos;            /* L2CAP_OPT_QOS */
	l2cap_rfc_t             rfc;            /* L2CAP_OPT_RFC */
} l2cap_cfg_opt_val_t;

/* L2CAP Disconnect Request */
#define L2CAP_DISCONNECT_REQ                    0x06
typedef struct {
	uint16_t        dcid; /* destination channel ID */
	uint16_t        scid; /* source channel ID */
} l2cap_discon_req_cp;

/* L2CAP Disconnect Response */
#define L2CAP_DISCONNECT_RSP                    0x07
typedef l2cap_discon_req_cp     l2cap_discon_rsp_cp;

/* L2CAP Echo Request */
#define L2CAP_ECHO_REQ                          0x08
/* No command parameters, only optional data */

/* L2CAP Echo Response */
#define L2CAP_ECHO_RSP                          0x09
#define L2CAP_MAX_ECHO_SIZE \
	(L2CAP_MTU_MAXIMUM - sizeof(l2cap_cmd_hdr_t))
/* No command parameters, only optional data */

/* L2CAP Information Request */
#define L2CAP_INFO_REQ                          0x0a
typedef struct {
	uint16_t        type; /* requested information type */
} l2cap_info_req_cp;

/* L2CAP Information Response */
#define L2CAP_INFO_RSP                          0x0b
typedef struct {
	uint16_t        type;   /* requested information type */
	uint16_t        result; /* 0x00 - success */
	/*      uint8_t info[]  -- info data (depends on type)
	*
	* L2CAP_CONNLESS_MTU - 2 bytes connectionless MTU
	*/
} l2cap_info_rsp_cp;

typedef union {
	/* L2CAP_CONNLESS_MTU */
	struct {
		uint16_t        mtu;
	} mtu;
} l2cap_info_rsp_data_t;

#pragma pack(pop)

/**************************************************************************
**************************************************************************
**             L2CAP Socket Definitions
**************************************************************************
**************************************************************************/

/* Socket options */
#define SO_L2CAP_IMTU           1       /* incoming MTU */
#define SO_L2CAP_OMTU           2       /* outgoing MTU */
#define SO_L2CAP_IQOS           3       /* incoming QoS */
#define SO_L2CAP_OQOS           4       /* outgoing QoS */
#define SO_L2CAP_FLUSH          5       /* flush timeout */
#define SO_L2CAP_LM             6       /* link mode */

/* L2CAP link mode flags */
#define L2CAP_LM_AUTH           (1<<0)  /* want authentication */
#define L2CAP_LM_ENCRYPT        (1<<1)  /* want encryption */
#define L2CAP_LM_SECURE         (1<<2)  /* want secured link */
