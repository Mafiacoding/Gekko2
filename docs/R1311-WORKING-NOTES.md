# R1311 unreleased signed-overflow correction

ADD/SUB/DADD/DSUB/ADDI/DADDI now raise a synchronous integer-overflow
exception (Cause.ExcCode 12) before writing the destination. A zero
destination still detects overflow. Word forms compare signed low 32-bit
operands and sign-extend valid results; doubleword forms operate on 64 bits.
Upper 64-bit GPR lanes are preserved. Unsigned variants retain wrapping.

The scalar core performs arithmetic in unsigned C types to avoid undefined
signed overflow. Direct PPC bodies detect the sign transition from original
operands and the wrapped result. Fault paths use an audited EABI helper
frame and set the synchronous exception marker. The six forms can also join
precise blocks and fused delay slots.

Verification: 12,288 independent integer/range-oracle cases in each actual
linked JIT and interpreter build. Covers zero/aliased destinations, signed
limits, EXL/BEV, delay-slot EPC/BD, unchanged faulting destinations and upper
lanes. JIT cases require observed native execution. All 215 host tests pass.
Platform allocation/cache services are mocked; this is not Wii FPS evidence.

This is an additional source milestone, not completion of the six-area
dynarec plan. No release ELF/DOL/checkpoint is delivered.
