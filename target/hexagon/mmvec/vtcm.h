/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _VTCM_H
#define _VTCM_H


#include "../max.h"
#include "../external_api.h"

typedef enum {
  POISON,
  SYNCED,
  SCAGA_LIST_TYPE_COUNT
} scaga_list_type_e;

typedef struct bytes_list_t bytes_list_t;
struct bytes_list_t {
  paddr_t paddr;
  size1u_t rw; // 0=sync, 1=R, 2=W, 3=RW
  int ct;
  bytes_list_t *next;
  bytes_list_t *prev;
};

typedef struct {
  	bytes_list_t *bytes[2];
  	bytes_list_t *first_byte[2];
  	bytes_list_t *last_byte[2];
    size4u_t byte_count[2];
  	scaga_callback_info_t scaga_info;
} vtcm_state_t;

#endif
