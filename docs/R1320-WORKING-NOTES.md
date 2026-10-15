# R1320 working notes — exact EE control-path admission audit

R1320 verifies the precise-block control-transfer admission boundary against the checked-in PCSX2 R5900 opcode tables instead of relying on hand-maintained comments.

The audit exhausts the primary branch/jump selectors, SPECIAL JR/JALR, all REGIMM selectors, and the complete COP1 rs/rt selector plane. It requires the block terminal policy to agree with the architectural control-opcode set and requires every legal canonical control encoding to be accepted by both the single-instruction PPC translator and the resident delay-slot block translator.

The verified canonical set contains 24 control encodings: J/JAL, JR/JALR, BEQ/BNE/BLEZ/BGTZ and likely forms, BLTZ/BGEZ and likely/link/link-likely forms, plus BC1F/BC1T/BC1FL/BC1TL. The R1320 discovery run reported 24/24 accepted by both PPC paths and zero selector mismatches.

This is a host-side translation/admission verification milestone. Generated PPC buffers are inspected through translator return values and are not executed natively on the host. Physical Wii stability and performance remain separate release gates.
