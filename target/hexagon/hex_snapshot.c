#include "hex_snapshot.h"

#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "qemu/rcu.h"
#include "sysemu/cpu-timers.h"
#include "sysemu/cpus.h"

#include <stdlib.h>
#include <string.h>

#define RESTORE_EXIT_STATUS 42

void hex_snapshot_send_request(CPUHexagonState *env,
                               snapshot_operation operation)
{
    if (write(env->snapshot_req_pipe[1], &operation,
              sizeof(snapshot_operation)) == -1) {
        CPUState *cs = env_cpu(env);
        cpu_abort(cs, "snapshot pipe write failed: %s\n", strerror(errno));
    }
}

static void run_fork_server(void)
{
    while (true) {
        pid_t pid = fork();
        if (pid == -1) {
            error_report("could not fork: %s\n", strerror(errno));
            abort();
        } else if (!pid) {
            // Running in child.
            break;
        }

        // Running in parent.

        int status = 0;
        int ret = waitpid(pid, &status, 0);
        if (ret == -1) {
            error_report("could not waitpid: %s\n", strerror(errno));
            abort();
        }

        if (WIFSIGNALED(status)) {
            error_report("child signaled: %s\n", strsignal(WTERMSIG(status)));
            abort();
        } else if (!WIFEXITED(status)) {
            error_report("child did not exit\n");
            abort();
        } else if (WEXITSTATUS(status) != RESTORE_EXIT_STATUS) {
            error_report("child exited with status: %d\n", WEXITSTATUS(status));
            abort();
        }
    }
}

static void handle_snapshot_request(void *ctx)
{
    CPUHexagonState *env = ctx;

    snapshot_operation operation = 0;
    if (read(env->snapshot_req_pipe[0], &operation,
             sizeof(snapshot_operation)) == -1) {
        error_report("could not read from snapshot pipe: %s", strerror(errno));
        abort();
    }

    switch (operation) {
    case SNAPSHOT_TAKE: {
        CPUState *cpu = NULL;

        cpu_disable_ticks();
        pause_all_vcpus();

        // Ensure that the RCU thread is restored when forking.
        rcu_enable_atfork();

        run_fork_server();

        // Running in child.

        rcu_disable_atfork();

        CPU_FOREACH (cpu) {
            // All vCPU threads have been destroyed by fork.
            cpu->created = false;
            qemu_init_vcpu(cpu);
        }

        // We cannot simply restart all vCPUs, we need to skip those not in run
        // mode mode, as they were not running before. This is likely a bug
        // caused by the Hexagon implementation which stops vCPUs instead of
        // halting them.

        cpu_enable_ticks();

        qemu_clock_enable(QEMU_CLOCK_VIRTUAL, true);
        CPU_FOREACH (cpu) {
            HexagonCPU *hex_cpu = HEXAGON_CPU(cpu);
            CPUHexagonState *env = &hex_cpu->env;
            if (get_exe_mode(env) == HEX_EXE_MODE_RUN) {
                cpu_resume(cpu);
            }
        }

        break;
    }
    case SNAPSHOT_RESTORE: {
        exit(RESTORE_EXIT_STATUS);
    }
    default: {
        error_report("Unsupported snapshot operation 0x%x\n", operation);
        abort();
    }
    }
}

void hex_snapshot_realize(CPUHexagonState *env, Error **errp)
{
    CPUState *cs = env_cpu(env);
    if (cs->cpu_index == 0) {
        int pipe_result = pipe(env->snapshot_req_pipe);
        if (pipe_result) {
            error_setg_errno(errp, errno, "snapshot pipe creation failed");
            return;
        }

        qemu_set_fd_handler(env->snapshot_req_pipe[0], handle_snapshot_request,
                            NULL, env);
    } else {
        CPUState *cpu0_s = NULL;
        CPUHexagonState *env0 = NULL;
        CPU_FOREACH (cpu0_s) {
            assert(cpu0_s->cpu_index == 0);
            env0 = &(HEXAGON_CPU(cpu0_s)->env);
            break;
        }

        memcpy(env->snapshot_req_pipe, env0->snapshot_req_pipe,
               sizeof(env->snapshot_req_pipe));
    }
}