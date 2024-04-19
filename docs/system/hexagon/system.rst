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
          │      │       │                 │       │        │ DDR │
          │      ├───────┤                 │       │        │     │
     ┌────┴──┐   │       │                 │       ├────────┤     │
     │QTimer ├───┤       │                 │       │        │     │
     │       │   │       └─────────────────┘       │        │     │
     └───────┘   │                                 │        │     │
                 │                                 │        │     │
                 │                                 │        │     │
                 │                                 │        │     │
                 │                                 │        └─────┘
                 │                                 │
                 │                                 │


MMU, TLBs
---------

The format of a TLB entry is shown below.

.. note::
    The small core DSPs have a different TLB format which is not yet
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
