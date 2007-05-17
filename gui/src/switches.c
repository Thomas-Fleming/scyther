/**
 *@file switches.c
 * \brief Handle command-line options
 *
 * Contains the main switch handling.
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "system.h"
#include "debug.h"
#include "timer.h"
#include "switches.h"
#include "error.h"
#include "specialterm.h"

// Program name
const char *progname = "scyther";

#include "version.h"

// Structures
struct switchdata switches;
extern struct tacnode *spdltac;

// Global
char *lastfoundprefix = NULL;

// Forward declarations
void process_environment (void);
void process_switches (int commandline);

//! Init switches
/**
 * Set them all to the default settings.
 */
void
switchesInit (int argc, char **argv)
{
  // Methods
  switches.match = 0;		// default matching
  switches.tupling = 0;

  // Pruning and Bounding
  switches.prune = 2;		// default pruning method (use nice heuristic)
  switches.maxproofdepth = INT_MAX;
  switches.maxtracelength = INT_MAX;
  switches.runs = 5;		// default is 5 for usability, but -r 0 or --maxruns=0 will set it back to INT_MAX
  switches.filterProtocol = NULL;	// default check all claims
  switches.filterLabel = NULL;	// default check all claims
  switches.maxAttacks = 0;	// no maximum default
  switches.maxOfRole = 0;	// no maximum default

  // Arachne
  switches.heuristic = 162;	// default goal selection method
  switches.maxIntruderActions = INT_MAX;	// max number of encrypt/decrypt events
  switches.agentTypecheck = 1;	// default do check agent types
  switches.concrete = true;	// default removes symbols, and makes traces concrete
  switches.initUnique = false;	// default allows initiator rho to contain duplicate terms
  switches.respUnique = false;	// default allows responder rho to contain duplicate terms
  switches.intruder = true;	// default allows an intruder
  switches.agentUnfold = 0;	// default not to unfold agents
  switches.abstractionMethod = 0;	// default no abstraction used
  switches.useAttackBuffer = false;	// don't use by default as it does not work properly under windows vista yet

  // Misc
  switches.switchP = 0;		// multi-purpose parameter
  switches.experimental = 0;	// experimental stuff defaults to 0, whatever that means.
  switches.removeclaims = false;	// default: leave claims from spdl file
  switches.addreachableclaim = false;	// add 'reachable' claims
  switches.addallclaims = false;	// add all sorts of claims
  switches.check = false;	// check the protocol for termination etc. (default off)
  switches.expert = false;	// expert mode (off by default)

  // Output
  switches.output = SUMMARY;	// default is to show a summary
  switches.report = 0;
  switches.reportClaims = 0;	// default don't report on claims
  switches.xml = false;		// default no xml output 
  switches.dot = false;		// default no dot output
  switches.human = false;	// not human friendly by default
  switches.reportMemory = 0;
  switches.reportTime = 0;
  switches.countStates = false;	// default off
  switches.extendNonReads = 0;	// default off
  switches.extendTrivial = 0;	// default off
  switches.plain = false;	// default colors for terminal
  switches.monochrome = false;	// default colors for dot
  switches.lightness = 0;	// lightness correction
  switches.clusters = false;	// default is no clusters for now

  // Process the environment variable SCYTHERFLAGS
  process_environment ();
  // Process the command-line switches
  switches.argc = argc;
  switches.argv = argv;
  process_switches (true);
}

//! Exit
void
switchesDone (void)
{
  if (lastfoundprefix != NULL)
    free (lastfoundprefix);
}

//! Open a (protocol) file.
/**
 * Uses the environment variable SCYTHERDIR to also search for files
 *
 * If a file was opened before, this is stored in the static char* (initially
 * NULL) lastfoundprefix. This prefix can override any other: the point of it is that
 * if you find a file in a non-standard location, which then does an include,
 * you want this to refer to the directory where you found that last file.
 *
 * If reopener == NULL then we open a new file pointer, otherwise reopen this one.
 */
