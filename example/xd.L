#!/usr/bin/env L
#
# A small hexdumper.
# without arg it reads from stdin
# With args it reads the files

(@
  {f			# remember file
    "File "F[0a]++>	# output what we dump
    F"r"$"I"$		# Open input
  }
  16w			# Width
  0p			# Pos
  (
    8192<b		# read in chunk (should be multiple of width)
    BPW"xd"$		# create hexdump
    P+p			# advance position
    >			# output hexdump
  B)			# loop until EOF
@)

