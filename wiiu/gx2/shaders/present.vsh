; Gekko2 Wii U presenter - vertex shader (Latte assembly for latte-assembler).
; Passes a 2D clip-space position and a texture coordinate through. UNVERIFIED: written from memory of the wut
; samples, never assembled or run by the author. If latte-assembler rejects it, send the exact error text.
; $MODE = "UniformRegister"

; $ATTRIB_VARS[0].name = "aPosition"
; $ATTRIB_VARS[0].type = "float2"
; $ATTRIB_VARS[0].location = 0
; $ATTRIB_VARS[1].name = "aTexCoord"
; $ATTRIB_VARS[1].type = "float2"
; $ATTRIB_VARS[1].location = 1

; $NUM_SPI_VS_OUT = 1
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0

00 CALL_FS NO_BARRIER
01 ALU: ADDR(32) CNT(2)
      0  x: MOV         R3.x, R1.x
         y: MOV         R3.y, R1.y
         z: MOV         R3.z, 0.0f
         w: MOV         R3.w, (0x3F800000, 1.0f).x
02 EXP_DONE: POS0, R3
03 EXP_DONE: PARAM0, R2.xyzz NO_BARRIER
END_OF_PROGRAM