FILE *
openFileSearch (char *filename, FILE * reopener)
{
  const char *separators = ":;\n";
  char *dirs;

  //! try a filename and a prefix.
  /**
   * Prefixes don't have to end with "/"; this will be added automatically.
   */
  int try (char *prefix)
  {
    char *buffer = NULL;
    int result = false;
    int buflen = 0;
    int prefixlen = 0;
    int namelen = 0;
    int addslash = false;
    int nameindex = 0;

    prefixlen = (int) strcspn (prefix, separators);
    namelen = strlen (filename);
    nameindex = prefixlen;

    buflen = prefixlen + namelen + 1;

    // Does the prefix end with a slash? (it should)
    if (prefixlen > 0 && prefix[prefixlen - 1] != '/')
      {
	addslash = true;
	buflen++;
	nameindex++;
      }

    buffer = (char *) malloc (buflen);
    memcpy (buffer, prefix, prefixlen);
    memcpy (buffer + nameindex, filename, namelen);
    buffer[buflen - 1] = '\0';

    // Add the slash in the center
    if (addslash)
      buffer[nameindex - 1] = '/';

    // Now try to open it
    if (reopener != NULL)
      {
	if (freopen (buffer, "r", reopener) != NULL)
	  result = true;
      }
    else
      {
	reopener = fopen (buffer, "r");
	if (reopener != NULL)
	  result = true;
      }

    if (result)
      {
	// There is a result. Does it have a prefix?
	char *ls;

	// Non-standard location, maybe we should warn for that
	if (switches.expert)
	  {
	    globalError++;
	    eprintf ("Reading file %s.\n", buffer);
	    globalError--;
	  }

	// Compute the prefix (simply scan for the last slash, if any)
	ls = strrchr (buffer, '/');
	if (ls != NULL)
	  {
	    // Store it for any next includes or something like that
	    // Clear the old one
	    if (lastfoundprefix != NULL)
	      free (lastfoundprefix);
	    ls[0] = '\0';
	    lastfoundprefix = buffer;
	    return true;
	  }
      }
    free (buffer);
    return result;
  }

  // main code.

  // Try last file prefix (if it exists!)
  if (lastfoundprefix != NULL)
    {
      if (try (lastfoundprefix))
	return reopener;
    }

  // Try current directory

  if (try (""))
    return reopener;

  // Now try the environment variable
  dirs = getenv ("SCYTHERDIR");
  while (dirs != NULL)
    {
      if (strlen (dirs) > 0)
	{
	  // try this one
	  if (try (dirs))
	    return reopener;
	  // skip to next
	  dirs = strpbrk (dirs, separators);
	  if (dirs != NULL)
	    {
	      // skip over separator
	      dirs++;
	    }
	}
      else
	{
	  break;
	}
    }

  // Nope
  return NULL;
}

//! Open a (protocol) file instead of stdin
int
openFileStdin (char *filename)
{
  if (openFileSearch (filename, stdin) == NULL)
    {
      return false;
    }
  else
    {
      return true;
    }
}

//! Process a single switch or generate help text
/**
 * When process is false, we just generate the help text.
 *
 * Yields new index, or -1 when an error occurred.
 * When the new index > argc, it should not be called anymore.
 * By convention, argc = the number of arguments + 1
 * The index steps through 1..argc-1.
 */
