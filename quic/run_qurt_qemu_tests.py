#!/pkg/qct/software/python/3.7.6/bin/python3

import time
import shlex
import subprocess
from subprocess import Popen, PIPE, TimeoutExpired
import argparse
import sys
import multiprocessing as mp
from glob import glob
import os.path
import random
import pathlib

class TimedoutProcess(object):
    def __init__(self, **args):
        self.__dict__ = { b'returncode': 124, }
        self.__dict__.update(args)

    def write(self, f):
        f.write(b'command: "' + bytes(self.__dict__['cmd'], encoding='ascii') + b'"\n')
        f.write(b'exit code: 124 (timed out)\n')
        dur_text = 'duration: ' + str(self.__dict__['dur_sec']) + ' sec\n'
        f.write(bytes(dur_text, encoding='ascii'))
        f.write(b'stdout:\n')
        f.write(self.stdout)
        f.write(b'---\n')
        f.write(b'stderr:\n')
        f.write(self.stderr)

def write_completed_proc(f, proc, dur_sec):
    args = ' '.join(proc.args)
    stdout = proc.stdout.decode('ascii', 'ignore')
    text = '''
command: {args}
exit code: {proc.returncode}
duration: {dur_sec} sec
output:
{stdout}
{proc.stderr}
'''.format(**locals())
    f.write(text.encode('ascii', 'ignore'))


def test_name(test_fullpath):
    return os.path.splitext(os.path.basename(test_fullpath))[0]

def run_lldb(prog, port_num):
    debug_orig = os.path.splitext(prog)[0] + '.elf'
    if os.path.exists(debug_orig):
        prog = debug_orig

    lldb = pathlib.Path("/prj/dsp/qdsp6/release/internal/HEXAGON/branch-8.5/linux64/latest/Tools/bin/hexagon-lldb");
    if lldb.exists():
        debug_cmd = f'/prj/dsp/qdsp6/release/internal/HEXAGON/branch-8.5/linux64/latest/Tools/bin/hexagon-lldb --batch --source ~bcain/src/qemu/crash_debug.lldb --one-line-before-file "file {prog}" --one-line-before-file "gdb-remote {port_num}"'
    else:
        debug_cmd = f'/prj/qct/llvm/release/internal/HEXAGON/branch-8.5/linux64/latest/Tools/bin/hexagon-lldb --batch --source ~bcain/src/qemu/crash_debug.lldb --one-line-before-file "file {prog}" --one-line-before-file "gdb-remote {port_num}"'

    debug_res = subprocess.run(shlex.split(debug_cmd), timeout=25., stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return debug_res

def run_one(prog, port_num, cmd_args, timeout_sec, **kwargs):
    test = test_name(prog)
    with Popen(cmd_args, **kwargs) as process:
        try:
            print(f'running {test} for {timeout_sec} sec')
            output, err = process.communicate('', timeout=timeout_sec)
        except TimeoutExpired as e:
            debug_res = run_lldb(prog, port_num)
            process.kill()
            err_text = e.stderr if e.stderr else b''
            err_text += b'debug:\n---\n' + debug_res.stdout + debug_res.stderr
            out_text = e.stdout + process.stdout.read()
#           raise TimeoutExpired(process.args, timeout_sec, output=process.stdout.read(), stderr=err_text)
            print(f'\tFAIL: {test} timed out')
            raise TimeoutExpired(process.args, timeout_sec, output=out_text, stderr=err_text)

        pass_fail = 'PASS' if process.returncode == 0 else 'FAIL'
        print(f'\t{pass_fail}: {test}')
        return subprocess.CompletedProcess(cmd_args, process.returncode, output, err)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-b', '--binary-dir', type=str,
        default='/prj/qct/coredev/hexagon/austin/scratch/users/bcain/qemu/qurt_kona_tests',
        help='Path to QuRT test case binaries',
        required=False)
    parser.add_argument('-o', '--output-dir', type=str,
        help='Path to put logs/output',
        default=os.path.join(os.getcwd(), 'qurt_test_' + time.strftime('%Y%b%d_%H%M%S')),
        required=False)
    parser.add_argument('--timeout-sec', '-t', type=int,
        help='Per-test timeout in seconds',
        default=300,
        required=False)
    parser.add_argument('-q', '--qemu-bin-dir', type=str,
        help='Path to find qemu binary to test',
        default='/prj/dsp/qdsp6/release/internal/HEXAGON/branch-8.5/linux64/latest/Tools/bin',
        required=False)
    parser.add_argument('-y', '--enable-yield', action='store_true',
        help='Enable the yield cmdline arg for some test cases',
        default=False,
        required=False)
    parser.add_argument('-e', '--extra-cpu-args', type=str,
        help='Extra arguments to pass to QEMU\'s "cpu" argument',
        default='',
        required=False)
    parser.add_argument('-i', '--icount', type=str,
        help='Use the icount value, set to ICOUNT',
        default=None,
        required=False)
    parser.add_argument('-j', '--proc-count', type=int,
        help='Process count',
        default=int(mp.cpu_count() * .85),
        required=False)
    args = parser.parse_args()

    proc_count = args.proc_count
    timeout_sec = args.timeout_sec

    def get_timeout(test_path):
        if 'pipe' in test_path:
            return 3 * args.timeout_sec
        elif 'sysclk' in test_path:
            return max(300, args.timeout_sec)
        return args.timeout_sec

    test_paths = [ (t, get_timeout(t)) for t in glob(os.path.join(args.binary_dir, '*.pbn'))]
    prog = os.path.abspath(os.path.join(args.qemu_bin_dir, 'qemu-system-hexagon'))
    if not os.path.exists(prog):
        raise Exception(f'could not find QEMU binary at "{prog}"')

    def run_tests(t_cfg):
        (t, timeout_sec) = t_cfg

        port_num = random.randint(1235, 60000)
        cmd = '{} -gdb tcp::{} -display none -monitor none -m 4G -no-reboot -kernel {}'.format(prog, port_num, t)

        if args.icount != None:
            cmd += f' -icount shift={args.icount}'

        test = test_name(t)
