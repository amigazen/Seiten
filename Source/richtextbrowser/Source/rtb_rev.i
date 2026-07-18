; SPDX-License-Identifier: BSD-2-Clause
; Copyright 2026 amigazen project
;

VERSION		EQU	1
REVISION	EQU	0
DATE	MACRO
		dc.b	'17.7.26'
	ENDM
VERS	MACRO
		dc.b	'richtextbrowser.gadget 1.0'
	ENDM
VSTRING	MACRO
		dc.b	'richtextbrowser.gadget 1.0 (17.7.26)',13,10,0
	ENDM
VERSTAG	MACRO
		dc.b	0,'$VER: richtextbrowser.gadget 1.0 (17.7.26)',0
	ENDM
