	BSA rs
	DEC 3
	HEX 0x1
	HEX 0x4
	HEX 0x8
	
	HLT
	
rs,
params,
	DEC 0
	LDA params I
	CMA
	INC
	STA n
	BUN next
	
loop,
	ISZ n
	BUN next
	BUN rs I
		
next,
	ISZ rs
	LDA rs I
	ADD r
	STA r
	BUN loop
	
r,  DEC 0	
n,	DEC 0

