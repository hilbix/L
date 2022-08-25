#!/bin/bash

rc()
{
  local e=$?
  [ "$e" = "$1" ] && return
  printf '\nExpected %s but got %s: to see error rerun as\n\tbash -x %q\n' "$1" "$e" "$0"
}

# Check coding for forgotten NULLs on FORMAT and Loops
egrep -w 'FORMAT|Loops|L_stderr|L_warn|Lunknown' l.c | fgrep -v ', NULL);' | fgrep -v ', ...)'
rc 1
egrep '_format' l.c | fgrep -v ', NULL)' | fgrep -v ', ...)'
rc 1
