; Gekko2 Wii U presenter - vertex shader (Latte assembly for latte-assembler).
; Passes position and texture coordinate straight through. The header and instruction layout follow the
; position+colour example in GaryOderNichts/wiiu-shaders (shaders.md); only the attribute names differ.
; Attribute location 0 arrives in R1, location 1 in R2.
; $MODE = "UniformRegister"

; $ATTRIB_VARS[0].name = "aPosition"
; $ATTRIB_VARS[0].type = "vec4"
; $ATTRIB_VARS[0].location = 0
; $ATTRIB_VARS[1].name = "aTexCoord"
; $ATTRIB_VARS[1].type = "vec4"
; $ATTRIB_VARS[1].location = 1

; $SPI_VS_OUT_CONFIG.VS_EXPORT_COUNT = 0
; $NUM_SPI_VS_OUT_ID = 1
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0

00 CALL_FS NO_BARRIER
01 EXP_DONE: POS0, R1
02 EXP_DONE: PARAM0, R2 NO_BARRIER
END_OF_PROGRAM