#       if 'cpz' in test:
#           cmd += ' -m 37G'
#           print(f'MODIFIED CMDLINE ARGS FOR "{test}": {cmd}')
        yield_needers = (
            'qurt_anysignal_t1_bootimg',
            'qurt_anysignal_t2_bootimg',
            'qurt_cpz_restart_bootimg',
            'qurt_fastint_t4_bootimg',
            'qurt_fastint_t5_bootimg',
            'qurt_fastint_t6_bootimg',
            'qurt_fastint_t7_bootimg',
            'qurt_fastint_t8_bootimg',
            'qurt_mailbox_t1_bootimg',
            'qurt_hvx_t9_bootimg',
            'qurt_isr_t4_bootimg',
            'qurt_isr_t3_bootimg',
            'qurt_isr_t2_bootimg',
            'qurt_isr_t1_bootimg',
            'qurt_prio_set_t2_bootimg',
            'qurt_prio_set_t3_bootimg',
            'qurt_prio_set_t4_bootimg',
            'qurt_profile_task_ready_time_bootimg',
            'qurt_scheduler_t5_bootimg',
            'qurt_signal_any_t1_bootimg',
            'qurt_signal_any_t2_bootimg',
            'qurt_trap_mini_unused_bootimg',
            'qurt_int_t1_bootimg',
            'qurt_qdi_buffer_lock_t1_bootimg',
            'qurt_thread_detach_mpd_bootimg',
            'qurt_thread_detach_t1_bootimg',
            'qurt_thread_join_mpd_bootimg',
            'qurt_proc_t3_bootimg',
        )
        if args.enable_yield:
            if test in yield_needers:
                cmd += ' -cpu v67,sched-limit=on'
                print(f'MODIFIED CMDLINE ARGS FOR "{test}": {cmd}')
        elif args.extra_cpu_args:
            cmd += f' -cpu v67,{args.extra_cpu_args}'
        t0 = time.time()
        try:
            res = run_one(t, port_num, shlex.split(cmd), timeout_sec, stdout=PIPE, stderr=PIPE)
            t_end = time.time()
            return (t, False, res, t_end - t0)
        except subprocess.TimeoutExpired as e:
            dur_sec = time.time() - t0
            return (t, True, TimedoutProcess(cmd=cmd, stdout=e.stdout, stderr=e.stderr, dur_sec=dur_sec), dur_sec)

