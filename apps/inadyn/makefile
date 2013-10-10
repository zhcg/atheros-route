SRCDIR = .
OUTDIR=bin
OUTFILE=$(OUTDIR)/inadyn

#ifdef DEBUG
	CFLAGS += -g
#endif
STRIPCMD=/usr/bin/strip
MAKECMD=gmake

OBJS=$(OUTDIR)/base64utils.o $(OUTDIR)/dyndns.o \
	$(OUTDIR)/errorcode.o $(OUTDIR)/get_cmd.o $(OUTDIR)/http_client.o \
	$(OUTDIR)/ip.o $(OUTDIR)/inadyn_hook.o $(OUTDIR)/os.o \
	$(OUTDIR)/os_unix.o $(OUTDIR)/tcp.o $(OUTDIR)/inadyn_cmd.o

COMPILE=gcc  -Wall  -pedantic -c  $(CFLAGS) -o "$(OUTDIR)/$(*F).o" $(CFG_INC) "$<"
STRIPMAIN=$(STRIPCMD) -N main $(OUTDIR)/os_unix.o
LINK=gcc -o "$(OUTFILE)" $(CFLAGS) $(OBJS)

# Pattern rules
$(OUTDIR)/%.o : $(SRCDIR)/%.c
	$(COMPILE)

# Build rules
all: $(OUTFILE)

$(OUTFILE): $(OUTDIR) $(OBJS) 
	    $(STRIPMAIN)
	    $(LINK)

# Clean this project
clean:
	rm -f $(OUTFILE)
	rm -f $(OBJS)


