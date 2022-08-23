> This is not ready yet

[![L Build Status](https://api.cirrus-ci.com/github/hilbix/L.svg?branch=master)](https://cirrus-ci.com/github/hilbix/L/master)

# L

Embedded Language (hence the letter `L` pronounced `el` like Embedded Language)

This is a minimalistic embedded language to process data streams.

It is extensible and easy to use.


## Usage

	git clone https://github.com/hilbix/l.git
	cd l
	make

You can then run things:

	./l file.l args.. <input >output

Like a simple replacement of `/bin/cat`:

	./l <(echo '(_"r"${8192<xX>X}_)') files.. 

What does this do?

- The program starts with all arguments on stack (first is TOS)
- `(_` until `_)` loops over the arguments
- `A"r"$` calls function `r` with the argument, thus opening the file for read.
  - This assumes you use the standard library, where "r" is mapped to `fopen(name, "r")`
  - This returns a file descriptor you can reuse later
- `{` to `}` then loops over this file
- `8192<` reads in up to 8192 bytes
  - It returns an empty buffer on EOF or error
- `x` stores the buffer into variable X
- `X>` outputs the buffer
- `X` puts the buffer on stack, such that the loop continues if there was data

One thing is missing here:

- With no arguments we want to copy STDIN to STDOUT
- Here function `r` fails as this expects something on the stack
- The way to go is to not to call `r` if there is no argument on stack

The change is easy, just call `r` only if there is something on the stack:

	./l <(echo '(_"n"${"r"$}{8192<xX>X}_)')

The standard function `n` pushes the number of elements on the stack.

## Program

The program consists of (mostly) single characters with a special meaning.
And everything works with a stack.

- Initially the stack carries all the arguments from the commandline, such that the TOS (Top Of Stack) carries the first argument.
- Input is connected to STDIN, and Output is connected to STDOUT.
- STDERR and other fildes are not connected anywhere.

- `_` NOP, this command does nothing
  - likewise any other control character
  - note that this is the default
  - for example, `SPC` can be an allias of `+` in CGI Web environments

- `a` to `z` pop TOS and store into variable
- `A` to `Z` push variable onto TOS

- `<` pop TOS and read the given number of bytes from STDIN into TOS as buffer
  - If number is `0` it blocks until input is available and pushes `0` on EOF or error, else the number of data available (usually 1)
  - On EOF an empty buffer is returned
  - If number is negative, it read nonblockingly, EOF cannot be signalled here
  - If TOS is buffer XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

- '>' pops TOS and writes it to STDOUT
  - If TOS is a number the number is written to STDOUT
  - If TOS is a buffer the buffer is written to STDOUT

- `(_)` loops over `_` as long as the TOS is not nullish
  - The TOS is examined at the end of the loop so the first loop always is taken
  - The TOS is only examined at the end of the loop, it is not popped!

- `{_}` is the same as `(_)` but:
  - The TOS is examined at the beginning of the loop
  - The TOS is popped
  - `if (X) { _ }` is implemented with `X{_0}`
  - `if (X) { _ } else { _ }` is implemented with `XzZZ{_0}!{_0}` (where `z` is a helper variable to do the "dup" on stack)

- `+` adds the two top elements on the stack
  - `5_3+` gives `8`
  - adding two buffers concatenates them
  - adding a number to a buffer increments all buffer bytes (with rollover)
  - adding a buffer to a number adds all bytes in the buffer to the number
  - `0"a"+` gives the value 97 (which is the ASCII value)
  - `0"aa"+` gives the value 194 (which is the ASCII value)

- `-` substracts the two top elements on the stack
  - `5_3-` gives `2`
  - substracting a number from a buffer XXXXXXXXXXXXXXXXXXXXXXXXXXX
  - substracting a buffer from a number XXXXXXXXXXXXXXXXXXXXXXXXXXX
  - substracting two buffers XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  - `"aabbccddeeff"5:`
  - `5"aabbccddeeff":`
  - `"aabbccddeeff""c"-`

- `.` multiples the two top elements of the stack
  - `5_3.` gives `15`
  - Multiplying a buffer with a number repeats the buffer the given number of times
  - Multiplying a number with a buffer XXXXXXXXXXXXXXXXXXXXXXXXXXXX
  - Multiplying two buffers returns the position of the buffer at TOS in the other buffer. `0` not found, else `1` and above
  - `"a"5.` gives `"aaaaa"`
  - `5"aabbccddeeff".`
  - `"aabbccddeeff""c".`

- `:` divides the two top elements of the stack and pushes the result and remainder, so remainder is the TOS
  - `A_B:` gives  `C` and `D` always holds up following equation: `C * A + D = B`
  - if `A` is 0 then `C` is `0`, and `D` is `B`
  - else `D` is positive and less than the absolute value of `B` if everything is ok
  - else `D` is negative if something failed (like division by zero)
  - `5_3:` gives `1` and `2`
  - `0_x` gives `0` and `x`
  - `5_0` gives `1` and `-5`
  - Dividing a buffer with a number extracts the first (negative: last) number bytes of the buffer
  - Dividing a number with a buffer XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  - Dividing a buffer with a buffer counts the number of occurances of the buffer at TOS in the other buffer.
  - `"aabbccddeeff"5:` gives `"aabbc"`
  - `"aabbccddeeff"-5:` gives `"ffeed"`
  - `5"aabbccddeeff":`
  - `"aabbccddeeff""c":` gives `2`

> Was ich brauche:
> String:
>	Split buffer at first sequence
>	Index of sequence
>	Count occurances
> Get Left/Right of string

Notes:

- "nullish" means
  - The stack is not empty
  - If the TOS is a number, it must not be `0`
  - If the TOS is a buffer, it must be nonempty
  - For other data types this is data type dependent


## Functions

Following functions are predefined (but this can be changed!):

- `n` pushes the number of elements on the stack

- `r` open input from a file (according to `fopen(name, "r")`
  - If argument is a string, this is the filename to open
  - If argument is a number, this is the fildes to open
  - If argument is a buffer, the buffer is read as input


# TODO

Things which are not yet implemented correctly or where the design must be changed.

Execution Environment:

- There is currently only one scope for execution.  This is wrong.
  - We need multiple environments, such that we can sandbox things 
  - This mainly means which `$` functions are available etc.

Code:

- Many code parts are implemented in C
  - In future, these should mainly be done in L

Main:

- Currently the main loop is intrinsic
- In future it should be a fast state machine
- Such that you do not run your main loop from L

## FAQ

WTF why?

- Because I needed something like this for some contract work
- But I was unable to find something where programs neatly fit into URLs

License?

- This Works is placed under the terms of the Copyright Less License,  
  see file [COPYRIGHT.CLL](COPYRIGHT.CLL).  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
- Read: It is free as free beer, free speech and free baby.
- Note: Even that it is completely free, you are not allowed to cover any part of this code with a Copyright, as this would violate the German Urheberrecht (until at least 2092).

Bugs?  Questions?

- Open Issue on GitHub.
- Perhaps I listen.

Contrib?  Patches?

- Open PR on GitHub.
- Stick to the License, so remove your Copyright!
- Perhaps I listen.

