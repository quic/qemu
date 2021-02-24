hexagon_ss = ss.source_set()

attribs_def = meson.current_source_dir() / 'attribs_def.h'

#
# Step 2
# We use the do_qemu.py script to generate the following files
#

semantics_gen = executable('gen_semantics', ['gen_semantics.c'],
    native: true, build_by_default: false)

semantics_generated = custom_target(
    'semantics_generated.pyinc',
    output: 'semantics_generated.pyinc',
    input: semantics_gen,
    command: [semantics_gen, '@OUTPUT@'],
)

opcodes = custom_target(
    'opcodes.c',
    output : ['qemu_def_generated.h', 'qemu_wrap_generated.h', 'opcodes_def_generated.h', 'op_attribs_generated.h', 'op_regs_generated.h', 'printinsn_generated.h',],
    input : 'do_qemu.py',
    command : [python, '@INPUT@', 'SYSTEM', semantics_generated, attribs_def, ],
    depends: [semantics_generated],
)
hexagon_ss.add(opcodes)

gen_dectree_import = executable('gen_dectree_import',
    ['gen_dectree_import.c', opcodes],
    native: true, build_by_default: false)

iset = custom_target(
    'gen_dectree_import',
    output : ['iset.py',],
    command : [gen_dectree_import, '@OUTPUT@'],
    depends: gen_dectree_import)
hexagon_ss.add(iset)

dectree = custom_target(
    'dectree_generated.h',
    output : ['dectree_generated.h',],
    input : 'dectree.py',
    command : ['PYTHONPATH=' + meson.current_build_dir(), python, '@INPUT@', '@OUTPUT@'],
    depends: iset,
)
hexagon_softmmu_ss = ss.source_set()
hexagon_ss.add(dectree)

hexagon_ss.add(files(
   'cpu.c',
   'cpu_helper.c',
   'hexswi.c',
   'translate.c',
   'op_helper.c',
   'gdbstub.c',
   'genptr.c',
   'reg_fields.c',
   'decode.c',
   'iclass.c',
   'opcodes.c',
   'printinsn.c',
   'arch.c',
   'fma_emu.c',
   'conv_emu.c',
   'mmvec/decode_ext_mmvec.c',
   'mmvec/system_ext_mmvec.c',
   'mmvec/mmvec_qfloat.c',
   'int16_emu.c',
   'system.c',
   'arch_options_calc.c',
   'hmx/ext_hmx.c',
   'hmx/mpy_hmx.c',
   'hmx/system_ext_hmx.c',
   'dma_adapter.c',
   'dma.c',
   'dma_descriptor.c',
   'desc_tracker.c',
   'uarch/queue.c',
))

hexagon_softmmu_ss.add(files(
  'hex_mmu.c',
))

target_arch += {'hexagon': hexagon_ss}
target_softmmu_arch += {'hexagon': hexagon_softmmu_ss}
