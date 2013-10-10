ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

SRCDIR = .
OUTDIR=bin
OUTFILE=$(OUTDIR)/inadyn

ifdef DEBUG
	CFLAGS += -g
endif
CC= $(CROSS)gcc
STRIPCMD = $(CROSS)strip
STRIPFLAGS = --strip-all --remove-section=.note --remove-section=.comment
MAKECMD= gmake
#LDFLAGS= -static
LDFLAGS= -rdynamic
LDLIBS= -ldl

OBJS=$(OUTDIR)/base64utils.o $(OUTDIR)/dyndns.o \
	$(OUTDIR)/errorcode.o $(OUTDIR)/get_cmd.o $(OUTDIR)/http_client.o \
	$(OUTDIR)/ip.o $(OUTDIR)/inadyn_hook.o $(OUTDIR)/os.o \
	$(OUTDIR)/tcp.o $(OUTDIR)/inadyn_cmd.o

SPLOBJ=$(OUTDIR)/os_unix.o

COMPILE=$(CC)  -Wall  -pedantic -c  $(CFLAGS) -o "$(OUTDIR)/$(*F).o" $(CFG_INC) "$<"
#STRIPMAIN=$(STRIPCMD) -N main $(OUTDIR)/os_unix.o
STRIPMAIN=sed -e 's/main/dumy/' $(OUTDIR)/os_unix.o > $(OUTDIR)/os_unix_wmain.o
LINK=$(CC) -o "$(OUTFILE)" $(LDFLAGS) $(CFLAGS) $(OBJS) $(OUTDIR)/os_unix_wmain.o $(LDLIBS)

# Pattern rules
$(OUTDIR)/%.o : $(SRCDIR)/%.c
	$(COMPILE)

# Build rules
all: $(OUTFILE)

$(OUTFILE): $(OUTDIR) $(OBJS) $(SPLOBJ)
	    $(STRIPMAIN)
	    $(LINK)
	    $(STRIPCMD) $(OUTFILE)

# Clean this project
clean:
	rm -f $(OUTFILE)
	rm -f $(OBJS) 
	rm -f $(SPLOBJ)
	rm -f $(OUTDIR)/os_unix_wmain.o

install:    
	cp $(OUTFILE) $(PREFIX)/
