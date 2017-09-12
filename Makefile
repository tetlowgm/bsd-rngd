# $FreeBSD$

PROG=	bsdrngd
SRCS=	main.c
MAN=
LDADD=	-lutil

# XXX: This would be dropped if installed in base.
BINDIR=	/usr/local/sbin

.include <bsd.prog.mk>
