/**********************************************************************
  gaul.h
 **********************************************************************

  gaul - Genetic Algorithm Utility Library.
  Copyright �2000-2001, Stewart Adcock <stewart@bellatrix.pcl.ox.ac.uk>

  The latest version of this program should be available at:
  http://www.stewart-adcock.co.uk/

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.  Alternatively, if your project
  is incompatible with the GPL, I will probably agree to requests
  for permission to use the terms of any other license.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY WHATSOEVER.

  A full copy of the GNU General Public License should be in the file
  "COPYING" provided with this distribution; if not, see:
  http://www.gnu.org/

 **********************************************************************

  Synopsis:	Public header file for GAUL.

		This file should be included by any code that will
		be linking to libga.

		This file is a bit empty at the moment, but prototypes
		for all of the public functions will be moved into here
		at some point.

 **********************************************************************/

#ifndef GAUL_H_INCLUDED
#define GAUL_H_INCLUDED

/*
 * Includes
 */
#include "SAA_header.h"

/*
 * Forward declarations.
 */
typedef struct entity_t entity;
typedef struct population_t population;

/*
 * Enumerated types, used to define varients of the GA algorithms.
 */
typedef enum ga_genesis_type_t
  {
  GA_GENESIS_UNKNOWN = 0,
  GA_GENESIS_RANDOM, GA_GENESIS_PDB, GA_GENESIS_SOUP, GA_GENESIS_USER
  } ga_genesis_type;

typedef enum ga_class_type_t
  {
  GA_CLASS_UNKNOWN = 0,
  GA_CLASS_DARWIN,
  GA_CLASS_LAMARCK, GA_CLASS_LAMARCK_ALL,
  GA_CLASS_BALDWIN, GA_CLASS_BALDWIN_ALL
  } ga_class_type;

typedef enum ga_elitism_type_t
  {
  GA_ELITISM_UNKNOWN = 0,
  GA_ELITISM_PARENTS_SURVIVE, GA_ELITISM_ROUGH, GA_ELITISM_ROUGH_COMP,
  GA_ELITISM_EXACT, GA_ELITISM_EXACT_COMP,
  GA_ELITISM_PARENTS_DIE
  } ga_elitism_type;

/*
 * Include remainder of this library's headers.
 * These should, mostly, contain private definitions etc.
 * But they currently contain everything.
 */
#include "ga_core.h"

#if HAVE_SLANG==1
#include "ga_intrinsics.h"
#endif

#endif	/* GAUL_H_INCLUDED */