int
switcher (const int process, int index, int commandline)
{
  char *this_arg;		// just a shortcut
  int this_arg_length;		// same here

  int argc;
  char **argv;

  char *arg_pointer;

  //! Check whether there are still n options left
  int enough_arguments_left (const int n, char shortopt, char *longopt)
  {
    if (index + n > argc)
      {
	error ("Option %c [%s] needs at least %i arguments.", shortopt,
	       longopt, n);
      }
    return 1;
  }

  // Skip over (processed) argument
  void arg_next (void)
  {
    index++;
    arg_pointer = argv[index];
  }

  //! Retrieve a (string) argument
  char *string_argument (void)
  {
    char *result;

    if (arg_pointer == NULL)
      {
	error ("Argument expected.");
      }
    result = arg_pointer;
    arg_next ();
    return result;
  }

  //! Parse an argument into an integer
  int integer_argument (void)
  {
    int result;

    if (arg_pointer == NULL)
      {
	error ("(Integer) argument expected.");
      }
    result = 0;
    if (sscanf (arg_pointer, "%i", &result) != 1)
      {
	error ("Could not parse expected integer argument.");
      }
    arg_next ();
    return result;
  }

  //! Detect whether this confirms to this option.
  /**
   * set arg_pointer and index
   */
  int detect (char shortopt, char *longopt, int args)
  {
    arg_pointer = NULL;

    if (!process)
      {
	// If we are not processing, we always yield true.
	return 1;
      }
    // Is it this option anyway?
    if (this_arg_length < 2 || this_arg[0] != '-')
      {
	// No option
	return 0;
      }
    // Compare
    if (this_arg[1] == '-')
      {
	int optlength;

	// This seems to be a long switch, so we handle it accordingly

	optlength = strlen (longopt);
	if (strncmp (this_arg + 2, longopt, optlength))
	  return 0;
	if (optlength + 2 < this_arg_length)
	  {
	    // This has an additional thing!
	    if (args > 0 && this_arg[2 + optlength] == '=')
	      {
		// It's the right thing
		if (optlength + 3 < this_arg_length)
		  {
		    arg_pointer = this_arg + 2 + optlength + 1;
		  }
		else
		  {
		    // arg = next
		    index++;
		    arg_pointer = argv[index];
		  }
	      }
	    else
	      {
		// It's not this option
		return 0;
	      }
	  }
	else
	  {
	    // arg = next
	    index++;
	    arg_pointer = argv[index];
	  }
      }
    else
      {
	// Short variant
	if (this_arg_length < 2 || this_arg[1] != shortopt)
	  return 0;
	if (args > 0 && this_arg_length > 2)
	  {
	    // This has an additional thing!
	    // We assume the argument follows immediately (no appended '=')
	    arg_pointer = this_arg + 2;
	  }
	else
	  {
	    // arg = next
	    index++;
	    arg_pointer = argv[index];
	  }
      }
    // Allright, this is the right option
    // Enough arguments left?
    return enough_arguments_left (args, shortopt, longopt);
  }

  //! Align columns
  void helptext (const char *left, const char *right)
  {
    printf ("  %-25s %s\n", left, right);
  }

  if (process)
    {
      argc = switches.argc;
      argv = switches.argv;
#ifdef DEBUG
      // Check range for debug; we trust the non-debug version :)
      if (index < 1 || index >= argc)
	{
	  error ("Bad index number %i for argc %i", index, argc);
	}
#endif
      this_arg = argv[index];
      this_arg_length = strlen (this_arg);
    }
  else
    {
      // Just doing help
      this_arg = NULL;
      this_arg_length = 0;
    }

  /*
   * -------------------------------------------------------------
   *    Process the options, one by one
   * -------------------------------------------------------------
   */

  /* ==================
   *  Generic options
   */
  if (detect ('d', "dot-output", 0))
    {
      if (!process)
	{
	  helptext ("-d, --dot-output", "show patterns in dot format");
	}
      else
	{
	  switches.output = ATTACK;
	  switches.dot = true;
	  return index;
	}
    }

  if (detect ('x', "xml-output", 0))
    {
      if (!process)
	{
	  helptext ("-x, --xml-output", "show patterns in XML format");
	}
      else
	{
	  switches.output = ATTACK;
	  switches.xml = true;
	  return index;
	}
    }

  if (detect (' ', "proof", 0))
    {
      if (!process)
	{
	  /*
	   * discourage: not very readable for non-experts yet
	   helptext ("    --proof", "show explicit proof");
	   */
	}
      else
	{
	  // Proof
	  switches.output = PROOF;
	  return index;
	}
    }

  if (detect (' ', "filter", 1))
    {
      if (!process)
	{
	  helptext ("--filter=<protocol>[,<label>]",
		    "check only certain claims");
	}
      else
	{
	  char *second;

	  switches.filterProtocol = string_argument ();
	  second = strchr (switches.filterProtocol, ',');
	  if (second != NULL)
	    {
	      // Cut off first part (turn ',' into '\0'; string is disposable) and proceed to next character.
	      second[0] = '\0';
	      second++;
	      switches.filterLabel = second;
	    }
	  return index;
	}
    }

  if (detect (' ', "remove-claims", 0))
    {
      if (!process)
	{
	  /* discourage:
	   *
	   * Causes all existing claims in the specification to be skipped.
	   */
	}
      else
	{
	  switches.removeclaims = true;
	  return index;
	}
    }

  if (detect ('c', "check", 0))
    {
      if (!process)
	{
	  helptext ("-c, --check",
		    "disable intruder and run statespace check. For correct protocols, end of roles should be reachable");
	}
      else
	{
	  switches.check = true;	// check (influences the number of runs after scanning the spdl file, setting it to the protocol role count)
	  switches.intruder = false;	// disable intruder
	  switches.removeclaims = true;	// remove parsed claims
	  switches.addreachableclaim = true;	// add reachability claims
	  return index;
	}
    }

  if (detect ('a', "auto-claims", 0))
    {
      if (!process)
	{
	  helptext ("-a, --auto-claims",
		    "ignore any existing claims and automatically generate claims");
	}
      else
	{
	  switches.removeclaims = true;
	  switches.addallclaims = true;
	  return index;
	}
    }

  if (detect ('C', "class", 0))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("-C, --class",
			"show full class (allow uninstantiated variables in pattern output)");
	    }
	}
      else
	{
	  switches.concrete = false;
	  return index;
	}
    }

  if (detect ('s', "state-space", 0))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("-s, --state-space",
			"ignore any existing claims and add 'reachable' claims. Gives complete characterization of a roles");
	    }
	}
      else
	{
	  switches.removeclaims = true;	// remove parsed claims
	  switches.addreachableclaim = true;	// add reachability claims
	  switches.prune = 0;	// do not prune anything
	  switches.concrete = false;	// show classes
	  return index;
	}
    }

  if (detect (' ', "concrete", 0))
    {
      if (!process)
	{
	  /* this is now the default */
	}
      else
	{
	  switches.concrete = true;
	  return index;
	}
    }

  /* ==================
   *  Bounding options
   */
  if (!process)
    {
      printf ("Switches that affect the state space:\n");
    }

  if (detect ('m', "match", 1))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("-m, --match=<int>",
			"type matching method [0] 0: No type-flaws allowed, 1: Allow basic type-flaws only, 2: Allow all type-flaws (not complete for this beta)");
	    }
	}
      else
	{
	  switches.match = integer_argument ();
	  return index;
	}
    }

  if (detect ('u', "untyped", 0))
    {
      if (!process)
	{
	  /* unadvisable, implicit m2 whilst we will need m1 */
	  /*
	     helptext ("-u, --untyped", "Consider all variables to be untyped");
	   */
	}
      else
	{
	  switches.match = 2;
	  return index;
	}
    }

  if (detect ('T', "timer", 1))
    {
      if (!process)
	{
	  /* Not shown in from help, as we don't want to encourage this
	     helptext ("-T, --timer=<int>", "maximum time in seconds [inf]");
	   */
	}
      else
	{
	  set_time_limit (integer_argument ());
	  return index;
	}
    }

  if (detect ('r', "max-runs", 1))
    {
      if (!process)
	{
	  helptext ("-r, --max-runs=<int>",
		    "maximum number of runs in patterns [5]");
	}
      else
	{
	  int arg = integer_argument ();
	  if (arg == 0)
	    {
	      switches.runs = INT_MAX;
	    }
	  else
	    {
	      switches.runs = arg;
	    }
	  return index;
	}
    }

  if (detect (' ', "unbounded", 0))
    {
      if (!process)
	{
	  helptext ("    --unbounded",
		    "Do not bound the number of runs in patterns");
	}
      else
	{
	  switches.runs = INT_MAX;
	  return index;
	}
    }

  if (detect ('l', "max-length", 1))
    {
      if (!process)
	{
	  /* not really needed if you prune runs
	     helptext ("-l, --max-length=<int>",
	     "prune traces longer than <int> events [inf]");
	   */
	}
      else
	{
	  switches.maxtracelength = integer_argument ();
	  return index;
	}
    }

  if (detect (' ', "scan-claims", 0))
    {
      if (!process)
	{
	  /* simply reduce allowed trace length to 0, cuts off any iterations immediately.
	   * useful to simply retrieve all claims.
	   */
	}
      else
	{
	  switches.maxtracelength = 0;
	  return index;
	}
    }

  if (detect ('A', "all-attacks", 0))
    {
      if (!process)
	{
	  helptext ("-A, --all-attacks",
		    "generate all attacks within the state space instead of just one");
	}
      else
	{
	  switches.prune = 0;
	  return index;
	}
    }

  if (detect (' ', "max-attacks", 1))
    {
      if (!process)
	{
	  /* not very important
	     helptext ("    --max-attacks=<int>", "when not 0, maximum number of attacks/states per claim [0]");
	   */
	}
      else
	{
	  /*
	   * This is the maximum number of *output* attacks per claim, except
	   * when it is 0 (unlimited).  Thus, for a choice N, the tool might
	   * still report 'at least X attacks' with X>N, but it will only
	   * output N attacks.
	   */
	  switches.maxAttacks = integer_argument ();
	  return index;
	}
    }

  if (detect (' ', "prune", 1))
    {
      if (!process)
	{
	  /* not very important
	     helptext ("    --prune=<int>", "pruning method when an attack is found [2]");

	     Semantics:
	     0 - Show all attacks (up to the maximum --max-attacks)
	     1 - Show first attack (only one)
	     2 - Show 'best' attack (use heuristics) (only one)

	     Thus, a value of '0' means multiple attacks are output, otherwise just one.
	   */
	}
      else
	{
	  switches.prune = integer_argument ();
	  return index;
	}
    }

  if (detect ('H', "human-readable", 0))
    {
      if (!process)
	{
	  /*
	   * Why?
	   *
	   helptext ("-H, --human-readable",
	   "try to make the output human-friendly (e.g. in XML).");
	   */
	}
      else
	{
	  switches.human = true;
	  switches.concrete = true;
	  return index;
	}
    }

  if (detect (' ', "ra-tupling", 0))
    {
      if (!process)
	{
	  /* disabled for now 
	     helptext ("    --ra-tupling", "compile using right-associative tupling");
	   */
	}
      else
	{
	  switches.tupling = 0;
	  return index;
	}
    }

  if (detect (' ', "la-tupling", 0))
    {
      if (!process)
	{
	  /* for experts only
	     helptext ("    --la-tupling", "compile using left-associative tupling");
	   */
	}
      else
	{
	  switches.tupling = 1;
	  return index;
	}
    }

  if (detect (' ', "tupling", 1))
    {
      if (!process)
	{
	  /* for experts only
	     helptext ("    --tupling", "tupling type to use");
	   */
	}
      else
	{
	  switches.tupling = integer_argument ();
	  return index;
	}
    }

  if (detect (' ', "abstraction-method", 1))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      /* Not working yet
	         helptext ("    --abstraction-method=<int>",
	         "Abstraction method used. Default: 0 (disabled)");
	       */
	    }
	}
      else
	{
	  switches.abstractionMethod = integer_argument ();
	  return index;
	}
    }


  /* ==================
   *  Modelchecker only
   */
  /*
     if (!process)
     {
     printf ("Switches for modelchecking engine:\n");
     }
   */

  /* obsolete, worked for modelchecker
   *
   if (detect (' ', "state-space", 0))
   {
   if (!process)
   {
   helptext ("--state-space",
   "output state space graph (in DOT format)");
   }
   else
   {
   switches.output = STATESPACE;
   return index;
   }
   }
   */

  /* ==================
   *  Arachne only
   */
  /*
     if (!process)
     {
     printf ("Switches for Arachne engine:\n");
     }
   */

  if (detect (' ', "heuristic", 1))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("    --heuristic=<int>", "use heuristic <int> [162]");
	    }
	}
      else
	{
	  switches.heuristic = integer_argument ();
	  return index;
	}
    }

  if (detect (' ', "agent-unfold", 1))
    {
      if (!process)
	{
	  /* discourage: hide
	   */
	}
      else
	{
	  switches.agentUnfold = integer_argument ();
	  return index;
	}
    }

  if (detect (' ', "extend-nonreads", 0))
    {
      if (!process)
	{
	  /* discourage: hide
	   */
	}
      else
	{
	  switches.extendNonReads = 1;
	  return index;
	}
    }

  if (detect (' ', "disable-intruder", 0))
    {
      if (!process)
	{
	  /* for testing purposes: hide
	   *
	   * Disables the intruder
	   */
	}
      else
	{
	  switches.intruder = false;
	  return index;
	}
    }

  if (detect (' ', "extravert", 0))
    {
      if (!process)
	{
	  /* discourage: hide
	   *
	   * Finds only attacks which exclude initiator Alice talking to Alice
	   */
	}
      else
	{
	  switches.initUnique = true;
	  switches.respUnique = true;
	  return index;
	}
    }

  if (detect (' ', "init-unique", 0))
    {
      if (!process)
	{
	  /* discourage: hide
	   *
	   * Finds only attacks which exclude initiator Alice talking to Alice
	   */
	}
      else
	{
	  switches.initUnique = true;
	  return index;
	}
    }

  if (detect (' ', "resp-unique", 0))
    {
      if (!process)
	{
	  /* discourage: hide
	   *
	   * Finds only attacks which exclude initiator Alice talking to Alice
	   */
	}
      else
	{
	  switches.respUnique = true;
	  return index;
	}
    }

  if (detect (' ', "extend-trivial", 0))
    {
      if (!process)
	{
	  /* discourage: hide
	   */
	}
      else
	{
	  switches.extendTrivial = 1;
	  return index;
	}
    }

  if (detect (' ', "monochrome", 0))
    {
      if (!process)
	{
	  /* discourage: hide
	   */
	}
      else
	{
	  switches.monochrome = true;
	  return index;
	}
    }

  if (detect (' ', "lightness", 1))
    {
      if (!process)
	{
	  /* discourage: hide
	   */
	}
      else
	{
	  switches.lightness = integer_argument ();
	  if ((switches.lightness < 0) || (switches.lightness > 100))
	    {
	      error
		("--lightness=x only accepts integer values between 0 and 100");
	    }
	  return index;
	}
    }

  if (detect (' ', "clusters", 0))
    {
      if (!process)
	{
	  /* discourage: hide
	   */
	}
      else
	{
	  switches.clusters = true;
	  return index;
	}
    }

  if (detect (' ', "intruder-actions", 1))
    {
      if (!process)
	{
	  /* fairly technical */
	}
      else
	{
	  switches.maxIntruderActions = integer_argument ();
	  return index;
	}
    }

  if (detect (' ', "disable-agenttypecheck", 0))
    {
      if (!process)
	{
	  /* maybe add after testing */
	}
      else
	{
	  switches.agentTypecheck = 0;
	  return index;
	}
    }

  /* ==================
   *  Experimental options
   *
   *  Only for experts
   */

  if (switches.expert)
    {
      if (detect (' ', "experimental", 1))
	{
	  if (!process)
	    {
	      /* unpredictable behaviour, can change throughout versions */
	    }
	  else
	    {
	      switches.experimental = integer_argument ();
	      return index;
	    }
	}
    }

  if (detect (' ', "max-of-role", 1))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("    --max-of-role=<int>",
			"maximum number of instances of each role [inf]");
	    }
	}
      else
	{
	  int arg = integer_argument ();
	  if (arg == 0)
	    {
	      switches.maxOfRole = 0;
	    }
	  else
	    {
	      switches.maxOfRole = arg;
	    }
	  return index;
	}
    }


  /* ==================
   *  Misc switches
   */

  if (!process)
    printf ("Misc. switches:\n");


  if (detect ('E', "expert", 0))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("-E, --expert", "Expert mode");
	    }
	}
      else
	{
	  switches.expert = true;
	  return index;
	}
    }

  if (detect (' ', "count-states", 0))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("    --count-states", "report on states (per claim)");
	    }
	}
      else
	{
	  switches.countStates = true;
	  return index;
	}
    }

  if (detect (' ', "echo", 0))
    {
      if (!process)
	{
	  /* not very important
	     helptext ("    --echo", "echo command line");
	   */
	}
      else
	{
	  /* print command line */
	  fprintf (stdout, "command\t");
	  commandlinePrint (stdout);
	  fprintf (stdout, "\n");
	  return index;
	}
    }

  if (detect ('e', "empty", 0))
    {
      if (!process)
	{
	  /* not very important
	     helptext ("-e,--empty", "do not generate output");
	   */
	}
      else
	{
	  switches.output = EMPTY;
	  return index;
	}
    }

  if (detect ('v', "version", 0))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("-v, --version", "version information");
	    }
	}
      else
	{
	  printf ("'%s' model checker for security protocols.\n", progname);
	  printf ("Version %s.\n", TAGVERSION);
	  if (switches.expert)
	    {
#ifdef DEBUG
	      printf ("Revision %s, compiled with debugging support.\n",
		      SVNVERSION);
#else
	      printf ("Revision %s\n", SVNVERSION);
#endif
	    }
	  printf ("Code by Cas Cremers\n");
	  exit (0);
	}
    }

  if (detect ('h', "help", 0))
    {
      if (!process)
	{
	  helptext ("-h, --help", "show short help");
	}
      else
	{
	  if (commandline)
	    {
	      printf ("Usage:\n");
	      printf ("  %s [switches] [FILE]\n\nSwitches:\n", progname);
	      switcher (0, 0, commandline);
	    }
	  exit (0);
	}
    }

  if (detect (' ', "long-help", 0))
    {
      if (!process)
	{
	  helptext ("    --long-help", "show long help");
	}
      else
	{
	  if (commandline)
	    {
	      printf ("Usage:\n");
	      printf ("  %s [switches] [FILE]\n\nSwitches:\n", progname);
	      switches.expert = true;
	      switcher (0, 0, commandline);
	    }
	  exit (0);
	}
    }

  if (detect (' ', "plain", 0))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("    --plain", "disable color terminal output");
	    }
	}
      else
	{
	  switches.plain = true;
	  return index;
	}
    }

