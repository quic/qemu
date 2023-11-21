/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include "macros_auto.h"


#if 0

#define POISON_STATE 128

#define MAX_SYNC 8


#define POISONED 1
#define NEVER_KNOWABLE 2
#define SYNCED 4
#define GATHER_WRITE 8
#define STORE_RELEASE 16
#define LOAD_ACQUIRE 32
#define SCATTER 64
#define GATHER_READ 128
#define SCATTER_OP 256


typedef struct byte_list_t byte_list_t;
typedef struct sync_list_t sync_list_t;

struct byte_list_t {
    paddr_t paddr;
    size2u_t state;
 
     byte_list_t * next;
     byte_list_t * previous;
    
 
    paddr_t sync_paddr;


} ;

struct sync_list_t {
    paddr_t paddr;  
    size2u_t state;

    sync_list_t * next;
    sync_list_t * previous;
};
#endif 

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
#if 0
    byte_list_t * bytes;
    byte_list_t * first_byte;
    byte_list_t * last_byte;
    
    sync_list_t * syncs;
    sync_list_t * first_sync;
    sync_list_t * last_sync;

    size4u_t byte_count; 
    size4u_t sync_count; 

    paddr_t store_release_addr[POISON_STATE];    
    size1u_t store_release_idx;
#endif
	
  	bytes_list_t *bytes[2];
  	bytes_list_t *first_byte[2];
  	bytes_list_t *last_byte[2];
    size4u_t byte_count[2]; 
  	scaga_callback_info_t scaga_info;
} vtcm_state_t;

#if 0
int is_byte_poisoned(vtcm_state_t * vtcm_state, paddr_t paddr);
byte_list_t * add_byte(vtcm_state_t * vtcm_state, paddr_t paddr);   
sync_list_t * add_sync(vtcm_state_t * vtcm_state, paddr_t paddr, int type);


void remove_byte(vtcm_state_t * vtcm_state, byte_list_t * entry);
void remove_sync(vtcm_state_t * vtcm_state, sync_list_t * entry);


byte_list_t * find_byte(vtcm_state_t * vtcm_state, paddr_t paddr);
sync_list_t * find_sync(vtcm_state_t * vtcm_state, paddr_t paddr);



void poison_byte(vtcm_state_t * vtcm_state, paddr_t paddr, paddr_t gather_paddr, int is_op) ;
void poison_byte_never_knowable(vtcm_state_t * vtcm_state, paddr_t paddr) ;


void set_sync_byte(vtcm_state_t * vtcm_state, paddr_t sync_paddr, int type) ;

void clear_byte(vtcm_state_t * vtcm_state, paddr_t paddr) ;
void clear_all_synced_bytes(vtcm_state_t * vtcm_state, paddr_t paddr, int type);

void add_gather_store(vtcm_state_t * vtcm_state, paddr_t paddr, paddr_t final_paddr);
void add_store_release(vtcm_state_t * vtcm_state, paddr_t paddr);
#endif

void vtcm_init_state(vtcm_state_t *vtcm_state);
void vtcm_clear_state(vtcm_state_t *vtcm_state);
bytes_list_t *find_byte(vtcm_state_t *vtcm_state, paddr_t paddr, scaga_list_type_e type);
void enlist_byte(thread_t *thread, paddr_t paddr, scaga_list_type_e type, size1u_t rw);
void delist_byte(thread_t *thread, bytes_list_t *entry, scaga_list_type_e type);
void depoison_bytes_for_this_sync(thread_t *thread, paddr_t sync_paddr);
int check_load_acquire(thread_t *thread, paddr_t paddr);
void update_scaga_callback_info(processor_t *proc, scaga_callback_info_t *scaga, int tnum, vaddr_t pc, paddr_t pa, sg_event_type_e event);
#endif
