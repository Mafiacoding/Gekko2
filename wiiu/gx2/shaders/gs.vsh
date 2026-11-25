; Gekko2 Wii U GS renderer - vertex shader (Latte assembly for latte-assembler).
; The CPU converts GS window coordinates to clip space, so the shader only passes everything through.
; Location 0 position (x,y,z,1) arrives in R1, location 1 colour (r,g,b,a; GS 0x80 = 1.0) in R2,
; location 2 texture coordinate (u,v normalised) in R3.
; $MODE = "UniformRegister"

; $ATTRIB_VARS[0].name = "aPosition"
; $ATTRIB_VARS[0].type = "vec4"
; $ATTRIB_VARS[0].location = 0
; $ATTRIB_VARS[1].name = "aColor"
; $ATTRIB_VARS[1].type = "vec4"
; $ATTRIB_VARS[1].location = 1
; $ATTRIB_VARS[2].name = "aTexCoord"
; $ATTRIB_VARS[2].type = "vec4"
; $ATTRIB_VARS[2].location = 2

; $SPI_VS_OUT_CONFIG.VS_EXPORT_COUNT = 1
; $NUM_SPI_VS_OUT_ID = 1
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0
; $SPI_VS_OUT_ID[0].SEMANTIC_1 = 1

00 CALL_FS NO_BARRIER
01 EXP_DONE: POS0, R1
02 EXP: PARAM0, R2 NO_BARRIER
03 EXP_DONE: PARAM1, R3 NO_BARRIER
END_OF_PROGRAM
