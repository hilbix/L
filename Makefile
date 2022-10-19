# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
# Read: Free as free beer, free speech, free baby.
# (Copyrighting babies is slavery.)

# This is a generic Makefile for GCC/GNU/Linux environments which:
# Version: 2022-10-19
#
# - Compiles all *.c to a bin accordingly
# - Never needs changes as everything can be kept in the sources.
#
# Unluckily, this is unbearable complex to express.  And error prone.
# And wrong.  Because options should be per-.c-file not global.
# Also it does not work for all types of dependencies,
# so recompilation might not be done if needed,
# or recompilation is done too often.  Sigh.

# BEGIN CONFIGURATION
# Override this to install somewhere else
INSTALL_PREFIX=/usr/local
# Set Debug flags (real debugging usually requires -O0)
DEBUG=-g
# Set Optimization flags
OPTIMIZE=-O3
# END CONFIGURATION
# Rest usually should not need changes

SRCS=$(wildcard *.c)
BINS=$(SRCS:.c=)
LIBS=$(shell sed -n 's/^[	 /]*\*[*	 ]*LIBS:[	 ]*//p' $(SRCS))
GCCFLAGS=$(shell sed -n 's/^[	 /]*\*[*	 ]*GCCFLAGS:[	 ]*//p' $(SRCS))
DEBIAN=$(shell sed -n 's/^[	 /]*\*[*	 ]*DEBIAN:[	 ]*//p' $(SRCS) | tr ' ' '\n' | sort -u)
CFLAGS=-Wall $(OPTIMIZE) $(DEBUG) $(shell bash -xc 'pkg-config --cflags -- $(LIBS)') $(GCCFLAGS)
LDLIBS=$(shell bash -xc 'pkg-config --libs -- $(LIBS)')

TMPDIR := .tmp

.PHONY:	love
love:	all

.PHONY:	all
all:	$(BINS)

# Hello World: Debug extracted configuration
.PHONY:	hw
hw:
	@/bin/bash -c 'printf "%15q=%s\\n" "$$@"' - INSTALL_PREFIX '$(INSTALL_PREFIX)' DEBUG '$(DEBUG)' OPTIMIZE '$(OPTIMIZE)' SRCS '$(SRCS)' LIBS '$(LIBS)' GCCFLAGS '$(GCCFLAGS)' LDLIBS '$(LDLIBS)' CFLAGS '$(CFLAGS)' SRCS '$(SRCS)' BINS '$(BINS)'

# Do not depend on $(BINS), as this usually is run with sudo
.PHONY:	install
install:
	install -s $(BINS) $(INSTALL_PREFIX)/bin/

.PHONY:	clean
clean:
	$(RM) $(BINS)
	$(RM) -r $(TMPDIR)

.PHONY:	debian
debian:
	sudo apt-get install $(APT_INSTALL_FLAGS) -- $(DEBIAN)

.PHONY:	test
test:	all
	./Test.sh

# make debug: assumes you only have one binary (so one *.c)
.PHONY:	debug
debug:	all
	DEBUG=: /usr/bin/gdb -q -nx -nw -ex r --args '$(BINS)' $(DEBUG_ARGS)

# I have not the slightes idea how to fix the bugs:
#
# Following is quite wrong, as it recompiles all *.o,
# not only the one for the individual target,
# and also it re-links everything needlessly in case a single *.o changes.
# (I can live with this flaw, as it is quite better than any workaround I was able to find.)
#
# My way ALWAYS looks like following:
#
# - each .c creates one .o and one .d (for auto-dependencies)
# - each .o creates one bin without extension (THIS IS THE PROBLEMATIC PART)
# - everything else is handled with includes (handled by .d)
#
# I really have not the slightest idea how to formulate this for the current directory,
# as bins do not have extensions, as following does not work as expected:
#
#	%: $(TMPDIR)/%.o
#
# Note that creating the bin in .tmp/ with extension .bin or similar
# and copy it out afterwards is plain bullshit.

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

