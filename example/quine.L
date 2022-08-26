#!/usr/bin/env L
#
# When run, this program outputs itself
# (Note: Function "l" is not yet implemented)

0~	# push -1
0	# push 0
"l"$	# dump currently running program from line 0 to -1 (end)
>	# output program

# Effectively these are only 8 byte today
#
# When L starts gets the full parser written in L
# the output might look different to the input.
# Probably something like:
#	0"~"$0"l"$">"$
# But this is the very same program.
# It might even be that we erase numbers to make the minimum parser as small as possible,
# then the program might even look like:
#	"0"$"~"$"0"$"l"$">"$
# It is likely that the parser becomes a bit more relaxed,
# to have a more clean bootstrap.  Then it might look like:
#	"0_~_0_l_>_""v0"$
#
# Nothing is decided yet.  Except that the future parsers
# will be able to handle current programs.
# As this is only a matter of simple translation.
#
# The "v0"$ above means "run parser version 0"
# which is the bootstrapping parser (which does not exist today).
# The current parser will be referred to as "v1".

