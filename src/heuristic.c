/**
 *
 *@file heuristic.c
 *
 * Heuristics code for Arachne method
 *
 */

#include <float.h>

#include "binding.h"
#include "system.h"
#include "specialterm.h"
#include "switches.h"
#include "hidelevel.h"

//! Check whether a binding (goal) is selectable
int
is_goal_selectable (const Binding b)
{
  if (b != NULL)
    {
      if (!b->blocked && !b->done)
	{
	  return 1;
	}
    }
  return 0;
}

//! Count selectable goals
int
count_selectable_goals (const System sys)
{
  List bl;
  int n;

  n = 0;
  bl = sys->bindings;
  while (bl != NULL)
    {
      Binding b;

      b = (Binding) bl->data;
      if (is_goal_selectable (b))
	{
	  n++;
	}
      bl = bl->next;
    }
  return n;
}

//! Return first selectable goal in the list
/**
 * The return list entry is either NULL, or a selectable goal.
 */
List
first_selectable_goal (List bl)
{
  while (bl != NULL && !is_goal_selectable ((Binding) bl->data))
    {
      bl = bl->next;
    }
  return bl;
}

//! Give an indication of the amount of consequences binding a term has
/**
 * Given a term, returns a float. 0: maximum consequences, 1: no consequences.
 */
float
termBindConsequences (const System sys, Term t)
{
  Termlist openVariables;

  openVariables = termlistAddVariables (NULL, t);
  if (openVariables == NULL)
    {
      // No variables, no consequences
      return 1;
    }
  else
    {
      // For each run event in the semitrace, check whether it contains any
      // of the open variables.
      int totalCount;
      int affectedCount;
      int run;

      totalCount = 0;
      affectedCount = 0;
      run = 0;
      while (run < sys->maxruns)
	{
	  Roledef rd;
	  int step;

	  rd = sys->runs[run].start;
	  step = 0;
	  while (step < sys->runs[run].height)
	    {
	      Termlist tl;

	      tl = openVariables;
	      while (tl != NULL)
		{
		  if ((rd->type == READ || rd->type == SEND)
		      && termSubTerm (rd->message, tl->term))
		    {
		      // This run event contains the open variable
		      affectedCount++;
		      tl = NULL;
		    }
		  else
		    {
		      tl = tl->next;
		    }
		}
	      totalCount++;
	      step++;
	      rd = rd->next;
	    }
	  run++;
	}

      termlistDelete (openVariables);
      if (totalCount > 0)
	{
	  // Valid computation
	  return (float) (totalCount - affectedCount) / totalCount;
	}
      else
	{
	  // No consequences, ensure no division by 0
	  return 1;
	}
    }
}

//! Determine whether a term is an open nonce variable
/**
 * Does not explore subterms
 */
int
isOpenNonceVar (Term t)
{
  t = deVar (t);
  if (realTermVariable (t))
    {
      return inTermlist (t->stype, TERM_Nonce);
    }
  else
    {
      return 0;
    }
}

//! Count unique open variables in term
/**
 */
int
count_open_variables (const Term t)
{
  Termlist tl;
  int n;

  tl = NULL;
  termlistAddVariables (tl, t);
  n = 0;
  while (tl != NULL)
    {
      if (!inTermlist (tl->next, t))
	{
	  if (isOpenNonceVar (t))
	    {
	      n = n + 1;
	    }
	}
      tl = tl->next;
    }
  termlistDelete (tl);
  return n;
}



//! Athena-like factor
/**
 * Lower is better (more nonce variables)
 */
float
term_noncevariables_level (const Term t)
{
  int onv;
  const int enough = 2;

  onv = count_open_variables (t);
  if (onv >= enough)
    {
      return 0;
    }
  else
    {
      return 1 - (onv / enough);
    }
}

//! Determine weight based on hidelevel
float
weighHidelevel (const System sys, const Term t, const float massknow,
		const float massprot)
{
  unsigned int hl;

  switch (hidelevelFlag (sys, t))
    {
    case HLFLAG_NONE:
      return 0;
    case HLFLAG_KNOW:
      return massknow;
    case HLFLAG_PROT:
      return massprot;
    }
  return 1;
}

