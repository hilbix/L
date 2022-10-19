#!/bin/bash

#set -x
set -o pipefail

STAT='"stat"$^[0a]^'

Lc()
{
  timeout 10 ./L -c "$1$STAT" "${@:2}"
}

rc()
{
  local e=$?
  [ "$e" = "$1" ] && return
  printf '\nExpected %s but got %s\n' "$1" "$e"
  exit 1
}

# Check coding for forgotten NULLs on FORMAT and Loops
egrep -w 'FORMAT|Loops|L_stderr|L_warn|Lunknown' L.c | fgrep -v ', NULL);' | fgrep -v ', ...)'
rc 1
egrep '_format' L.c | fgrep -v ', NULL)' | fgrep -v ', ...)'
rc 1

make
rc 0

Lc '(>@" ">)' arg1 arg2 | cmp - <(echo -n 'arg1 arg2 ')
rc 0

Lc '"r"$"I"$(8192<xX>X)' "$0" | cmp - "$0"
rc 0

Lc '"r"$"I"$(1<xX>X)' "$0" | cmp - "$0"
rc 0

Lc '01->[0a]>' | cmp - <(echo -1)
rc 0

Lc '1a ("int"$b BA=x A> X!{"="} X1+!{">"} X1-!{"<"} >B>[0a]> @)' 0 1 2 | cmp - <(echo $'1>0\n1=1\n1<2')
rc 0

echo Tests OK

