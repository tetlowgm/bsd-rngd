# $FreeBSD$

PROG=	bsdrngd
SRCS=	main.c
MAN=
LDADD=	-lutil

.include <bsd.prog.mk>
