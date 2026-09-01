/**************************************************************************
*  File: castle.c                                          Part of tbaMUD *
*  Usage: Retired King's Castle special-procedure compatibility stub.     *
*                                                                         *
*  All rights reserved. See license for complete information.             *
*                                                                         *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "spec_procs.h"

/*
 * King's Castle legacy C behavior has been retired.
 *
 * Area-specific NPC behavior should be implemented with DG Scripts rather
 * than hardcoded SPECIAL() procedures. This function remains as a no-op
 * compatibility symbol so older declarations or build files that still
 * reference assign_kings_castle() do not cause linker failures.
 */
void assign_kings_castle(void)
{
  /* Intentionally empty. */
}
