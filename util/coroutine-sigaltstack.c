/*
 * sigaltstack coroutine initialization code
 *
 * Copyright (C) 2006  Anthony Liguori <anthony@codemonkey.ws>
 * Copyright (C) 2011  Kevin Wolf <kwolf@redhat.com>
 * Copyright (C) 2012  Alex Barcelo <abarcelo@ac.upc.edu>
** This file is partly based on pth_mctx.c, from the GNU Portable Threads
**  Copyright (c) 1999-2006 Ralf S. Engelschall <rse@engelschall.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/* XXX Is there a nicer way to disable glibc's stack check for longjmp? */
#undef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 0

#include "qemu/osdep.h"
#include <pthread.h>
#include "qemu/coroutine_int.h"

#ifdef CONFIG_SAFESTACK
#error "SafeStack is not compatible with code run in alternate signal stacks"
#endif

typedef struct {
    Coroutine base;
    void *stack;
    size_t stack_size;
    sigjmp_buf env;
} CoroutineSigAltStack;

/**
 * Per-thread coroutine bookkeeping
 */
typedef struct {
    /** Currently executing coroutine */
    Coroutine *current;

    /** The default coroutine */
    CoroutineSigAltStack leader;

    /** Information for the signal handler (trampoline) */
    sigjmp_buf tr_reenter;
    CoroutineSigAltStack *tr_handler;
    pthread_mutex_t mutex;
} CoroutineThreadState;

static pthread_key_t thread_state_key;

static void coroutine_trampoline(int signal);

static CoroutineThreadState *coroutine_get_thread_state(void)
{
    CoroutineThreadState *s = pthread_getspecific(thread_state_key);

    if (!s) {
        s = g_malloc0(sizeof(*s));
        s->current = &s->leader.base;
        pthread_setspecific(thread_state_key, s);
    }
    return s;
}

static void qemu_coroutine_thread_cleanup(void *opaque)
{
    CoroutineThreadState *s = opaque;

    g_free(s);
}

static void QEMU_CONSTRUCTOR(coroutine_init)(void)
{
    int ret;
    sigset_t sigs;
    struct sigaction sa;

    ret = pthread_key_create(&thread_state_key, qemu_coroutine_thread_cleanup);
    if (ret != 0) {
        fprintf(stderr, "unable to create leader key: %s\n", strerror(errno));
        abort();
    }

    /*
     * This constructor function is running in the main thread. Masking SIGUSR2
     * here means that both the main thread, and directly or indirectly
     * descendant threads thereof, will block SIGUSR2. (The signal mask is
     * thread-specific, and inherited through pthread_create().)
     */
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &sigs, NULL);

    /*
     * Establish the SIGUSR2 signal handler. This is a process-wide operation,
     * and so will apply to all threads from here on.
     *
     * We'll only unblock the delivery of SIGUSR2 in a narrow window, in
     * qemu_coroutine_new().
     *
     * While the handler is running, SIGUSR2 itself will be blocked due to
     * setting none of the SA_NODEFER and SA_RESETHAND flags below. All other
     * signals will *remain* blocked, from where we unblock SIGUSR2 in
     * qemu_coroutine_new(). Still, we need to set "sa_mask" below *somehow*,
     * and then it's simplest to make it block all signals, even though it
     * makes no difference to the signal mask that's already going to be in
     * effect when the handler is entered.
     */
    sa.sa_handler = coroutine_trampoline;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = SA_ONSTACK;
    if (sigaction(SIGUSR2, &sa, NULL) != 0) {
        perror("Unable to install SIGUSR2 handler");
        abort();
    }

    CoroutineThreadState *coTS = coroutine_get_thread_state();
    pthread_mutex_init(&coTS->mutex, NULL);
}

/* "boot" function
 * This is what starts the coroutine, is called from the trampoline
 * (from the signal handler when it is not signal handling, read ahead
 * for more information).
 */
static void coroutine_bootstrap(CoroutineSigAltStack *self, Coroutine *co)
{
    /* Initialize longjmp environment and switch back the caller */
    if (!sigsetjmp(self->env, 0)) {
        siglongjmp(*(sigjmp_buf *)co->entry_arg, 1);
    }

    while (true) {
        co->entry(co->entry_arg);
        qemu_coroutine_switch(co, co->caller, COROUTINE_TERMINATE);
    }
}

/*
 * This is used as the signal handler. This is called with the brand new stack
 * (thanks to sigaltstack). We have to return, given that this is a signal
 * handler and the sigmask and some other things are changed.
 */
static void coroutine_trampoline(int signal)
{
    CoroutineSigAltStack *self;
    Coroutine *co;
    CoroutineThreadState *coTS;

    /* Get the thread specific information */
    coTS = coroutine_get_thread_state();
    self = coTS->tr_handler;

    if (!coTS || !self) {
        /*
         * Never reached -- the top of coroutine_trampoline() can only be
         * entered from the sigsuspend() call in qemu_coroutine_new().
         */
        abort();
    }

    coTS->tr_handler = NULL;
    co = &self->base;

    /*
     * Here we have to do a bit of a ping pong between the caller, given that
     * this is a signal handler and we have to do a return "soon". Then the
     * caller can reestablish everything and do a siglongjmp here again.
     */
    if (!sigsetjmp(coTS->tr_reenter, 0)) {
        return;
    }

    /*
     * Ok, the caller has siglongjmp'ed back to us, so now prepare
     * us for the real machine state switching. We have to jump
     * into another function here to get a new stack context for
     * the auto variables (which have to be auto-variables
     * because the start of the thread happens later). Else with
     * PIC (i.e. Position Independent Code which is used when PTH
     * is built as a shared library) most platforms would
     * horrible core dump as experience showed.
     */
    coroutine_bootstrap(self, co);
}

