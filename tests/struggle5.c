/**********************************************************************
  struggle5.c
 **********************************************************************

  struggle5 - Test/example program for GAUL.
  Copyright �2001, Stewart Adcock <stewart@bellatrix.pcl.ox.ac.uk>

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

  Synopsis:	Test/example program for GAUL.

		This example shows the use of multiple populations
		with GAUL's so called "archipelago" scheme.  This is
		the basic island model of evolution.

		This program aims to generate the final sentence from
		Chapter 3 of Darwin's "The Origin of Species",
		entitled "Struggle for Existence".

		This example is explained in docs/html/tutorial5.html

 **********************************************************************/

/*
 * Includes
 */
#include "gaul.h"

/* Specify the number of populations(islands) to use. */
#define GA_STRUGGLE_NUM_POPS	4

/*
 * The solution string.
 */
char *target_text="When we reflect on this struggle, we may console ourselves with the full belief, that the war of nature is not incessant, that no fear is felt, that death is generally prompt, and that the vigorous, the healthy, and the happy survive and multiply.";


/**********************************************************************
  struggle_score()
  synopsis:	Score solution.
  parameters:
  return:
  updated:	16/06/01
 **********************************************************************/

boolean struggle_score(population *pop, entity *entity)
  {
  int		k;		/* Loop variable over all alleles. */

  entity->fitness = 0.0;

  /* Loop over alleles in chromosome. */
  for (k = 0; k < pop->len_chromosomes; k++)
    {
    if ( ((char *)entity->chromosome[0])[k] == target_text[k])
      entity->fitness+=1.0;
    /*
     * Component to smooth function, which helps a lot in this case:
     * Comment it out if you like.
     */
    entity->fitness += (127.0-fabs(((char *)entity->chromosome[0])[k]-target_text[k]))/50.0;
    }

  return TRUE;
  }


/**********************************************************************
  main()
  synopsis:	Erm?
  parameters:
  return:
  updated:	20/06/01
 **********************************************************************/

int main(int argc, char **argv)
  {
  int		i;				/* Loop over populations. */
  population	*pop[GA_STRUGGLE_NUM_POPS];	/* Array of populations. */

  random_init();
  random_seed(i);

  for (i=0; i<GA_STRUGGLE_NUM_POPS; i++)
    {
    pop[i] = ga_genesis_char(
       100,			/* const int              population_size */
       1,			/* const int              num_chromo */
       strlen(target_text),	/* const int              len_chromo */
       NULL,		 	/* GAgeneration_hook      generation_hook */
       NULL,			/* GAiteration_hook       iteration_hook */
       NULL,			/* GAdata_destructor      data_destructor */
       NULL,			/* GAdata_ref_incrementor data_ref_incrementor */
       struggle_score,		/* GAevaluate             evaluate */
       ga_seed_printable_random,	/* GAseed                 seed */
       NULL,			/* GAadapt                adapt */
       ga_select_one_roulette,	/* GAselect_one           select_one */
       ga_select_two_roulette,	/* GAselect_two           select_two */
       ga_mutate_printable_singlepoint_drift,	/* GAmutate       mutate */
       ga_crossover_char_allele_mixing,	/* GAcrossover            crossover */
       NULL			/* GAreplace replace */
            );

    ga_population_set_parameters(
       pop[i],		/* population      *pop */
       1.0,		/* double  crossover */
       0.1,		/* double  mutation */
       0.01,            /* double  migration */
                              );
    }

  ga_evolution_archipelago( GA_STRUGGLE_NUM_POPS, pop,
       GA_CLASS_DARWIN,	GA_ELITISM_PARENTS_SURVIVE, 200 );

  for (i=0; i<GA_STRUGGLE_NUM_POPS; i++)
    {
    printf( "The best solution on island %d with score %f was:\n",
            i, ga_get_entity_from_rank(pop[i],0)->fitness );
    printf( "%s\n",
            ga_chromosome_char_to_staticstring(pop[i],
                               ga_get_entity_from_rank(pop[i],0)));

    ga_extinction(pop);
    }

  exit(2);
  }

