#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <q6standalone.h>


#define MAX_THREADS 4

#define asm_ipendad() \
     __asm__ __volatile__ ( \
         "%0 = ipendad" \
         : "=r" (ipendad) \
    );


#define asm_ssr() \
     __asm__ __volatile__ ( \
         "%0 = ssr" \
         : "=r" (ssr) \
    );
unsigned read_ssr()

{
  unsigned ssr;

  asm_ssr();
  return ssr;
}

#define asm_modectl() \
     __asm__ __volatile__ ( \
         "%0 = MODECTL" \
         : "=r" (modectl) \
    );
unsigned read_modectl()

{
  unsigned modectl;

  asm_modectl();
  return modectl;
}

#define asm_wait()    \
    __asm__ __volatile__ (  \
  "    wait(r0)\n"  \
  : : : "r0"    \
    )

void do_wait()

{
  asm_wait();
}

#define asm_resume()    \
    __asm__ __volatile__ (  \
    "    r0 = %0\n"   \
    "    resume(r0)\n"    \
    : : "r" (mask) : "r0"      \
    );
void do_resume(unsigned mask)

{
  asm_resume();
}

#define asm_swi()    \
    __asm__ __volatile__ (  \
    "    r0 = %0\n"   \
    "    swi(r0)\n"    \
    : : "r" (mask) : "r0"      \
    );

void send_swi(unsigned mask)

{
  asm_swi();
}

/*
	this function assigns an interrupt to multiple threads
*/
void iassignw(unsigned irq_to_thread)
{
	__asm__ __volatile__ (
		"r0 = %0\n"
		"isync\n"
		"iassignw(r0)\n"
		"isync\n"
		:
		: "r" (irq_to_thread)
		: "r0"
	);

}
/*
	this function returns an interrupt assigned to a thread
*/
unsigned iassignr(unsigned thread)
{
	unsigned int interrupt;
	__asm__ __volatile__ (
		"r1 = %1\n"
		"isync\n"
		"r0 = iassignr(r1)\n"
		"isync\n"
		"%0 = r0\n"
		: "=r" (interrupt)
		: "r" (thread)
		: "r0"
	);
	return interrupt;
}
/*
    This function allows one thread to initialize its imask reg for irqs.
*/
void imaskw(unsigned int imask_irq)
{
	unsigned int thread=0xff;
	__asm__ __volatile__ (
		"r0 = %0\n"
		"p0 = %1\n"
		"imask = r0\n"
		"isync\n"
		:
		: "r" (imask_irq), "r" (thread)
		: "r0", "r1"
	);
}

/*
    This function allows thread to read its imask reg.
*/
unsigned int getimask(unsigned int thread)
{
	unsigned int imask;
	__asm__ __volatile__ (
		"isync\n"
		"r0 = getimask(%1)\n"
		"isync\n"
		"%0 = r0\n"
		: "=r" (imask)
		: "r" (thread)
		: "r0"
	);
	return imask;
}

// enables ints for mulitple threads
void setimask(unsigned int thread, unsigned int imask_irq)
{
  unsigned thread_mask = 0x1 << thread;
	__asm__ __volatile__ (
		"r0 = %0\n"
		"p0 = %1\n"
		"setimask(p0,r0)\n"
		"isync\n"
		:
		: "r" (imask_irq), "r" (thread_mask)
		: "r0", "r1"
	);
}

typedef void (*ThreadT)(void *);
long long t_stack[MAX_THREADS][1024];
volatile int intcnt[8];

void int_hdl(int intno)

{
  intcnt[intno]++;
}

void my_thread_function(void *y)

{
  // accept only our interrupt
  int tid = *(int *)y;
  unsigned read_mask;
  unsigned ssr = read_ssr();
  printf("app:%s: tid %d, ssr 0x%x, imask 0x%x\n",
    __FUNCTION__, tid, ssr, getimask(tid));

  register_interrupt(tid, int_hdl);
  printf("app:%s: before wait: tid %d: thread init complete, modectl = 0x%x\n",
    __FUNCTION__, tid, read_modectl());

  // let creating thread know we are done with initialization
  do_wait();

  // thread 0 sends to thread 1, thread 1 sends to thread 2, etc.
  static unsigned int send_int_mask[4] = { 0x0, 0x4, 0x2, 0x0 };
  printf("app:%s: tid %u: sending swi with mask 0x%x, modectl = 0x%x\n",
    __FUNCTION__, tid, send_int_mask[tid], read_modectl());
  send_swi(send_int_mask[tid]);

  while(intcnt[tid] == 0)
    ;
  printf("app:%s: intcnt[tid=%d] %d, imask 1:0x%x, imask 2:0x%x\n",
    __FUNCTION__, tid, intcnt[tid], getimask(1), getimask(2));
}

void wait_for_threads()

{
  unsigned t1mode, t2mode;
  do {
    unsigned modectl = read_modectl();
    t1mode = modectl & (0x1 << (16+1));  // thread 1 wait bit
    t2mode = modectl & (0x1 << (16+2));  // thread 2 wait bit
  } while (t1mode == 0 || t2mode == 0);
}

int main()

{
  unsigned join_mask = 0;
  int tid = 0;
  int id[MAX_THREADS] = { 0, 1, 2, 3 };

  unsigned ssr = read_ssr();
  printf("app:%s: tid %d, ssr 0x%x, imask 0x%x\n",
    __FUNCTION__, tid, ssr, getimask(tid));
  //imaskw(0xFF);   // dont accept any interrupts

  // kick off the two threads and let them do their init
  unsigned first_modectl = read_modectl();
  tid = 1;
  join_mask |= 0x1 << tid;
  thread_create(my_thread_function, &t_stack[tid][1023], tid, (void *)&id[tid]);
  tid = 2;
  join_mask |= 0x1 << tid;
  thread_create(my_thread_function, &t_stack[tid][1023], tid, (void *)&id[tid]);

  // wait for both threads to finish their init and then restart them
  wait_for_threads();

  tid = 0;
  setimask(tid, 0xFF);
  tid = 1;
  setimask(tid, (~(0x1 << tid)) & 0xFF);
  tid = 2;
  setimask(tid, (~(0x1 << tid)) & 0xFF);
  printf("app:%s: after wait: imask 0:0x%x, imask 1:0x%x, imask 2:0x%x, imask 3:0x%x\n",
    __FUNCTION__, getimask(0), getimask(1), getimask(2), getimask(3));

  printf("app:%s: threads done with init: join mask 0x%x, first 0x%x, modectl 0x%x\n",
    __FUNCTION__, join_mask, first_modectl, read_modectl());
  do_resume(join_mask);

  // wait for both to finish
  //thread_join(join_mask);
  thread_join(1<<1);
  thread_join(1<<2);

  printf("app:%s: printing intcnt array: modectl 0x%x\n",
    __FUNCTION__, read_modectl());
  printf("all: ");
  for (int i = 0 ; i < sizeof(intcnt) / sizeof(*intcnt) ; ++i) {
    if ((i % 4) == 0)
      printf("\napp: ");
    printf("intcnt[%2u] = %u ", i, intcnt[i]);
  }

  printf("\napp:%s: PASS\n", __FUNCTION__);
  return 0;
}