#ifdef DEBUG
  if (detect ('D', "debug", 1))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("-D, --debug=<int>",
			"set debug (verbosity) level. [0]");
	    }
	}
      else
	{
	  debugSet (integer_argument ());
	  return index;
	}
    }
#endif

  if (detect ('o', "output", 1))
    {
      if (!process)
	{
	  helptext ("-o, --output=<FILE>", "output file [stdout]");
	}
      else
	{
	  // Set output file name
	  /* try to open */
	  if (!freopen (arg_pointer, "w", stdout))
	    {
	      printfstderr ("Could not create output file '%s'.\n",
			    arg_pointer);
	      exit (1);
	    }
	  arg_next ();
	  return index;
	}
    }

  if (detect (' ', "append-output", 1))
    {
      if (!process)
	{
	  helptext ("    --append-output=<FILE>",
		    "append output file [stdout]");
	}
      else
	{
	  // Set output file name
	  /* try to open */
	  if (!freopen (arg_pointer, "a", stdout))
	    {
	      printfstderr ("Could not create output file '%s'.\n",
			    arg_pointer);
	      exit (1);
	    }
	  arg_next ();
	  return index;
	}
    }

  if (detect (' ', "errors", 1))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("    --errors=<FILE>",
			"write errors to file [stderr]");
	    }
	}
      else
	{
	  // Set output file name
	  /* try to open */
	  if (!freopen (arg_pointer, "w", stderr))
	    {
	      printfstderr ("Could not create error file '%s'.\n",
			    arg_pointer);
	      exit (1);
	    }
	  arg_next ();
	  return index;
	}
    }

  if (detect (' ', "append-errors", 1))
    {
      if (!process)
	{
	  if (switches.expert)
	    {
	      helptext ("    --append-errors=<FILE>",
			"append errors to file [stderr]");
	    }
	}
      else
	{
	  // Set output file name
	  /* try to open */
	  if (!freopen (arg_pointer, "a", stderr))
	    {
	      printfstderr ("Could not create append error file '%s'.\n",
			    arg_pointer);
	      exit (1);
	    }
	  arg_next ();
	  return index;
	}
    }

  // If the option is not recognized, it means a file name.
  if (!process)
    {
      helptext ("FILE", "input file ('-' for stdin)");
    }
  else
    {
      if (!strcmp (this_arg, "-") && commandline)
	{
	  // '-' input: Leave input to stdin
	}
      else
	{
	  // not '-' input: change stdin to come from this file
	  if (!openFileStdin (this_arg))
	    {
	      // The file was not found. We have two options...
	      if (this_arg[0] == '-')
		{
		  printfstderr ("Unknown switch '%s'.\n", this_arg);
		}
	      else
		{
		  printfstderr ("Could not open input file '%s'.\n",
				this_arg);
		}
	      exit (1);
	    }
	  return index + 1;
	}
    }

  // Now show the environment variables
  if (!process)
    {
      printf
	("\nThere are two environment variables that influence the behaviour of Scyther.\n");
      printf
	("  SCYTHERFLAGS    Put any default command-line options here, syntax as on the command line.\n");
      printf
	("  SCYTHERDIR      Colon-separated path of directories to search for input files if a file\n");
      printf
	("                  is not found in the current directory. Note: use '$HOME' instead of '~'.\n");
    }

  return 0;
}

