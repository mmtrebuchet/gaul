/**********************************************************************
  struggle3.c
 **********************************************************************

  struggle3 - Test/example program for GAUL.
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

		This program demonstrates the use of custom GA
		termination criteria.  It also provides an example of
		how to collate some simple statistics.

		This program aims to generate the final sentence from
		Chapter 3 of Darwin's "The Origin of Species",
		entitled "Struggle for Existence".

		This example is explained in docs/html/tutorial3.html

 **********************************************************************/

/*
 * Includes
 */
#include "gaul.h"

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
  struggle_adaptation()
  synopsis:	Adapt solution by making a single hill-climbing step
		for a randomly selected allele.
  parameters:
  return:
  updated:	07/07/01
 **********************************************************************/

entity *struggle_adaptation(population *pop, entity *child)
  {
  entity	*adult;		/* Adapted solution. */
  int		allele;		/* Randomly selected allele. */

/*
 * We must generate a new solution by copying the original solution.
 * This function copys all genomic, and if appropriate, phenomic data.
 * It is never safe to adapt the solution in place.
 */
  adult = ga_entity_clone(pop, child);

/* Make point mutation. */
  allele = random_int(strlen(target_text));
  ((char *)adult->chromosome[0])[allele]++;
  struggle_score(pop, adult);

  if (adult->fitness > child->fitness)
    return adult;

/* Searching in that previous direction didn't help. */
  ((char *)adult->chromosome[0])[allele]-=2;
  struggle_score(pop, adult);

  if (adult->fitness > child->fitness)
    return adult;

/* We must already be at a maxima. */
  ((char *)adult->chromosome[0])[allele]++;
  adult->fitness = child->fitness;

  return adult;
  }


/**********************************************************************
  struggle_generation_hook()
  synopsis:	This function is called by the main GA routine at the
		beginning of every generation.
  parameters:
  return:
  updated:	07/07/01
 **********************************************************************/

boolean struggle_generation_hook(int generation, population *pop)
  {
  entity	*adult;		/* Adapted solution. */
  int		allele;		/* Randomly selected allele. */

/*
 * Display on-line statistics every 20th generation.
 */

/*
 * Stop if we have the exact solution.
 */

/*
 * Stop if the population has converged to a narrow range of solutions.
 */

  return TRUE;	/* TRUE indicates that evolution should continue. */
  }


/**********************************************************************
  main()
  synopsis:	Erm?
  parameters:
  return:
  updated:	07/07/01
 **********************************************************************/

int main(int argc, char **argv)
  {
  population	*pop=NULL;	/* Population structure. */

  random_init();
  random_seed(42);

  pop = ga_genesis_char(
     80,			/* const int              population_size */
     1,				/* const int              num_chromo */
     strlen(target_text),	/* const int              len_chromo */
     NULL,		 	/* GAgeneration_hook      generation_hook */
     NULL,			/* GAiteration_hook       iteration_hook */
     NULL,			/* GAdata_destructor      data_destructor */
     NULL,			/* GAdata_ref_incrementor data_ref_incrementor */
     struggle_score,		/* GAevaluate             evaluate */
     ga_seed_printable_random,	/* GAseed                 seed */
     struggle_adaptation,	/* GAadapt                adapt */
     ga_select_one_roulette,	/* GAselect_one           select_one */
     ga_select_two_roulette,	/* GAselect_two           select_two */
     ga_mutate_printable_singlepoint_drift,	/* GAmutate    mutate */
     ga_crossover_char_allele_mixing,	/* GAcrossover         crossover */
     NULL			/* GAreplace replace */
            );

  ga_population_set_parameters(
     pop,		/* population   *pop */
     0.9,		/* double       crossover */
     0.1,		/* double       mutation */
     0.0		/* double       migration */
                            );

  if ( ga_evolution( pop, GA_CLASS_LAMARCK, GA_ELITISM_PARENTS_SURVIVE,	1000 ) )
    {
    printf("The evolution was stopped because the termination criteria were met.\n" );
    }
  else
    {
    printf("The evolution was stopped because the maximum number of generations were performed.\n" );
    }

  printf( "The final solution with score %f was:\n",
          ga_get_entity_from_rank(pop,0)->fitness );
  printf( "%s\n",
          ga_chromosome_char_to_staticstring(pop, ga_get_entity_from_rank(pop,0)) );

  ga_extinction(pop);

  exit(2);
  }

