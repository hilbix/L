#!/usr/bin/env L
#
# same as echo2, only written more nicely (but less easy to understand)

(@		# loop over all arguments
  {>}		# output argument
  @{' '>}	# if more arguments output SPC
@)		# I like it how this matches (@
[0a]>		# output LF at the end

