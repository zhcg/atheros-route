all: ripquery

include ../MCONFIG
include ../MRULES

OBJS=	query.o

ripquery: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

install: ripquery
	install -o root -g root -m $(BINMODE) ripquery $(INSTALLROOT)$(SBINDIR)
	install -m$(MANMODE) ripquery.8 $(INSTALLROOT)$(MANDIR)/man8
clean:
	rm -f $(OBJS) ripquery

$(OBJS): ../version.h
