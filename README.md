> This is currently terribly incomplete.
>
> For example most functions (like "r" noted below) are not yet implemented.
>
> However, some of the basic [`example`](example/)s already work.
>
> Also see issues.


[![L Build Status](https://api.cirrus-ci.com/github/hilbix/L.svg?branch=master)](https://cirrus-ci.com/github/hilbix/L/master)


# L

Embedded Language (hence the letter `L` pronounced `el` like Embedded Language)

This is a minimalistic embedded language to process data streams (read: filter stdin to stdout).

Main Design Goals:

- Easy to use
- Easy to embed
- Easy to extend
- Easy to change

Additional Design Goals:

- KISS
- Keep together what belongs together
  - This especially means braces in C
  - They are either on the same line
  - Or on the same column
- Perfectism reached when you can no more remove anything

Unimportant Design Goals:

- Maximum performance
- Small scripts
- Expressive scripts

Design Goals Antipatterns (will never be implemented in L):

- Code Polishing
- Shiny Code
- Bloat
- Cathedral
- Bazaar
- Tabstops other than 8
- Imposed Perfectism
- Code of Conduct
- Political Correctness
- Getting Commercial 
- Patents
- Copyright
- Nazis


## Usage

	git clone https://github.com/hilbix/L.git
	cd L
	make

You can then run things:

	./L file.l args.. <input >output
	./L -c 'script' args.. <input >output

Like "hello world":

	./L -c '"hello world">'

Or a simple replacement of `/bin/cat`:

	./L -c '(@ {"r"$x} (8192<xX>X) @)' -- files..

What does this do?

- The program starts with all arguments on stack (first is TOS)
- `(` until `@)` loops as long as there are arguments on the stack
  - If there are no arguments
- `@` pushes the stack size (which here is the number of arguments) to the stack
- `{` starts a conditional, which only runs if `@` did not return 0
- `"r"$` calls function `r` with the argument
  - This function opens the given file (top of stack) as input.
  - This assumes you use the standard library, where "r" is mapped to `fopen(name, "r")`
  - This returns a file descriptor you can reuse later
- `x` pops the returned file descriptor from stack
  - we do not need this
  - (as the file is still connected to input, it is not closed)
- `}` ends the conditional
- `(` to `)` then loops over the input
- `8192<` reads in up to 8192 bytes
  - It returns an empty buffer on EOF or error
- `x` stores the buffer into variable X
- `X>` outputs the buffer
- `X` puts the buffer on stack, such that the loop continues if there was data

More Examples see folder [`example/`](example/).


## Embedding

> Embedding is in it's early stages

The main goal for L is to be embeddable.
In fact, it was constructed for exactly that purpose:

- As a core for some closed source proprietary tools.

However, things must first evolve here.

For now just have a look into [`L.c`](L.c) how to wrap and include it into your own code.


## Options

- `-c` runs a script given on commandline
  - The first non-option becomes the script
  - The rest become the args to the script
  - This is the same behavior as in shells
  - If given multiply this does not change anything

- `-f file` reads a file
  - The file must be given as the next argument
  - This can be given multiply
  - The programs then run interleavingly
  - Running more than one program currently has bugs, see TODO section below!

- `--` ends the options
  - Not yet implemented


## Program

The program consists of (mostly) single characters with a special meaning.
And everything works with a stack.

- Initially the stack carries all the arguments from the commandline, such that the TOS (Top Of Stack) carries the first argument.
- Input is connected to STDIN, and Output is connected to STDOUT.
- STDERR and other fildes are not connected anywhere.

- `#` starts a comment until the end of line
  - This way you can use shebang in scripts etc.

- Spaces and other control characters are NOPs
  - NOPs do just nothing (are completely ignored)
  - note that `SPC` can be an alias of `+` in CGI Web environments

- `@` pushes the number of elements on the stack

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

- `(_)` loops over `_`
  - The first loop always is taken
  - At the end of the loop the TOS is examined
  - If it is not nullish, the loop is run again
  - The TOS is popped if the stack is nonempty

- `{_}` is the conditional:
  - The TOS is examined to be not nullish
  - The TOS is popped if the stack is nonempty
  - `if (X) { _ }` (in C or Java) can be written like `X{_}`
  - `if (X) { _ } else { _ }` (in C or Java) can be written like `XzZ{_}Z!{_}`
    (where `z` is a helper variable to cache the conditional)

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
  - The stack is empty, or
  - If the TOS is a number, it is exactly `0`, or
  - If the TOS is a buffer, it is empty
  - Other data types currently fail when testing for nullish


## Functions

> Functions are mostly not available yet

Following functions are predefined (but this can be changed!):

- `stat` pushes some informational counters to stack
  - This might become a structure in future
  - `stats` outputs those informational counters to STDERR
  - So `"stats"$` is similar to `"stat"$>`, except that it prints to STDERR not to STDOUT
  - `stats` might vanish as soon output to other FDs than 1 (STDOUT) is possible.

- `r` open input from a file (according to `fopen(name, "r")`
  - If argument is a string, this is the filename to open
  - If argument is a number, this is the fildes to open
  - If argument is a buffer, the buffer is read as input


# TODO

Things which are not yet implemented correctly or where the design must be changed.

Memory:

- Currently memory is not freed correctly
  - Hence `L` eats up all memory very fast

Stack:

- We only have one global stack
- However we can run multiple programs in parallel
- As all use the stack this is completly wrong
- Hence currently option `-f` is unusable if used more than once
- See also: Execution Environment

Execution Environment:

- There is currently only one scope for execution.  This is wrong.
  - We need multiple environments, such that we can sandbox things 
- This mainly means which `$` functions are available etc.
  - However also programs should have their own stack

Code:

- Many code parts are still implemented in C
  - In future, these should mainly be done in L

Main:

- Currently the main loop is blocking
- In future it should become event driven
- For example the read command blocks.


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

