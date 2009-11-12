/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org

    QNX Graphics Framework SDL driver
    Copyright (C) 2009 Mike Gorchak
    (mike@malva.ua, lestat@i.com.ua)
*/

#ifndef __SDL_GF_INPUT_H__
#define __SDL_GF_INPUT_H__

#include "SDL_config.h"
#include "SDL_video.h"
#include "../SDL_sysvideo.h"

#include <errno.h>

#include <gf/gf.h>

#include <sys/hiddi.h>
#include <sys/hidut.h>

#include "SDL_qnxgf.h"

typedef struct SDL_MouseData
{
    SDL_DisplayData *didata;
} SDL_MouseData;

int32_t gf_addinputdevices(_THIS);
int32_t gf_delinputdevices(_THIS);

#define SDL_GF_MOUSE_COLOR_BLACK 0xFF000000
#define SDL_GF_MOUSE_COLOR_WHITE 0xFFFFFFFF
#define SDL_GF_MOUSE_COLOR_TRANS 0x00000000

/*****************************************************************************/
/* This is HIDDI closed interface declarations                               */
/*****************************************************************************/
#define HID_TYPE_MAIN      0x0
#define HID_TYPE_GLOBAL    0x1
#define HID_TYPE_LOCAL     0x2
#define HID_TYPE_RESERVED  0x3

#define HID_GLOBAL_USAGE_PAGE 0x0
#define HID_LOCAL_USAGE       0x0

typedef struct _hid_byte
{
    uint8_t HIDB_Length;
    uint8_t HIDB_Type;
    uint8_t HIDB_Tag;
    uint8_t reserved[1];
} hid_byte_t;

typedef struct _hidd_global_item
{
    uint16_t usage_page;
    uint16_t logical_min;
    uint16_t logical_max;
    uint16_t physical_min;
    uint16_t physical_max;
    uint16_t unit_expo;
    uint16_t unit;
    uint16_t report_size;
    uint16_t report_id;
    uint16_t report_count;
} hidd_global_item_t;

typedef struct _hidd_local_item
{
    uint16_t type;
    uint8_t reserved[2];
    uint32_t value;
    struct _hidd_local_item *next_local;
    struct _hidd_local_item *alt_local;
} hidd_local_item_t;

typedef struct _hidd_local_table
{
    hidd_local_item_t *usage_info;
    hidd_local_item_t *designator_info;
    hidd_local_item_t *string_info;
    uint8_t delimiter;
    uint8_t reserved[3];
} hidd_local_table_t;

typedef struct _hidd_field
{
    struct hidd_report_instance *report;
    struct hidd_collection *collection;
    uint16_t report_offset;
    uint16_t flags;
    hidd_global_item_t gitem;
    hidd_local_table_t *ltable;
    struct _hidd_field *next_field;
    void *user;
} hidd_field_t;

typedef struct hidd_report_instance
{
    uint8_t report_id;
    uint8_t reserved[1];
    uint16_t report_type;
    hidd_field_t *field;
    uint16_t num_field;
    uint16_t byte_len;
    uint16_t bit_len;
    uint8_t reserved2[2];
    struct hidd_collection *collection;
    struct hidd_report_instance *next_col_report;
    struct hidd_report_instance *next_report;
} hidd_report_instance_t;

typedef struct hidd_report
{
    TAILQ_ENTRY(hidd_report) link;
    hidd_report_instance_t *rinst;
    hidd_device_instance_t *dev_inst;
    uint32_t flags;
    struct hidd_connection *connection;
    void *user;
} hidd_report_t;

typedef struct hidview_device
{
    struct hidd_report_instance *instance;
    struct hidd_report *report;
} hidview_device_t;

/*****************************************************************************/
/* Closed HIDDI interface declarations end                                   */
/*****************************************************************************/

/* Maximum devices and subdevices amount per host */
#define SDL_HIDDI_MAX_DEVICES 64

/* Detected device/subdevice type for SDL */
#define SDL_GF_HIDDI_NONE     0x00000000
#define SDL_GF_HIDDI_MOUSE    0x00000001
#define SDL_GF_HIDDI_KEYBOARD 0x00000002
#define SDL_GF_HIDDI_JOYSTICK 0x00000003

extern void hiddi_enable_mouse();
extern void hiddi_disable_mouse();

#endif /* __SDL_GF_INPUT_H__ */