#   test_paths = test_paths[:5]
    test_count = len(test_paths)
    test_dir = os.path.dirname(args.binary_dir)
    print(f'Found {test_count} total cases in {test_dir}')

    t0 = time.time()
    start_timestamp = time.ctime()
    SKIP_LIST_TERMS = ()
    SKIP_LIST_TERMS = ('osam', 'power', 'rmutex', 'rmutex_timed', 'qurt_sleep', 'sysclk',)
    SOLO_LIST_TERMS = ()
    skipped = [t for t in test_paths if any(skip_term in t[0] for skip_term in SKIP_LIST_TERMS)]
    solo    = [t for t in test_paths if any(solo_term in t[0] for solo_term in SOLO_LIST_TERMS)]
    test_paths = [t for t in test_paths if not((t in solo) or (t in skipped))]

    # for fileio test:
    os.chdir(args.binary_dir)
    assert('fin' in os.listdir('.'))

    os.makedirs(args.output_dir)

    # Start Tests:

    with mp.Pool(processes=1) as p:
        res = p.map_async(run_tests, solo)
        print('\nSOLO phase\n')
        results = res.get()

    with mp.Pool(processes=proc_count) as p:
        res = p.map_async(run_tests, test_paths)
        print('\nPARALLEL phase\n')
        results += res.get()

    for (t, _) in skipped:
        print(f'SKIPPED: {test_name(t)}')

    total_dur_sec = time.time() - t0

    # Report results:

    for test, expired, res, dur_sec in results:
        fname = test_name(test)
        passed = not expired and res.returncode == 0
        log_filename = ('pass_' if passed else 'fail_') + '{}.log'.format(fname)
        log_path = os.path.join(args.output_dir, log_filename)
        with open(log_path, 'wb') as f:
            if expired:
                res.write(f)
            else:
                write_completed_proc(f, res, dur_sec)
            f.write(b'\n\n')
    passes = sorted([test_name(test) for test, expired, res, _ in results if not expired and res.returncode == 0])
    fails = sorted([test_name(test) for test, expired, res, _ in results if expired or (not expired and res.returncode != 0)])
    skips = sorted([test_name(test_path) for test_path, _ in skipped])

    summary_path = os.path.join(args.output_dir, 'summary.log')
    fail_list = '\n\t'.join(fails)
    skip_list = '\n\t'.join(skips)
    summary = f'''
test cases: {args.binary_dir}
qemu-path: {prog}
per-test default timeout: {timeout_sec} sec
total duration: {total_dur_sec} sec
executed on: {start_timestamp}
UTC sec: {t0}

pass count: {len(passes)}
fail count: {len(fails)}
skip count: {len(skips)}

FAILED:
\t{fail_list}
SKIPPED:
\t{skip_list}
'''
    with open(summary_path, 'wt') as f:
        f.write(summary)
    with open(os.path.join(args.output_dir, 'summary_passes.txt'), 'wt') as f:
        f.write('\n'.join(passes))
        f.write('\n')
    with open(os.path.join(args.output_dir, 'summary_failures.txt'), 'wt') as f:
        f.write('\n'.join(fails))
        f.write('\n')
    print(summary)

    sys.exit(0 if len(fails) == 0 else 1)