Coroutine *qemu_coroutine_new(void)
{
    CoroutineSigAltStack *co;
    CoroutineThreadState *coTS;
    stack_t ss;
    stack_t oss;
    sigset_t sigs, osigs;
    sigjmp_buf old_env;

    /* The way to manipulate stack is with the sigaltstack function. We
     * prepare a stack, with it delivering a signal to ourselves and then
     * put sigsetjmp/siglongjmp where needed.
     * This has been done keeping coroutine-ucontext as a model and with the
     * pth ideas (GNU Portable Threads). See coroutine-ucontext for the basics
     * of the coroutines and see pth_mctx.c (from the pth project) for the
     * sigaltstack way of manipulating stacks.
     */

    co = g_malloc0(sizeof(*co));
    co->stack_size = COROUTINE_STACK_SIZE;
    co->stack = qemu_alloc_stack(&co->stack_size);
    co->base.entry_arg = &old_env; /* stash away our jmp_buf */

    /*
     * Set the new stack.
     */
    ss.ss_sp = co->stack;
    ss.ss_size = co->stack_size;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, &oss) < 0) {
        abort();
    }

    /*
     * Now transfer control onto the signal stack and set it up.
     * It will return immediately via "return" after the sigsetjmp()
     * was performed.
     */

    sigfillset(&sigs);
    pthread_sigmask(SIG_BLOCK, &sigs, &osigs);

    coTS = coroutine_get_thread_state();
    pthread_mutex_lock(&coTS->mutex);
    assert(!coTS->tr_handler);
    coTS->tr_handler = co;

    pthread_kill(pthread_self(), SIGUSR2);

    sigfillset(&sigs);
    sigdelset(&sigs, SIGUSR2);


    do {
        sigsuspend(&sigs);
        if (coTS->tr_handler) {
            /*
             * Sometimes sigsuspend could exit for other signals
             * reset and try again
             */
            pthread_sigmask(SIG_BLOCK, &sigs, &osigs);
            pthread_kill(pthread_self(), SIGUSR2);
            sigfillset(&sigs);
            sigdelset(&sigs, SIGUSR2);
        }
    } while (coTS->tr_handler);

    assert(!coTS->tr_handler);

    pthread_sigmask(SIG_SETMASK, &osigs, NULL);

    /*
     * Inform the system that we are back off the signal stack by
     * removing the alternative signal stack. Be careful here: It
     * first has to be disabled, before it can be removed.
     */
    sigaltstack(NULL, &ss);
    ss.ss_flags = SS_DISABLE;
    if (sigaltstack(&ss, NULL) < 0) {
        abort();
    }
    sigaltstack(NULL, &ss);
    if (!(oss.ss_flags & SS_DISABLE)) {
        sigaltstack(&oss, NULL);
    }

    /*
     * Now enter the trampoline again, but this time not as a signal
     * handler. Instead we jump into it directly. The functionally
     * redundant ping-pong pointer arithmetic is necessary to avoid
     * type-conversion warnings related to the `volatile' qualifier and
     * the fact that `jmp_buf' usually is an array type.
     */
    if (!sigsetjmp(old_env, 0)) {
        siglongjmp(coTS->tr_reenter, 1);
    }

    /*
     * Ok, we returned again, so now we're finished
     */

    pthread_mutex_unlock(&coTS->mutex);

    return &co->base;
}

void qemu_coroutine_delete(Coroutine *co_)
{
    CoroutineSigAltStack *co = DO_UPCAST(CoroutineSigAltStack, base, co_);

    qemu_free_stack(co->stack, co->stack_size);
    g_free(co);
}

CoroutineAction qemu_coroutine_switch(Coroutine *from_, Coroutine *to_,
                                      CoroutineAction action)
{
    CoroutineSigAltStack *from = DO_UPCAST(CoroutineSigAltStack, base, from_);
    CoroutineSigAltStack *to = DO_UPCAST(CoroutineSigAltStack, base, to_);
    CoroutineThreadState *s = coroutine_get_thread_state();
    int ret;

    s->current = to_;

    ret = sigsetjmp(from->env, 0);
    if (ret == 0) {
        siglongjmp(to->env, action);
    }
    return ret;
}

Coroutine *qemu_coroutine_self(void)
{
    CoroutineThreadState *s = coroutine_get_thread_state();

    return s->current;
}

bool qemu_in_coroutine(void)
{
    CoroutineThreadState *s = pthread_getspecific(thread_state_key);

    return s && s->current->caller && !s->current->cpu;
}

bool qemu_in_coroutine_cpu(void)
{
    CoroutineThreadState *s = pthread_getspecific(thread_state_key);

    return s && s->current->caller && s->current->cpu;
}

