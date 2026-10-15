# Pinned dependencies for R1358-B

Lightrec: https://github.com/pcercuei/lightrec
Revision: 853d0c81f33a7498c9f8ea8a42a19fae48132bb6 (source vendored).
GNU Lightning: vendored from emukidid/pcsxgc revision
 a30b271718541dce8a8fe161b56b90553f66ab7d, pcsx_rearmed/deps/lightning,
with its prepared include/lightning/lightning.h.
Upstream remote documented in .gitrepo: https://github.com/pcercuei/gnu_lightning.git.

Lightrec configuration: threads OFF, first-pass interpreter OFF, external code
buffer ON, logging OFF, memset substitution/idle detection OFF. Conservative
register-only IOP pilot; guest memory, COP0, branches, exceptions and timing
remain with the existing core. GNU Lightning's PPC jit_flush uses libogc
DCFlushRange + ICInvalidateRange on GEKKO instead of POSIX page queries.

Original licenses and copyright notices are included in each dependency.
Lightrec: LGPL-2.1-or-later in source SPDX headers (upstream COPYING included).
GNU Lightning: LGPL-3.0-or-later; COPYING and COPYING.LESSER included.
TLSF: original license in tlsf/tlsf.h and tlsf/tlsf.c.
This archive includes all modified library source and offline build inputs.
Rebuilding and relinking with modified libraries is supported via build_r1358.sh.
