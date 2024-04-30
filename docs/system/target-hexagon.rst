.. _Hexagon-System-emulator:

Hexagon System emulator
-----------------------

Use the ``qemu-system-hexagon`` executable to simulate a 32-bit Hexagon
machine.

Hexagon DSPs are suited to various functions and generally appear in a
"DSP subsystem" of a larger system-on-chip (SoC).

Hexagon System Architecture
===========================

Hexagon DSPs are often included in a subsystem that looks like the diagram
below.  Instructions are loaded into DDR before the DSP is brought out of
reset and the first instructions are fetched from DDR via the EVB/reset vector.

In a real system, a TBU/SMMU would normally arbitrate AXI accesses but
we don't have a need to model that for QEMU.

.. admonition:: Diagram

 .. code:: text

              AHB (local) bus                     AXI (global) bus
                    │                                 │
                    │                                 │
       ┌─────────┐  │       ┌─────────────────┐       │
       │ L2VIC   ├──┤       │                 │       │
       │         ├──┼───────►                 ├───────┤
       └─────▲───┘  │       │   Hexagon DSP   │       │
             │      │       │                 │       │        ┌─────┐
             │      │       │    N threads    │       │        │ DDR │
             │      ├───────┤                 │       │        │     │
        ┌────┴──┐   │       │                 │       ├────────┤     │
        │QTimer ├───┤       │                 │       │        │     │
        │       │   │       │                 │       │        │     │
        └───────┘   │       │   ┌─────────┐   │       │        │     │
                    │       │  ┌─────────┐│   │       │        │     │
        ┌───────┐   │       │  │  HVX xM ││   │       │        │     │
        │QDSP6SS├───┤       │  │         │┘   │       │        │     │
        └───────┘   │       │  └─────────┘    │       │        └─────┘
                    │       │                 │       │
        ┌───────┐   │       └─────────────────┘       │
        │  CSR  ├───┤
        └───────┘   │   ┌──────┐   ┌───────────┐
                    │   │ TCM  │   │   VTCM    │
                        │      │   │           │
                        └──────┘   │           │
                                   │           │
                                   │           │
                                   │           │
                                   └───────────┘

Components
----------

* L2VIC: the L2 vectored interrupt controller.  Supports 1024 input
  interrupts, edge- or level-triggered.
* QTimer: ARMSSE-compatible programmable timer device. Its interrupts are
  wired to the L2VIC.  Architectural registers ``TIMER``, ``UTIMER`` read
  through to the QTimer device.
* QDSP6SS: DSP subsystem features, accessible to the entire SoC, including
  DSP NMI, watchdog, reset, etc.  Not yet modeled in QEMU.
* CSR: Configuration/Status Registers.  Not yet modeled in QEMU.
* TCM: DSP-exclusive tightly-coupled memory.  This memory can be used for
  DSPs when isolated from DDR and in some bootstrapping modes.
* VTCM: DSP-exclusive vector tightly-coupled memory.  This memory is accessed
  by some HVX instructions.
* HVX: the vector coprocessor supports 64 and 128-byte vector registers.
  64-byte mode is not implemented in QEMU.  The Supervisor Status Register
  field ``SSR.XA`` binds a DSP hardware thread to one of the eight possible
  HVX contexts.  The guest OS is responsible for managing this resource.


Bootstrapping
-------------
Hexagon systems do not generally have access to a block device.  So, for
QEMU the typical use case involves loading a binary or ELF file into memory
and executing from the indicated start address::

    $ qemu-system-hexagon -kernel ./prog -append 'arg1 arg2'

Semihosting
-----------
Hexagon supports a semihosting interface similar to other architectures'.
The ``trap0`` instruction can activate these semihosting calls so that the
guest software can access the host console and filesystem.

Scheduler
---------
The Hexagon system architecture has a feature to assist the guest OS
task scheduler.  The guest OS can enable this feature by setting
``SCHEDCFG.EN``.  The ``BESTWAIT`` register is programmed by the guest OS
to indicate the priority of the highest priority task waiting to run on a
hardware thread.  The reschedule interrupt is triggered when any hardware
thread's priority in ``STID.PRIO`` is worse than the ``BESTWAIT``.  When
it is triggered, the ``BESTWAIT.PRIO`` value is reset to 0x1ff.

Interrupts
----------
When interrupts arrive at a Hexagon DSP core, they are priority-steered to
be handled by an eligible hardware thread with the lowest priority.

Memory
------
Each hardware thread has an ``SSR.ASID`` field that contains its Address
Space Identifier.  This value is catenated with a 32-bit virtual address -
the MMU can then resolve this extended virtual address to a physical address.

TLBs
----
The format of a TLB entry is shown below.

.. note::
    The Small Core DSPs have a different TLB format which is not yet
    supported.

.. admonition:: Diagram

 .. code:: text

             6                   5                   4               3
       3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |v|g|x|A|A|             |                                       |
      |a|l|P|1|0|     ASID    |             Virtual Page              |
      |l|b| | | |             |                                       |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

         3                   2                   1                   0
       1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      | | | | |       |                                             | |
      |x|w|r|u|Cacheab|               Physical Page                 |S|
      | | | | |       |                                             | |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


* ASID: this is the addres-space identifier
* A1, A0: the behavior of these cache line attributes are not modeled by QEMU.
* xP: the extra-physical bit is the most significant physical address bit.
* S: the S bit and the LSBs of the physical page indicate the page size
* val: this is the 'valid' bit, when set it indicates that page matching
  should consider this entry.

.. list-table:: Page sizes
   :widths: 25 25 50
   :header-rows: 1

   * - S-bit
     - Phys page LSBs
     - Page size
   * - 1
     - -
     - 4kb
   * - 0
     - 0b1
     - 16kb
   * - 0
     - 0b10
     - 64kb
   * - 0
     - 0b100
     - 256kb
   * - 0
     - 0b1000
     - 1MB
   * - 0
     - 0b10000
     - 4MB
   * - 0
     - 0b100000
     - 16MB

* glb: if the global bit is set, the ASID is not considered when matching
  TLBs.
* Cacheab: the cacheability attributes of TLBs are not modeled, these bits
  are ignored.
* RWX: read-, write-, execute-, enable bits.  Indicates if user programs
  are permitted to read/write/execute the given page.
* U: indicates if user programs can access this page.

Hexagon Features
================
.. toctree::
   hexagon/cdsp

