# R1253: texture function correction

The GS previously ignored TEX0.TCC and treated HIGHLIGHT/HIGHLIGHT2 as MODULATE.
TCC now follows each GS context, including TEX2 preserving existing TCC/TFX.
MODULATE and DECAL retain vertex alpha when TCC=0. With TCC=1, MODULATE
multiplies texture/vertex alpha, DECAL and HIGHLIGHT2 use texture alpha, and
HIGHLIGHT adds texture and vertex alpha. Both highlight modes add vertex alpha
to modulated RGB. Texture arithmetic saturates independently of framebuffer
COLCLAMP, following the included PCSX2 GSDrawScanline.cpp AlphaTFX/ColorTFX.

The regression suite passes all 149 tests. Four older fixtures were corrected
to set TCC explicitly when their expected pixel includes texture alpha; the
COLCLAMP test now verifies actual framebuffer blend overflow as well as
texture-stage saturation. New tests cover all eight TFX/TCC combinations,
highlight alpha saturation, separate contexts and TEX2 preservation.

This correction is not proof that the OSDSYS menu is reached. Fresh coldboot
comparison is still being evaluated; no forced guest state changes are used.
