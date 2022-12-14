/* This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 * Read: This is free software as in free beer, free speech and free baby.
 *
 * GCCFLAGS:	-Wno-unused-function
 *
 * vim:	ts=8
 *
 * I am a bit curious about
 * who the first idiot was to think that it perhaps is a
 * good idea to redefine TABs to some other default than 8:
 *	https://www.imdb.com/title/tt0387808/
 * And here a reference to the proper default, the only one I accept:
 *	https://www.ibm.com/docs/de/aix/7.1?topic=t-tabs-command
 *	https://en.wikipedia.org/wiki/Tab_key#Tab_characters
 * Please do not mix TAB with indentation.
 * I use indentation of 2 for over 35 years now
 * with braces which belong together being either on the same row or column
 * (else I am unable to spot which braces belong together,
 *  sorry, this is some of my human limitations I cannot overcome).
 * And I expect TAB to stay at column 8 for much longer!
 */

#include "L.h"

int
main(int argc, char **argv)
{
  L	_;

  Ldebug	= getenv("DEBUG");
  DP();

  /* Step 1:
   *
   * Initialize the main L structure
   *
   * In future you can have
   * - more than one such L instance
   * - apply your own memory management
   */
  _		= L_init(NULL, NULL);		/* initialize environment	*/
  L_register(_, Lfns, Lfns_, NULL);		/* register standard functions, _ functions and possibly yours	*/

  /* Here:
   * Implement loading etc. of program.
   * We do this with our standard argc/argv processing
   */
  Largcargv(_, argc, argv);

  L_loop(_);
  DP(" end");
  return Lexit(_);
}

