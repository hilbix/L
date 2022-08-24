# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

# I really have no idea why following is needed
# Also I have not the slightes idea how to fix the bugs (see below)

# Override this to install somewhere else
INSTALL_PREFIX=/usr/local

# Rest usually should not need changes

SRCS=$(wildcard *.c)
BINS=$(SRCS:.c=)
SETS=$(shell sed -n 's/^[	 /]*\*[*	 ]*LIBS:[	 ]*//p' $(SRCS))
CFLAGS=-Wall -O3 -g $(shell bash -xc 'pkg-config --cflags $(SETS)') $(shell sed -n 's/^[	 /]*\*[*	 ]*GCCFLAGS:[	 ]*//p' $(SRCS))
LDLIBS=$(shell bash -xc 'pkg-config --libs $(SETS)')

TMPDIR := .tmp

.PHONY:	love
love:	all

.PHONY:	all
all:	$(BINS)

hw:
	echo $(LIBS)
	echo $(LDLIBS)

# Do not depend on $(BINS), as this usually is run with sudo
.PHONY:	install
install:
	install -s $(BINS) $(INSTALL_PREFIX)/bin

.PHONY:	clean
clean:
	$(RM) $(BINS)
	$(RM) -r $(TMPDIR)

.PHONY:	debian
debian:
	sudo apt-get install $(APT_INSTALL_FLAGS) -- `sed -n 's/^[	 /]*\*[*	 ]*DEBIAN:[	 ]*//p' *.c | tr ' ' '\n' | sort -u`

.PHONY:	test
test:	all
	./Test.sh

.PHONY:	debug
debug:	all
	/usr/bin/gdb -q -nx -nw -ex r --args '$(BINS)' -- $(DEBUG_ARGS)

# Following is quite wrong, as it recompiles all *.o,
# not only the one for the individual target,
# and also it re-links everything needlessly in case a single *.o changes.
# (I can live with this flaw, as it is quite better than any workaround I was able to find.)
#
# My way ALWAYS looks like following:
#
# - each .c creates one .o and one .d (for auto-dependencies)
# - each .o creates one bin (THIS IS THE PROBLEMATIC PART)
# - everything else is handled with includes (handled by .d)
#
# I really have not the slightest idea how to formulate this for the current directory,
# as bins do not have extensions, as following does not work as expected:
#
#	%: $(TMPDIR)/%.o
#
# Note that creating the bin in .tmp/ or similar is bullshit.

OBJS := $(SRCS:%.c=$(TMPDIR)/%.o)
$(BINS):	$(OBJS)
	$(CC) $(LDFLAGS) $(TMPDIR)/$@.o -o $@ $(LDLIBS)

# I really have no idea why all this shit is needed

$(TMPDIR)/%.o:	%.c $(TMPDIR)/%.d Makefile | $(TMPDIR)
	$(CC) -MT $@ -MMD -MP -MF $(TMPDIR)/$*.d $(CFLAGS) -o $@ -c $<

$(TMPDIR):
	@mkdir -p $@

DEPS := $(SRCS:%.c=$(TMPDIR)/%.d)
$(DEPS):

include $(wildcard $(DEPS))