//! newkeylevel (weighted)
int
newkeylevel (const int level)
{
  // keylevel is from { -1,0,1 } where -1 means delay
  if (level == 1)
    return 0;
  else
    return 1;
}

//! Determine the weight of a given goal
/**
 * 0 to ... (lower is better)
 *
 * --heuristic has two distint interpretations. If it is 0 or greater, it a
 * selection mask. If it is smaller than 0, it is some special tactic.
 *
 * selection masks for --select-goal
 *	1:	constrain level of term
 * 	2:	key or not
 * 	4:	consequences determination
 * 	8:	select also single variables (that are not role variables)
 * 	16:	single variables are better
 * 	32:	incorporate keylevel information
 *
 * special tactics for --select-goal
 *	-1:	random goal selection
 *
 */
float
computeGoalWeight (const System sys, const Binding b)
{
  float w;
  int smode;
  Term t;

  void erode (const float deltaw)
  {
    if (smode & 1)
      {
	w = w + deltaw;
      }
    smode = smode / 2;
  }

  // Total weight
  w = 0;
  // We will shift this mode variable
  smode = switches.heuristic;
  t = b->term;

  // Determine buf_constrain levels
  // Bit 0: 1 use hidelevel
  erode (2 * weighHidelevel (sys, t, 0.5, 0.5));
  // Bit 1: 2 key level (inverted)
  erode (0.5 * (1 - b->level));
  // Bit 2: 4 constrain level
  erode (term_constrain_level (t));
  // Bit 3: 8 nonce variables level (Cf. what I think is in Athena)
  erode (term_noncevariables_level (t));

  // Define legal range
  if (smode > 0)
    error ("--heuristic mode %i is illegal", switches.heuristic);

  // Return total weight
  return w;
}

//! Goal selection
/**
 * Selects the most constrained goal.
 *
 * Because the list starts with the newest terms, and we use <= (as opposed to <), we
 * ensure that for goals with equal constraint levels, we select the oldest one.
 *
 */
Binding
select_goal_masked (const System sys)
{
  List bl;
  Binding best;
  float best_weight;

  // Find the most constrained goal
  if (switches.output == PROOF)
    {
      indentPrint ();
      eprintf ("Listing open goals that might be chosen: ");
    }
  best_weight = FLT_MAX;
  best = NULL;
  bl = sys->bindings;
  while (bl != NULL)
    {
      Binding b;

      b = (Binding) bl->data;

      // Only if not done and not blocked
      if (is_goal_selectable (b))
	{
	  if (!isTermVariable (b->term))
	    {
	      float w;

	      w = computeGoalWeight (sys, b);

	      // Spacing between output
	      if (switches.output == PROOF && best != NULL)
		eprintf (", ");

	      // Better alternative?
	      if (w <= best_weight)
		{
		  best_weight = w;
		  best = b;
		  if (switches.output == PROOF)
		    eprintf ("*");
		}
	      if (switches.output == PROOF)
		{
		  termPrint (b->term);
		  eprintf ("<%.2f>", w);
		}
	    }
	}
      bl = bl->next;
    }
  if (switches.output == PROOF)
    {
      if (best == NULL)
	eprintf ("none");
      eprintf ("\n");
    }
  return best;
}

//! Goal selection special case -1: random
/**
 * Simply picks an open goal randomly. Has to be careful to skip singular stuff etc.
 */
Binding
select_goal_random (const System sys)
{
  int n;

  n = count_selectable_goals (sys);
  if (n > 0)
    {
      int choice;
      List bl;

      // Choose a random goal between 0 and n
      choice = rand () % n;

      // Fetch it
      bl = sys->bindings;
      while (choice >= 0)
	{
	  bl = first_selectable_goal (bl);
	  if (bl == NULL)
	    {
	      error ("Random chooser selected a NULL goal.");
	    }
	  choice--;
	}
      return (Binding) bl->data;
    }
  else
    {
      return (Binding) NULL;
    }
}

//! Goal selection function, generic
Binding
select_goal (const System sys)
{
  if (switches.heuristic >= 0)
    {
      // Masked
      return select_goal_masked (sys);
    }
  else
    {
      // Special cases
      switch (switches.heuristic)
	{
	case -1:
	  return select_goal_random (sys);
	}
      error ("Unknown value (<0) for --goal-select.");
    }
  return (Binding) NULL;
}
