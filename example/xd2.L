#!/usr/bin/env L
#
# Another variant of hexdumper
#
# "xd"$ takes following arguments
# (TOS is above)
# str prefix (optional)
# str suffix (optional)
# str middle (optional)
# num width
# num pos
# num ascii (optional)
# buf source
#
# ascii is 1 by default
# if 0 then the ascii part is suppressed
# if bit 2 is set, all character are dumped, else only ASCII ones
# if bit 4 is set, SPC is used instead of . for replacement character
# if bit 8 is set, right identation is not done (good for suffix = [0a])
# perhaps more to come

(@
  '(stdin)'f		# preset stdout filename
  {f			# remember file
    F"r"$"I"$		# Open input
  }
  F': '+f		# put a ': ' after filename
  16w			# Width
  0p			# Pos
  (
    8192<b		# read in chunk (should be multiple of width)
    B14PW": "[0a]F"xd"$	# create hexdump a bit different to xd.L
    P+p			# advance position
    >			# output hexdump
  B)			# loop until EOF
@)

