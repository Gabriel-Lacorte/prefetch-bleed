# Prefetch Bleed

An evolution of [`exploits-forsale/prefetch-tool`][upstream], which itself
adapts Will Vandevanter's
[**EntryBleed**](https://www.willsroot.io/2022/12/entrybleed.html)
research to Windows. The upstream tool ships four hand-tuned
algorithms, dispatched on the CPU brand string (`Intel`, `Intel N200`,
`AMD`, `AMD Mobile`), and uses a "two-leaks-in-a-row" heuristic to
filter noise. `prefetch-bleed` replaces all of that with a single
statistical pipeline that performs reliably across vendors and
microarchitectures with no per-CPU tuning.

[upstream]: https://github.com/exploits-forsale/prefetch-tool

## Technique

For every 2 MiB-aligned slot in the kernel virtual range, time a
`prefetchnta + prefetcht2` chain, preceded by an invalid
`syscall` to force a kernel entry that warms the page-walker caches
for legitimate KVAs. Then:

1. **Min-of-N sampling.** Each slot is probed `N` times; only the
   minimum TSC delta is kept. The minimum is the cleanest possible
   observation, free of preemption / SMI / NMI tail noise.
2. **Median baseline.** The median across all slot timings defines the
   noise floor. Median is robust to the heavy tail that drags the
   arithmetic mean off-target.
3. **Bidirectional run detection.** The kernel image manifests as a
   contiguous run of slots that deviate from the median by &gt;~7 %, in
   *either* direction, fast (Intel: page-walker / L2 TLB hits) or
   slow (AMD: present-mapping page walks). Detecting both signs
   uniformly removes the upstream vendor branching.
4. **Quorum across passes.** Steps 1&ndash;3 run `passes` times. The mode
   of the candidate slot indices is accepted only if it gathers
   `quorum` matching votes, a real consensus, generalising
   the upstream "two-in-a-row" check.

The sampling thread is pinned to `THREAD_PRIORITY_TIME_CRITICAL` for
the whole sweep to keep the TSC stream stable.

## Credits

| | |
|---|---|
| [Will Vandevanter](https://www.willsroot.io/2022/12/entrybleed.html) | Original EntryBleed research on Linux |
| [@exploits-forsale](https://github.com/exploits-forsale/prefetch-tool) | Windows port and per-vendor heuristics this work builds on |
| Elias Bachaalany | Original timed-prefetch assembly skeleton |

