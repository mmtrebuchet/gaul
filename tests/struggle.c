/**********************************************************************
  struggle.c
 **********************************************************************

  struggle - Test/example program for GAUL.
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

		This program is fairly lean, showing how little
		application code is needed when using GAUL.

		This program aims to generate the final sentence from
		Chapter 3 of Darwin's "The Origin of Species",
		entitled "Struggle for Existence".

 **********************************************************************/

/*
 * Includes
 */
#include "SAA_header.h"

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
  main()
  synopsis:	Erm?
  parameters:
  return:
  updated:	16/06/01
 **********************************************************************/

int main(int argc, char **argv)
  {
  int		i;		/* Runs. */
  population	*pop=NULL;	/* Population of solutions. */

  random_init();

  for (i=0; i<50; i++)
    {
    if (pop) ga_extinction(pop);

    random_seed(i);

    pop = ga_genesis_char(
       250,			/* const int              population_size */
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
       ga_mutate_printable_singlepoint_drift,	/* GAmutate               mutate */
       ga_crossover_char_allele_mixing,	/* GAcrossover            crossover */
       NULL			/* GAreplace replace */
            );

    ga_population_set_parameters(
       pop,		/* population      *pop */
       1.0,		/* double  crossover */
       0.1,		/* double  mutation */
       0.0              /* double  migration */
                              );

    ga_evolution(
       pop,		/* population              *pop */
       GA_CLASS_DARWIN,	/* const ga_class_type     class */
       GA_ELITISM_PARENTS_SURVIVE,	/* const ga_elitism_type   elitism */
       500		/* const int               max_generations */
              );

    printf("The final solution with seed = %d was:\n", i);
    printf("%s\n", ga_chromosome_char_to_staticstring(pop, pop->entity_iarray[0]));
    printf("With score = %f", pop->entity_iarray[0]->fitness);
    printf("\n");
    }

  ga_extinction(pop);

  exit(2);
  }


