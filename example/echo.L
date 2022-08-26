#!/usr/bin/env L
#
# This is a simple replacement of /bin/echo (without options)

@{
 >	# output first argument
 @{(	# loop over remaining arguments
  ' '>	# output space
  >	# output argument
 @)}
}
[0a]>	# Output LF at the end

