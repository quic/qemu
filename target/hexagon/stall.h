/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

/*
 *
 * This file contains stall macros
 *
 *
 *
 */


#ifndef _STALL_H_
#define _STALL_H_

#include "global_types.h"
#include <stdio.h>

struct ProcessorState;

#include <stdio.h>

/* #define CROSS_FETCH_STALL 1 */
#define STALL_TRACING 0

#if STALL_TRACING
#ifdef ARCH_SIDE
#define STALLDEBUG(...) fprintf(stdout,__VA_ARGS__); fflush(stdout);
#define FESTALLDEBUG(...) fprintf(stdout,__VA_ARGS__); fflush(stdout);
#else
#define STALLDEBUG(a,b,c,d) fprintf(stdout,a,b,c,d); fflush(stdout);
#define FESTALLDEBUG(a,b,c,d) fprintf(stdout,a,b,c,d); fflush(stdout);
#endif
#else
#ifdef VERIFICATION
#define STALLDEBUG(...) /* warn("%s:",__FUNCTION__); warn(__VA_ARGS__)*/;
#define FESTALLDEBUG(...)
#else
#ifdef ARCH_SIDE
#define STALLDEBUG(...) thread = thread;
#define FESTALLDEBUG(...)
#else
#define STALLDEBUG(a,b,c,d)
#define FESTALLDEBUG(a,b,c,d)
#endif
#endif
#endif


extern char *stall_names[];
extern char *tstat_names[];
extern char *pstat_names[];
extern char *stall_descrs[];
extern char *tstat_descrs[];
extern char *pstat_descrs[];
extern char *pstat_help[];
extern char *tstat_help[];
extern char *stall_help[];
extern char *histo_names[];
extern char *histo_descrs[];
extern char *histo_help[];
extern size4u_t histo_sizes[];
extern int pmu_bucket_for_staller[];

#define INC_PCYCLES(PROC)
#define INC_TSTAT(STAT)
#define INC_TSTATPC(STAT, PC)
#define DEC_TSTATPC(STAT, PC)
#define DEC_TSTAT(STAT)
#define INC_TSTATN(STAT,N)
#define INC_TSTATNPC(STAT,N,PC)
#define DEC_TSTATN(STAT,N)
#define INC_TSTALL(STAT)
#define INC_TSTALLN(STAT,N)
#define DEC_TSTALLN(STAT,N)
#define INC_HISTO(HISTO,BUCKET)
#define INC_HISTON(HISTO,BUCKET,N)
#define INC_TSTATLOG(STAT)
#define INC_TSTATNLOG(STAT)
#define INC_PSTAT(STAT)
#define INC_PSTATN(STAT,N)
#define INC_PSTATPC(STAT, PC)
#define INC_PSTATNPC(STAT,N, PC)


//#include "stat_auto.h"

typedef enum stall_ids stall_signal_t;

enum stall_class {
	STALL_CLASS_ND = 0,
	STALL_CLASS_IU = 1
};

#define NO_STALL NUM_STALL_TYPES

#define BUS_STALL_CHECK {                       \
		bus->staller(bus);                      \
	}

#define L1S_STALL_CHECK {                       \
		l1s->staller(l1s);                      \
	}

#define L1S_STALL_SET(STALLTYPE) {								\
		STALLDEBUG("Set stall: tnum=%d cyc=%lld %s\n",		\
				   thread->threadId,						\
				   thread->processor_ptr->monotonic_pcycles,	\
				   stall_names[STALLTYPE]);					\
		l1s->status |= EXEC_STATUS_REPLAY;				\
		l1s->staller = staller_##STALLTYPE;				\
		l1s->stall_type = STALLTYPE;}


#ifdef VERIFICATION
#define STALL_CHECK {													\
        CALLBACK(proc->options->stall_callback,proc->system_ptr,        \
                 proc,thread->threadId,arch_pmu_get_tstall_to_pmu_mapping(thread->processor_ptr, thread->stall_type),&thread->stall_info[0]); \
		thread->staller(thread);										\
	}

