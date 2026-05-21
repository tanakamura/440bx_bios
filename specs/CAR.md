# Cache-As-RAM Notes

- Target CPU is Pentium II (P6 family).
- DRAM is not available yet during early bring-up, so writable/executable temporary storage must come from cacheable address ranges.

## What CAR means here

- Generated code cannot be placed in ROM.
- Before DRAM init, generated code or stack/data must live in a cache-resident address range.
- In practice this means a `WB`-typed linear address range in the CPU cache hierarchy.

## Pentium II implications

- For this project, `CAR` should be understood as using the normal write-back cache hierarchy, not a special separate RAM.
- If external L2 is enabled and working, it can contribute to CAR behavior.
- Do not assume only L2 is involved; instruction/data caching can use the normal hierarchy.

## Executing generated code from CAR

- Generated code can be written into a `WB` cacheable address range and executed from that same address.
- This does not require ROM or DRAM backing to succeed immediately, as long as the code remains resident in cache.
- In early bring-up, keep generated code in a CAR area separate from the ROM execution window.

## Instruction visibility after writing code

- Instruction fetches can observe code written into a cacheable CAR region.
- Do not think of this as “L1D must write back to DRAM before L1I can see it”.
- The important rule is self-modifying-code synchronization: after writing instruction bytes, execute a serializing step before jumping to them.
- For practical tests here, use a serializing instruction such as `CPUID` between writing code bytes and executing them.

## Current working assumptions

- ROM execution window and CAR scratch/code area should be kept separate.
- Large delay loops are undesirable because ROM execution is slow.
- CAR stability must be validated with minimal tests before relying on C code.

## Current board observations

- During CAR bring-up, making the ROM execution window `0xF0000-0xFFFFF` `WB` too early destabilized SMBus/SPD probing. Keep the pre-DRAM/CAR path conservative.
- After leaving CAR and jumping through stage2 to stage3 in high DRAM, the
  runtime may switch `0xC0000-0xFFFFF` from ROM decode to PAM shadow DRAM.
- Post-CAR fixed MTRRs should make conventional RAM `0x00000-0x9ffff` `WB`. The shadow windows `0xc0000-0xfffff` are also `WB` after CAR; `0xa0000-0xbffff` stays `UC` for VGA/MMIO compatibility.
- The BIOS real-mode thunk/DPT/GDT now live in `0xf0000-` shadow DRAM, so the old low-memory private areas at `0x80000`/`0x9fc00` are not reserved for BIOS runtime.
- On this board, explicitly enabling L2 during the post-CAR transition made execution less stable, so leave L2 enable alone for now.

## Pentium II L2 observations on this machine

- `BBL_CR_CTL3` readback after a successful post-CAR transition was `0x0100040a`.
- In that state, `bit0 (L2Configured)=0` and `bit8 (L2Enabled)=0`.
- Forcing only `bit8` on changed the MSR readback to `0x0100250a`, but the simple bandwidth test did not improve at all. `L2 256KiB` remained DRAM-like, not L2-like.
- Forcing both `bit0` and `bit8` produced `cfg=1 en=1` readback, but execution immediately became corrupted afterward instead of showing a stable L2 speedup.
- Conclusion for this board/CPU pair: L2 is not safely usable with a naive `BBL_CR_CTL3 |= 0x101` style enable.
- A proper Pentium II Slot 1 L2 init likely needs a fuller sequence: latency programming, cache size / bank / physical range setup, and possibly L2 controller register programming before `L2Enabled` is meaningful.

## Why the naive L2 enable fails

- Existing Slot 1/P6 implementations do more than flip `BBL_CR_CTL3.L2Configured` and `BBL_CR_CTL3.L2Enabled`.
- A known implementation path does roughly this:
  - check `BBL_CR_CTL3.L2NotPresent`
  - validate clock ratio / FSB-derived constraints
  - program preliminary `BBL_CR_CTL3` fields
  - calculate and program L2 latency
  - calculate and program L2 size per bank
  - calculate and program L2 physical address range
  - optionally set ECC-related controls
  - walk all cache lines and initialize tags/data with explicit L2 controller commands
  - set `L2Configured`
  - invalidate
  - write L2 control register 5
  - finally set `L2Enabled`
- In other words, the external Slot 1 L2 behaves like a controller that needs explicit configuration and tag/data initialization first.
- On this machine, the observed corruption after forcing `cfg=1 en=1` is consistent with “controller not fully initialized” rather than “no L2 silicon exists”.

## Current minimal working L2 init

- On this machine, the following minimal sequence was enough to get a measurable L2 speedup:
  - read `L2REG0/2/3` through cache-configuration accesses
  - use `L2REG3[2:0]` as the physical range selector
  - set `BBL_CR_CTL3` size field to a value accepted by the controller
  - initialize all cache lines with `L2CMD_TWW | MESI=I` across 4 ways for `256KiB`
  - set `L2Configured`
  - `invd`
  - `write_l2(5, 0)`
  - set `L2Enabled`
- A confirmed post-init readback was:
  - `BBL_CR_CTL3*=0130250b`
  - `L2REG0=40`
  - `L2REG2=08`
  - `L2REG3=03`
- A direct experiment with `L2 sizefield=512K` was stable on this Pentium II system.
- In that configuration:
  - `L2 alias@size=0000`
  - `BBL_CR_CTL3*=0130450b`
  - `L1 16KiB: 2.53 B/cycle`
  - `L2 256KiB: 1.52 B/cycle`
  - `DRAM 4MiB: 0.68 B/cycle`
- The heavy L2 controller initialization loop should run from WB DRAM, not from the ROM window. Current bootblock copies an `l2svc` helper to `0x00190000` after post-CAR transition and calls it before installing `blobsvc`.
- Given Pentium II SKU expectations, `512KiB` is the current preferred sizefield over the earlier `256K` interpretation.
- With that init, the simple benchmark changed from roughly:
  - `L2 256KiB: 0.04 B/cycle`
  - `DRAM   4MiB: 0.03-0.04 B/cycle`
  to:
  - `L1  16KiB: 2.53 B/cycle`
  - `L2 256KiB: 1.52 B/cycle`
  - `DRAM   4MiB: 0.68 B/cycle`
- So L2 is clearly active and faster than DRAM.
- A naive “largest size field accepted by `BBL_CR_CTL3`” probe reported `4096K`, but that was rejected as a false interpretation of controller-accepted field values.
