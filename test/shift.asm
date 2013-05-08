LDA	d
BSA shift_4_l
STA d
HLT

d,	DEC 10

shift_4_l,
	DEC 0	//	per l'indirizzo di ritorno
	CIL
	CIL
	CIL
	CIL
	BUN shift_4_l I

