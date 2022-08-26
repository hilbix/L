#!/usr/bin/env L
#
# Alternative implementation of simple replacement of /bin/echo (without options)

(		# loop over all arguments
  @{>}		# output argument
  @{' '>}	# if more arguments output SPC
@)
[0a]>		# output LF at the end

