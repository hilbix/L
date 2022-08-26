#!/usr/bin/env L
#
# This is a simple replacement of /bin/cat (without options)

(@			# push number of arguments on stack
  {			# if there is an argument
    "r"$		# open the file for read
    "I"$		# use opened file as input
  }			# the current stack value (argument) is popped here
  (			# now copy the data from input (file) to stdout
    8192<x		# read in a 8192 byte chunks into variable x
    X>			# output x to stdout
    X			# on EOF the buffer is empty, hence nullish
  )			# do the loop until EOF reached
@)			# loop over the other arguments until stack is empty

