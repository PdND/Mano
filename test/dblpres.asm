	LDA al
	ADD bl
	STA yl
	CLA
	CIL
	ADD ah
	ADD bh
	STA yh
	
	HLT
	
al,	HEX 1
ah, HEX 1
bl, HEX 2
bh, HEX 2
yl, HEX 3
yh, HEX 3
