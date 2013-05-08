	LDA r
	ADD n1
	ISZ 1
	ISZ loops
	BUN 1
	STA r
	HLT

loops, DEC -4

n1,	DEC 2
n2, DEC 3
n3, DEC 4
n4, DEC 5

r, DEC 0