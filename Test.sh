#!/bin/bash

rc()
{
  local e=$?
  [ "$e" = "$1" ] && return
  printf '\nExpected %s but got %s: to see error rerun as\n\tbash -x %q\n' "$1" "$e" "$0"
  exit 1
}

# Check coding for forgotten NULLs on FORMAT and Loops
egrep -w 'FORMAT|Loops|L_stderr|L_warn|Lunknown' l.c | fgrep -v ', NULL);' | fgrep -v ', ...)'
rc 1
egrep '_format' l.c | fgrep -v ', NULL)' | fgrep -v ', ...)'
rc 1

make
rc 0

timeout 10 ./L -c '(>@)' arg1 arg2
rc 0

timeout 10 ./L -c '"r"$"I"$(1<xX>X)' "$0" | cmp - "$0"
rc 0