//! Process environment
void
process_environment (void)
{
  char *flags;

  flags = getenv ("SCYTHERFLAGS");
  if (flags != NULL)
    {
      int slen;

      slen = strlen (flags);
      if (slen > 0)
	{
	  /**
	   * We scan the flags here, but assume a stupid upper limit of 100 pieces, otherwise this all becomes fairly vague.
	   */
	  int max = 100;
	  char *argv[100];
	  int count;
	  char *args;
	  char *scanflag;
	  char *argn;

	  /* make a safe copy */
	  args = (char *) malloc (slen + 1);
	  memcpy (args, flags, slen + 1);

	  /* warning */
	  /*
	     globalError++;
	     eprintf ("warning: using environment variable SVNSCYTHER ('%s')\n",
	     args);
	     globalError--;
	   */

	  {
	    int i;

	    i = 0;
	    while (i < max)
	      {
		argv[i] = "";
		i++;
	      }
	  }

	  scanflag = args;
	  count = 0;
	  /* ugly use of assignment in condition */
	  while (count < max)
	    {
	      argn = strtok (scanflag, "\t ");
	      scanflag = NULL;
	      if (argn != NULL)
		{
		  count++;
		  argv[count] = argn;
		}
	      else
		{
		  break;
		}
	    }
	  /*
	     warning ("found %i arguments in SCYTHERFLAGS\n", count);
	   */

	  switches.argc = count + 1;
	  switches.argv = argv;
	  process_switches (false);
	}
    }
}

//! Process switches
void
process_switches (int commandline)
{
  int index;

  if (switches.argc == 1)
    {
      if (commandline)
	{
	  printf ("Try '%s --help' for more information, or visit:\n",
		  progname);
	  printf (" http://people.inf.ethz.ch/cremersc/scyther/index.html\n");
	  exit (0);
	}
      else
	{
	  return;
	}
    }

  index = 1;
  while (index < switches.argc && index > 0)
    {
      index = switcher (1, index, commandline);
    }
}