#else
#define STALL_CHECK {													\
		STALLDEBUG("Staller: tnum=%d cyc=%lld %s\n",					\
				   thread->threadId,									\
				   thread->processor_ptr->monotonic_pcycles,            \
				   stall_names[thread->stall_type]);					\
        CALLBACK(proc->options->stall_callback,proc->system_ptr,        \
                 proc,thread->threadId,arch_pmu_get_tstall_to_pmu_mapping(thread->processor_ptr, thread->stall_type),&thread->stall_info[0]); \
		thread->staller(thread);										\
	}
#endif

/* #ifdef COMPILE_DEBUG  */
/* #define UARCH_FUNCTION(func_name) func_name##_##debug  */
/* #else  */
/* #define UARCH_FUNCTION(func_name) func_name  */
/* #endif  */

#define STALL_SET(STALLTYPE) {                                  \
		STALLDEBUG("Set stall: tnum=%d cyc=%lld %s\n",          \
				   thread->threadId,                            \
				   thread->processor_ptr->monotonic_pcycles,	\
				   stall_names[STALLTYPE]);                     \
		thread->status |= EXEC_STATUS_REPLAY;                   \
		thread->staller = staller_##STALLTYPE;                  \
		thread->stall_type = STALLTYPE;                         \
    }

#define STALL_SET_TIMED(STALLTYPE, TIME) {                  \
		STALLDEBUG("Set stall: tnum=%d cyc=%lld %s\n",		\
				   thread->threadId,						\
				   thread->processor_ptr->monotonic_pcycles,	\
				   stall_names[STALLTYPE]);					\
		thread->status |= EXEC_STATUS_REPLAY;				\
		thread->staller = staller_##STALLTYPE;				\
        thread->stall_time = TIME;                          \
		thread->stall_type = STALLTYPE;}

#define STALL_SET_F(STALLTYPE, STALLFUNCTION) {				\
		STALLDEBUG("Set stall: tnum=%d cyc=%lld %s\n",		\
				   thread->threadId,						\
				   thread->processor_ptr->monotonic_pcycles,	\
				   stall_names[STALLTYPE]);					\
		thread->status |= EXEC_STATUS_REPLAY;				\
		thread->staller = STALLFUNCTION;					\
		thread->stall_type = STALLTYPE;}

#define STALL_SET_TIMED_F(STALLTYPE, STALLFUNCTION, TIME) { \
		STALLDEBUG("Set stall: tnum=%d cyc=%lld %s\n",		\
				   thread->threadId,						\
				   thread->processor_ptr->monotonic_pcycles,	\
				   stall_names[STALLTYPE]);					\
		thread->status |= EXEC_STATUS_REPLAY;				\
		thread->staller = STALLFUNCTION;					\
        thread->stall_time = TIME;                          \
		thread->stall_type = STALLTYPE;}

#define BUS_STALL_SET_F(STALLTYPE, STALLFUNCTION) {       \
		bus->status |= EXEC_STATUS_REPLAY;                \
		bus->staller = STALLFUNCTION;                     \
		bus->stall_type = STALLTYPE;}

#define CU_LOCKED(STALLTYPE)                    \
    ((STALLTYPE == cu_locked) || (STALLTYPE == k0locked))

void close_cachedebug(struct ProcessorState *proc);

char *stall_get_tstat_name(struct ProcessorState *proc, int tnum,
						   int tstat_id, char *dst);
char *stall_get_tstat_descr(struct ProcessorState *proc, int tnum,
							int tstat_id, char *dst);
char *stall_get_pstat_name(struct ProcessorState *proc, int tnum,
						   int pstat_id, char *dst);
char *stall_get_pstat_descr(struct ProcessorState *proc, int tnum,
							int pstat_id, char *dst);
char *stall_get_stall_name(struct ProcessorState *proc, int tnum,
						   int pstat_id, char *dst);
char *stall_get_stall_descr(struct ProcessorState *proc, int tnum,
							int pstat_id, char *dst);

void stall_dump_raw(struct ProcessorState *proc, FILE * file);

void stall_close_all_files(struct ProcessorState *proc);


#endif							/* #ifndef _STALL_H_ */
