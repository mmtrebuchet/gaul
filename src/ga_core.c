/**********************************************************************
  ga_core.c
 **********************************************************************

  ga_core - Genetic algorithm routines.
  Copyright �2000-2002, Stewart Adcock <stewart@linux-domain.com>

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

  Synopsis:     Routines for handling populations and performing GA
		operations.

		Also contains a number of helper functions providing
		alternative optimisation schemes for comparison and
		analysis purposes.

		BEWARE: NOT THREAD-SAFE!

		Internally, and in the public C interface, pointers
		are used to identify the population and entity
		structures.  However, in the scripting interface these
		pointers are unusable, so identifing integers are
		used instead.

		Note that the temperatures in the simulated annealling
		and MC functions do not exactly run from the initial
		temperature to the final temperature.  They are offset
		slightly so that sequential calls to these functions
		will have a linear temperature change.

  Vague usage details:	Set-up with ga_genesis().
			Perform calculations with ga_evolution().
			Grab data for post-analysis with ga_transcend().
			Evolution will continue if ga_evolution() is
			called again without calling ga_genesis() again.

  To do:	Reading/writing 'soup'.
		Replace the send_mask int array with a bit vector.
		Use Memory Chunks for efficient memory allocation?
		Instead of using a 'allocated' flag for each entity, keep a list of unused entities.  (Or use mem_chunks).
		Use table structure for maintainance of multiple populations.
		All functions here should be based on entity/population _pointers_ while the functions in ga_intrinsics should be based on _handles_.
		ga_population_new() should return the reference pointer not the handle.
		Consider using the table structure, coupled with memory chunks, for handling the entities within a population.  This should be as efficient as the current method, and will remove the need for a maximum population size.
		More "if (!pop) die("Null pointer to population structure passed.");" checks are needed.
		Use a list of structures containing info about user defined crossover and mutation functions.
		Genome distance measures (tanimoto, euclidean, tverski etc.)

		Population/entity iterator functions.

 **********************************************************************/

#include "ga_core.h"

/*
 * Global variables.
 */
THREAD_LOCK_DEFINE_STATIC(pop_table_lock);
TableStruct	*pop_table=NULL;	/* The population table. */

/**********************************************************************
  Private utility functions.
 **********************************************************************/

/**********************************************************************
  destruct_list()
  synopsis:	Destroys an userdata list and it's contents.  For
		many applications, the destructor callback will just
		be free() or similar.  This callback may safely be
		NULL.
  parameters:	population *pop	Population.
		SLList *list	Phenomic data.
  return:	none
  last updated:	20/12/00
 **********************************************************************/

static void destruct_list(population *pop, SLList *list)
  {
  SLList        *this;		/* Current list element */
  int		num_destroyed;	/* Count number of things destroyed. */
  vpointer	data;		/* Data in item. */

/* A bit of validation. */
  if ( !pop->data_destructor ) return;
  if ( !list ) die("Null pointer to list passed.");

/* Deallocate individual structures. */
  num_destroyed = 0;
  this=list;

  while(this!=NULL)
    {
    if ((data = slink_data(this)))
      {
      pop->data_destructor(data);
      num_destroyed++;
      }
    this=slink_next(this);
    }

/* Deallocate list sructure. */
  slink_free_all(list);

#if GA_DEBUG>2
  /*
   * Not typically needed now, because num_destrtoyed may
   * (correctly) differ from the actual number of chromosomes.
   */
  if (num_destroyed != pop->num_chromosomes)
    printf("Uh oh! Dodgy user data here? %d %d\n",
                 num_destroyed, pop->num_chromosomes);
#endif

  return;
  }


/**********************************************************************
  Population handling functions.
 **********************************************************************/

/**********************************************************************
  ga_population_new()
  synopsis:	Allocates and initialises a new population structure,
		and assigns a new population id to it.
  parameters:	const int stable_size		Num. individuals carried into next generation.
		const int num_chromosome	Num. of chromosomes.
		const int len_chromosome	Size of chromosomes (may be ignored).
  return:	population *	new population structure.
  last updated: 13 Feb 2002
 **********************************************************************/

population *ga_population_new(	const int stable_size,
				const int num_chromosome,
				const int len_chromosome)
  {
  int		i;		/* Loop variable. */
  population	*newpop=NULL;	/* New population structure. */
  unsigned int	pop_id;		/* Handle for new population structure. */

  if ( !(newpop = s_malloc(sizeof(population))) )
    die("Unable to allocate memory");

  newpop->size = 0;
  newpop->stable_size = stable_size;
  newpop->max_size = stable_size*3;
  newpop->orig_size = 0;
  newpop->num_chromosomes = num_chromosome;
  newpop->len_chromosomes = len_chromosome;
  newpop->data = NULL;

  newpop->crossover_ratio = 1.0;
  newpop->mutation_ratio = 1.0;
  newpop->migration_ratio = 1.0;

  if ( !(newpop->entity_array = s_malloc(newpop->max_size*sizeof(entity))) )
    die("Unable to allocate memory");

  if ( !(newpop->entity_iarray = s_malloc(newpop->max_size*sizeof(entity*))) )
    die("Unable to allocate memory");
  
  for (i=0; i<newpop->max_size; i++)
    {
    newpop->entity_array[i].allocated=FALSE;
    newpop->entity_array[i].data=NULL;
    newpop->entity_array[i].fitness=GA_MIN_FITNESS;	/* Lower bound on fitness */

    newpop->entity_array[i].chromosome=NULL;

/*    newpop->entity_iarray[i] = &(newpop->entity_array[i]);*/
    }

/*
 * Clean the callback functions.
 * Prevents eronerous callbacks - helpful when debugging!
 */
  newpop->generation_hook = NULL;
  newpop->iteration_hook = NULL;

  newpop->data_destructor = NULL;
  newpop->data_ref_incrementor = NULL;

  newpop->chromosome_constructor = NULL;
  newpop->chromosome_destructor = NULL;
  newpop->chromosome_replicate = NULL;
  newpop->chromosome_to_bytes = NULL;
  newpop->chromosome_from_bytes = NULL;
  newpop->chromosome_to_string = NULL;

  newpop->evaluate = NULL;
  newpop->seed = NULL;
  newpop->adapt = NULL;
  newpop->select_one = NULL;
  newpop->select_two = NULL;
  newpop->mutate = NULL;
  newpop->crossover = NULL;
  newpop->replace = NULL;

/*
 * Add this new population into the population table.
 */
  THREAD_LOCK(pop_table_lock);
  if (!pop_table) pop_table=table_new();

  pop_id = table_add(pop_table, (vpointer) newpop);
  THREAD_UNLOCK(pop_table_lock);

  plog( LOG_DEBUG, "New pop = %p id = %d", newpop, pop_id);

  return newpop;
  }


/**********************************************************************
  ga_population_clone()
  synopsis:	Allocates and initialises a new population structure,
		and fills it with an exact copy of the data from an
		existing population.  The population's user data
		field is referenced.
		Entity id's between the popultions will _NOT_
		correspond.
  parameters:	population *	original population structure.
  return:	population *	new population structure.
  last updated: 07/07/01
 **********************************************************************/

population *ga_population_clone(population *pop)
  {
  int		i;		/* Loop variable. */
  population	*newpop=NULL;	/* New population structure. */
  entity	*newentity;	/* Used for cloning entities. */
  unsigned int	pop_id;		/* Handle for new population structure. */

  newpop = s_malloc(sizeof(population));

/*
 * Clone parameters.
 */
  newpop->size = 0;
  newpop->stable_size = pop->stable_size;
  newpop->max_size = pop->max_size;
  newpop->orig_size = 0;
  newpop->num_chromosomes = pop->num_chromosomes;
  newpop->len_chromosomes = pop->len_chromosomes;
  newpop->data = pop->data;

  newpop->crossover_ratio = pop->crossover_ratio;
  newpop->mutation_ratio = pop->mutation_ratio;
  newpop->migration_ratio = pop->migration_ratio;

/*
 * Clone the callback functions.
 */
  newpop->generation_hook = pop->generation_hook;
  newpop->iteration_hook = pop->iteration_hook;

  newpop->data_destructor = pop->data_destructor;
  newpop->data_ref_incrementor = pop->data_ref_incrementor;

  newpop->chromosome_constructor = pop->chromosome_constructor;
  newpop->chromosome_destructor = pop->chromosome_destructor;
  newpop->chromosome_replicate = pop->chromosome_replicate;
  newpop->chromosome_to_bytes = pop->chromosome_to_bytes;
  newpop->chromosome_from_bytes = pop->chromosome_from_bytes;
  newpop->chromosome_to_string = pop->chromosome_to_string;

  newpop->evaluate = pop->evaluate;
  newpop->seed = pop->seed;
  newpop->adapt = pop->adapt;
  newpop->select_one = pop->select_one;
  newpop->select_two = pop->select_two;
  newpop->mutate = pop->mutate;
  newpop->crossover = pop->crossover;
  newpop->replace = pop->replace;

/*
 * Clone each of the constituent entities.
 */
  newpop->entity_array = s_malloc(newpop->max_size*sizeof(entity));
  newpop->entity_iarray = s_malloc(newpop->max_size*sizeof(entity*));
  
  for (i=0; i<newpop->max_size; i++)
    {
    newpop->entity_array[i].allocated=FALSE;
    newpop->entity_array[i].data=NULL;
    newpop->entity_array[i].fitness=GA_MIN_FITNESS;
    newpop->entity_array[i].chromosome=NULL;
    }

  for (i=0; i<pop->size; i++)
    {
    newentity = ga_get_free_entity(newpop);
    ga_entity_copy(newpop, newentity, pop->entity_iarray[i]);
    }

/*
 * Add this new population into the population table.
 */
  THREAD_LOCK(pop_table_lock);
  if (!pop_table) pop_table=table_new();

  pop_id = table_add(pop_table, (vpointer) newpop);
  THREAD_UNLOCK(pop_table_lock);

  plog( LOG_DEBUG, "New pop = %p id = %d (cloned from %p)",
        newpop, pop_id, pop );

  return newpop;
  }


/**********************************************************************
  ga_get_num_populations()
  synopsis:	Gets the number of populations.
  parameters:	none
  return:	int	number of populations, -1 for undefined table.
  last updated: 13/06/01
 **********************************************************************/

int ga_get_num_populations(void)
  {
  int	num;

  THREAD_LOCK(pop_table_lock);
  if (!pop_table)
    {
    num=-1;
    }
  else
    {
    num = table_count_items(pop_table);
    }
  THREAD_UNLOCK(pop_table_lock);

  return num;
  }


/**********************************************************************
  ga_get_population_from_id()
  synopsis:	Get population pointer from its internal id.
  parameters:	unsigned int	id for population.
  return:	int
  last updated: 22/01/01
 **********************************************************************/

population *ga_get_population_from_id(unsigned int id)
  {
  return (population *) table_get_data(pop_table, id);
  }


/**********************************************************************
  ga_get_population_id()
  synopsis:	Get population internal id from its pointer.
  parameters:	unsigned int	id for population.
  return:	int
  last updated: 22/01/01
 **********************************************************************/

unsigned int ga_get_population_id(population *pop)
  {
  return table_lookup_index(pop_table, (vpointer) pop);
  }


/**********************************************************************
  ga_get_all_population_ids()
  synopsis:	Get array of internal ids for all currently
		allocated populations.  The returned array needs to
		be deallocated by the caller.
  parameters:	none
  return:	unsigned int*	array of ids
  last updated: 18 Dec 2001
 **********************************************************************/

unsigned int *ga_get_all_population_ids(void)
  {
  return table_get_index_all(pop_table);
  }


/**********************************************************************
  ga_get_all_populations()
  synopsis:	Get array of all currently allocated populations.  The
		returned array needs to be deallocated by the caller.
  parameters:	none
  return:	population**	array of population pointers
  last updated: 18 Dec 2001
 **********************************************************************/

population **ga_get_all_populations(void)
  {
  return (population **) table_get_data_all(pop_table);
  }


/**********************************************************************
  ga_entity_seed()
  synopsis:	Fills a population structure with genes.  Defined in
		a user-specified function.
  parameters:	population *	The entity's population.
		entity *	The entity.
  return:	boolean success.
  last updated:	28/02/01
 **********************************************************************/

boolean ga_entity_seed(population *pop, entity *adam)
  {

  if (!pop) die("Null pointer to population structure passed.");
  if (!pop->seed) die("Population seeding function is not defined.");

  pop->seed(pop, adam);

  return TRUE;
  }


/**********************************************************************
  ga_population_seed()
  synopsis:	Fills all entities in a population structure with
		genes from a user-specified function.
  parameters:	none
  return:	boolean success.
  last updated: 28/02/01
 **********************************************************************/

boolean ga_population_seed(population *pop)
  {
  int		i;		/* Loop variables. */
  entity	*adam;		/* New population member. */

  plog(LOG_DEBUG, "Population seeding by user-defined genesis.");

  if (!pop) die("Null pointer to population structure passed.");
  if (!pop->seed) die("Population seeding function is not defined.");

  for (i=0; i<pop->stable_size; i++)
    {
    adam = ga_get_free_entity(pop);
    pop->seed(pop, adam);
    }

  return TRUE;
  }


/**********************************************************************
  ga_population_seed_soup()
  synopsis:	Seed a population structure with starting genes from
		a previously created soup file.
  parameters:
  return:
  last updated: 06/07/00
 **********************************************************************/

boolean ga_population_seed_soup(population *pop, const char *fname)
  {
#if 0
  int		i, j, k;	/* Loop variables */
  entity	*this_entity;	/* Current population member */
  quaternion	q;		/* Quaternion */
  FILE          *fp;		/* File ptr */
  char          *line=NULL;	/* Line buffer */
  int           line_len;	/* Current buffer size */
  molstruct     *mol;		/* Current molstruct structure */
#endif

  plog(LOG_DEBUG, "Population seeding by reading soup file.");
  plog(LOG_FIXME, "Code incomplete.");

  if (!fname) die("Null pointer to filename passed.");
  if (!pop) die("Null pointer to population structure passed.");

#if 0
/*
 * Open soup file
 */
  FILE          *fp;		/* File ptr */
  if((fp=fopen(fname,"r"))==NULL)
    dief("Cannot open SOUP file \"%s\".", fname);
#endif


  return TRUE;
  }


/**********************************************************************
  ga_write_soup()
  synopsis:	Writes a soup file based on the current gene pool.
  parameters:
  return:
  last updated: 06/07/00
 **********************************************************************/

boolean ga_write_soup(population *pop)
  {

  plog(LOG_DEBUG, "Writing soup file.");
  plog(LOG_FIXME, "Code incomplete.");

  return TRUE;
  }


/**********************************************************************
  ga_population_save()
  synopsis:	Writes entire population and it's genetic data to disk.
		Note: Currently does not store any of the userdata --
		just stores the genes and fitness.
		FIXME: Broken by chromosome datatype abstraction.
  parameters:
  return:
  last updated: 12/01/01
 **********************************************************************/

boolean ga_population_save(population *pop, char *fname)
  {
  int		i;	/* Loop variables. */
  FILE          *fp;	/* File handle. */

  plog(LOG_DEBUG, "Saving population to disk.");
  if (!fname) die("Null pointer to filename passed.");
  if (!pop) die("Null pointer to population structure passed.");

/*
 * Open output file.
 */
  if((fp=fopen(fname,"w"))==NULL)
    dief("Cannot open population file \"%s\" for output.", fname);

/*
 * Program info.
 */
  fprintf(fp, "ga_core.c ga_population_save(\"%s\")\n", fname);
  fprintf(fp, "%s %s\n", VERSION_STRING, BUILD_DATE_STRING);

/*
 * Population info.
 */
  fprintf(fp, "%d %d %d %d\n", pop->size, pop->stable_size,
                  pop->num_chromosomes, pop->len_chromosomes);

/*
 * Entity info.
 */
  for (i=0; i<pop->size; i++)
    {
    fprintf(fp, "%f\n", pop->entity_iarray[i]->fitness);
/*
    for (j=0; j<pop->num_chromosomes; j++)
      {
      }
*/
    fprintf(fp, "%s\n", pop->chromosome_to_string(pop, pop->entity_iarray[i]));
    plog(LOG_DEBUG, "Entity %d has fitness %f", i, pop->entity_iarray[i]->fitness);
    }

/*
 * Footer info.
 */
  fprintf(fp, "That's all folks!\n"); 

/*
 * Close file.
 */
  fclose(fp);

  return TRUE;
  }


/**********************************************************************
  ga_population_read()
  synopsis:	Reads entire population and it's genetic data back
		from disk.
		FIXME: Broken by chromosome datatype abstraction.
  parameters:
  return:
  last updated: 06/07/00
 **********************************************************************/

population *ga_population_read(char *fname)
  {
  population	*pop=NULL;		/* New population structure. */
#if 0
  int		i, j, k;		/* Loop variables. */
  FILE          *fp;			/* File handle. */
  int		*chromosome;		/* Current chromosome. */
  char		origfname[MAX_LINE_LEN];	/* Original filename. */
  char		version_str[MAX_LINE_LEN], build_str[MAX_LINE_LEN];	/* Version details. */
  char  	test_str[MAX_LINE_LEN];	/* Test string. */
  int		size, stable_size, num_chromosomes, len_chromosomes;
  entity	*newentity;		/* New entity in new population. */

  plog(LOG_DEBUG, "Reading population from disk.");
  if (!fname) die("Null pointer to filename passed.");

/*
 * Open output file.
 */
  if((fp=fopen(fname,"r"))==NULL)
    dief("Cannot open population file \"%s\" for input.", fname);

/*
 * Program info.
 * FIXME: Potential buffer overflow.
 */
  fscanf(fp, "ga_core.c ga_population_save(\"%[^\"]\")\n", origfname);
  fscanf(fp, "%s %s\n", version_str, build_str);

  plog(LOG_DEBUG, "Reading \"%s\", \"%s\", \"%s\"",
               origfname, version_str, build_str);

/*
 * Population info.
 */
  fscanf(fp, "%d %d %d %d", &size, &stable_size,
                  &num_chromosomes, &len_chromosomes);

/*
 * Allocate a new population structure.
 */
  pop = ga_population_new(stable_size, num_chromosomes, len_chromosomes);

/*
 * Got a population structure?
 */
  if (!pop) die("Unable to allocate population structure.");

/*
 * Entity info.
 */
  for (i=0; i<size; i++)
    {
    newentity = ga_get_free_entity(pop);
    fscanf(fp, "%lf", &(newentity->fitness));

    for (j=0; j<pop->num_chromosomes; j++)
      {
      chromosome = newentity->chromosome[j];

      for (k=0; k<pop->len_chromosomes; k++)
        fscanf(fp, "%d", &(chromosome[k]));
      }
    plog(LOG_DEBUG, "Entity %d has fitness %f", i, newentity->fitness);
    }

/*
 * Footer info.
 */
  fscanf(fp, "%s", test_str); 
/* FIXME: Kludge: */
  if (strncmp(test_str, "That's all folks!", 6)) die("File appears to be corrupt.");

/*
 * Close file.
 */
  fclose(fp);

  plog(LOG_DEBUG, "Have read %d entities.", pop->size);
#endif

  return pop;
  }


/**********************************************************************
  ga_entity_evaluate()
  synopsis:	Score a single entity.
  parameters:	population *pop
		entity *entity
  return:	double			the fitness
  last updated: 06 Feb 2002
 **********************************************************************/

double ga_entity_evaluate(population *pop, entity *entity)
  {

  if (!pop) die("Null pointer to population structure passed.");
  if (!entity) die("Null pointer to entity structure passed.");
  if (!pop->evaluate) die("Evaluation callback not defined.");

  pop->evaluate(pop, entity);

  return entity->fitness;
  }


/**********************************************************************
  ga_population_score_and_sort()
  synopsis:	Score and sort entire population.  This is probably
		a good idea after reading the population from disk!
		Note: remember to define the callback functions first.
  parameters:
  return:
  last updated: 28/02/01
 **********************************************************************/

boolean ga_population_score_and_sort(population *pop)
  {
  int		i;		/* Loop variable over all entities. */
  double	origfitness;	/* Stored fitness value. */

/*
 * Score and sort the read population members.
 *
 * Note that this will (potentially) use a huge amount of memory more
 * than the original population data if the userdata hasn't been maintained.
 * Each chromosome is decoded separately, whereas originally many
 * degenerate chromosomes would share their userdata elements.
 */
  for (i=0; i<pop->size; i++)
    {
    origfitness = pop->entity_iarray[i]->fitness;
    pop->evaluate(pop, pop->entity_iarray[i]);

#if GA_DEBUG>2
    if (origfitness != pop->entity_iarray[i]->fitness)
      plog(LOG_NORMAL,
           "Recalculated fitness %f doesn't match stored fitness %f for entity %d.",
           pop->entity_iarray[i]->fitness, origfitness, i);
#endif
    }

  quicksort_population(pop);

  return TRUE;
  }


#if 0
FIXME: The following 3 functions need to be fixed for the new absracted chromosome types.
/**********************************************************************
  ga_population_convergence_genotypes()
  synopsis:	Determine ratio of converged genotypes in population.
  parameters:
  return:
  last updated: 31/05/01
 **********************************************************************/

double ga_population_convergence_genotypes( population *pop )
  {
  int		i, j;		/* Loop over pairs of entities. */
  int		total=0, converged=0;	/* Number of comparisons, matches. */

  if (!pop) die("Null pointer to population structure passed.");
  if (pop->size < 1) die("Pointer to empty population structure passed.");

  for (i=1; i<pop->size; i++)
    {
    for (j=0; j<i; j++)
      {
      if (ga_compare_genome(pop, pop->entity_iarray[i], pop->entity_iarray[j]))
        converged++;
      total++;
      }
    }

  return (double) converged/total;
  }


/**********************************************************************
  ga_population_convergence_chromosomes()
  synopsis:	Determine ratio of converged chromosomes in population.
  parameters:
  return:
  last updated: 31/05/01
 **********************************************************************/

double ga_population_convergence_chromosomes( population *pop )
  {
  int		i, j;		/* Loop over pairs of entities. */
  int		k;		/* Loop over chromosomes. */
  int		total=0, converged=0;	/* Number of comparisons, matches. */

  if (!pop) die("Null pointer to population structure passed.");
  if (pop->size < 1) die("Pointer to empty population structure passed.");

  for (i=1; i<pop->size; i++)
    {
    for (j=0; j<i; j++)
      {
      for (k=0; k<pop->num_chromosomes; k++)
        {
/* FIXME: Not totally effiecient: */
        if (ga_count_match_alleles( pop->len_chromosomes,
                                    pop->entity_iarray[i]->chromosome[k],
                                    pop->entity_iarray[j]->chromosome[k] ) == pop->len_chromosomes)
          converged++;
        total++;
        }
      }
    }

  return (double) converged/total;
  }


/**********************************************************************
  ga_population_convergence_alleles()
  synopsis:	Determine ratio of converged alleles in population.
  parameters:
  return:
  last updated: 31/05/01
 **********************************************************************/

double ga_population_convergence_alleles( population *pop )
  {
  int		i, j;		/* Loop over pairs of entities. */
  int		k;		/* Loop over chromosomes. */
  int		total=0, converged=0;	/* Number of comparisons, matches. */

  if (!pop) die("Null pointer to population structure passed.");
  if (pop->size < 1) die("Pointer to empty population structure passed.");

  for (i=1; i<pop->size; i++)
    {
    for (j=0; j<i; j++)
      {
      for (k=0; k<pop->num_chromosomes; k++)
        {
        converged+=ga_count_match_alleles( pop->len_chromosomes,
                                           pop->entity_iarray[i]->chromosome[k],
                                           pop->entity_iarray[j]->chromosome[k] );
        total+=pop->len_chromosomes;
        }
      }
    }

  return (double) converged/total;
  }
#endif


/**********************************************************************
  ga_get_entity_rank()
  synopsis:	Gets an entity's rank (subscript into entity_iarray of
		the population).  This is not necessarily the fitness
		rank unless the population has been sorted.
  parameters:
  return:
  last updated: 22/01/01
 **********************************************************************/

int ga_get_entity_rank(population *pop, entity *e)
  {
  int	rank=0;		/* The rank. */

  while (rank < pop->size)
    {
    if (pop->entity_iarray[rank] == e) return rank;
    rank++;
    }

  return -1;
  }


/**********************************************************************
  ga_get_entity_rank_from_id()
  synopsis:	Gets an entity's rank (subscript into entity_iarray of
		the population).  This is not necessarily the fitness
		rank unless the population has been sorted.
  parameters:
  return:
  last updated: 16/03/01
 **********************************************************************/

int ga_get_entity_rank_from_id(population *pop, int id)
  {
  int	rank=0;		/* The rank. */

  while (rank < pop->size)
    {
    if (pop->entity_iarray[rank] == &(pop->entity_array[id])) return rank;
    rank++;
    }

  return -1;
  }


/**********************************************************************
  ga_get_entity_id_from_rank()
  synopsis:	Gets an entity's id from its rank.
  parameters:
  return:
  last updated: 16/03/01
 **********************************************************************/

int ga_get_entity_id_from_rank(population *pop, int rank)
  {
  int	id=0;		/* The entity's index. */

  while (id < pop->max_size)
    {
    if (&(pop->entity_array[id]) == pop->entity_iarray[rank]) return id;
    id++;
    }

  return -1;
  }


/**********************************************************************
  ga_get_entity_id()
  synopsis:	Gets an entity's internal index (subscript into the
		entity_array buffer).
		FIXME: Add some more error checking.
  parameters:
  return:
  last updated: 22/01/01
 **********************************************************************/

int ga_get_entity_id(population *pop, entity *e)
  {
  int	id=0;	/* The index. */

  if (!e) die("Null pointer to entity structure passed.");

  while (id < pop->max_size)
    {
    if (&(pop->entity_array[id]) == e) return id;
    id++;
    }

  return -1;

/*  return (&(pop->entity_array[0]) - &(e[0]))/sizeof(entity);*/
/*  return (pop->entity_array - e)/sizeof(entity);*/
  }


/**********************************************************************
  ga_get_entity_from_id()
  synopsis:	Gets a pointer to an entity from it's internal index
		(subscript into the entity_array buffer).
		FIXME: Add some error checking.
  parameters:
  return:
  last updated: 22/01/01
 **********************************************************************/

entity *ga_get_entity_from_id(population *pop, const unsigned int id)
  {
  return &(pop->entity_array[id]);
  }


/**********************************************************************
  ga_get_entity_from_rank()
  synopsis:	Gets a pointer to an entity from it's internal rank.
		(subscript into the entity_iarray buffer).
		Note that this only relates to fitness ranking if
		the population has been properly sorted.
		FIXME: Add some error checking.
  parameters:
  return:
  last updated: 22/01/01
 **********************************************************************/

entity *ga_get_entity_from_rank(population *pop, const unsigned int rank)
  {
  return pop->entity_iarray[rank];
  }


/**********************************************************************
  ga_entity_setup()
  synopsis:	Allocates/prepares an entity structure for use.
		Chromosomes are allocated, but will contain garbage.
  parameters:
  return:
  last updated: 19/12/00
 **********************************************************************/

boolean ga_entity_setup(population *pop, entity *joe)
  {

  if (!joe) die("Null pointer to entity structure passed.");

/* Allocate chromosome structures. */
  if (joe->chromosome==NULL)
    {
    if (!pop->chromosome_constructor) die("Chromosome constructor not defined.");

    pop->chromosome_constructor(pop, joe);
    }

/* Physical characteristics currently undefined. */
  joe->data=NULL;

  joe->fitness=GA_MIN_FITNESS;	/* Lower bound on fitness */

/* Mark it as used. */
  joe->allocated=TRUE;

  return TRUE;
  }


/**********************************************************************
  ga_entity_dereference_by_rank()
  synopsis:	Marks an entity structure as unused.
		Deallocation is expensive.  It is better to re-use this
		memory.  So, that is what we do.
		Any contents of entities data field are freed.
		If rank is known, this is much quicker than the plain
		ga_entity_dereference() function.
		Note, no error checking in the interests of speed.
  parameters:
  return:
  last updated:	16/03/01
 **********************************************************************/

boolean ga_entity_dereference_by_rank(population *pop, int rank)
  {
  int		i;	/* Loop variable over the indexed array. */
  entity	*dying=pop->entity_iarray[rank];	/* Dead entity. */

/* Bye bye entity. */
  dying->allocated=FALSE;

/* Clear user data. */
  if (dying->data)
    {
    destruct_list(pop, dying->data);
    dying->data=NULL;
    }

/* Population size is one less now! */
  pop->size--;

/* Update entity_iarray[], so there are no gaps! */
  for (i=rank; i<pop->size; i++)
    pop->entity_iarray[i] = pop->entity_iarray[i+1];

/*
  printf("ENTITY %d DEREFERENCED.\n", ga_get_entity_id(dying));
  printf("New pop size = %d\n", pop->size);
*/

  return TRUE;
  }


/**********************************************************************
  ga_entity_dereference()
  synopsis:	Marks an entity structure as unused.
		Deallocation is expensive.  It is better to re-use this
		memory.  So, that is what we do.
		Any contents of entities data field are freed.
		If rank is known, this above
		ga_entity_dereference_by_rank() function is much
		quicker.
  parameters:
  return:
  last updated:	16/03/01
 **********************************************************************/

boolean ga_entity_dereference(population *pop, entity *dying)
  {
  return ga_entity_dereference_by_rank(pop, ga_get_entity_rank(pop, dying));
  }


/**********************************************************************
  ga_entity_clear_data()
  synopsis:	Clears some of the entity's data.  Safe if data doesn't
		exist anyway.
  parameters:
  return:
  last updated: 20/12/00
 **********************************************************************/

void ga_entity_clear_data(population *p, entity *entity, const int chromosome)
  {
  SLList	*tmplist;
  vpointer	data;		/* Data in item. */

  if (entity->data)
    {
    tmplist = slink_nth(entity->data, chromosome);
    if ( (data = slink_data(tmplist)) )
      {
      p->data_destructor(data);
      tmplist->data=NULL;
      }
    }

  return;
  }


/**********************************************************************
  ga_entity_blank()
  synopsis:	Clears the entity's data.
		Equivalent to an optimised ga_entity_dereference()
		followed by ga_get_free_entity().  It is much more
		preferable to use this fuction!
		Chromosomes are guarenteed to be intact, but may be
		overwritten by user.
  parameters:
  return:
  last updated: 18/12/00
 **********************************************************************/

void ga_entity_blank(population *p, entity *entity)
  {
  if (entity->data)
    {
    destruct_list(p, entity->data);
    entity->data=NULL;
    }

  entity->fitness=GA_MIN_FITNESS;

/*
  printf("ENTITY %d CLEARED.\n", ga_get_entity_id(entity));
*/

  return;
  }


/**********************************************************************
  ga_get_free_entity()
  synopsis:	Returns pointer to an unused entity structure from the
		population's entity pool.  Increments population size
		too.
  parameters:
  return:
  last updated: 13 Feb 2002
 **********************************************************************/

entity *ga_get_free_entity(population *pop)
  {
  static int	index=0;	/* Current position in entity array. */
  int		new_max_size;	/* Increased maximum number of entities. */
  int		i;
  int		*entity_indices;	/* Offsets within entity buffer. */

/*
  plog(LOG_DEBUG, "Locating free entity structure.");
*/

/*
 * Do we have any free structures?
 */
  if (pop->max_size == (pop->size+1))
    {	/* No. */
    plog(LOG_VERBOSE, "No unused entities available -- allocating additional structures.");

/*
 * Note that doing this offset calculation stuff may seem overly expensive, but
 * it the only available techinique that is guarenteed to be portable.
 * Memory is freed as soon as possible to reduce page faults with large populations.
 */
    entity_indices = s_malloc(pop->max_size*sizeof(int));
    for (i=0; i<pop->max_size; i++)
      {
      entity_indices[i] = pop->entity_iarray[i]-pop->entity_array;
      }

    s_free(pop->entity_iarray);

    new_max_size = (pop->size * 3)/2;
    pop->entity_array = s_realloc(pop->entity_array, new_max_size*sizeof(entity));
    pop->entity_iarray = s_malloc(new_max_size*sizeof(entity*));

    for (i=0; i<pop->max_size; i++)
      {
      pop->entity_iarray[i] = pop->entity_array+entity_indices[i];
      }

    s_free(entity_indices);

    for (i=pop->max_size; i<new_max_size; i++)
      {
      pop->entity_array[i].allocated=FALSE;
      pop->entity_array[i].data=NULL;
      pop->entity_array[i].fitness=GA_MIN_FITNESS;
      pop->entity_array[i].chromosome=NULL;
      }

    pop->max_size = new_max_size;
    }

/* Find unused entity structure */
  while (pop->entity_array[index].allocated==TRUE)
    {
    if (index == 0) index=pop->max_size;
    index--;
    }

/* Prepare it */
  ga_entity_setup(pop, &(pop->entity_array[index]));

/* Store in lowest free slot in entity_iarray */
  pop->entity_iarray[pop->size] = &(pop->entity_array[index]);

/* Population is bigger now! */
  pop->size++;

/*
  printf("ENTITY %d ALLOCATED.\n", index);
*/

  return &(pop->entity_array[index]);
  }


/**********************************************************************
  ga_copy_data()
  synopsis:	Copy one chromosome's portion of the data field of an
		entity structure to another entity structure.  (Copies
		the portion of the phenome relating to that chromosome)
		'Copies' NULL data safely.
		The destination chromosomes must be filled in order.
		If these entities are in differing populations, no
		problems will occur provided that the
		data_ref_incrementor callbacks are identical or at least
		compatible.
  parameters:
  return:
  last updated: 18/12/00
 **********************************************************************/

boolean ga_copy_data(population *pop, entity *dest, entity *src, const int chromosome)
  {
  vpointer	tmpdata;	/* Temporary pointer. */

  if (!src || (tmpdata = slink_nth_data(src->data, chromosome)) == NULL)
    {
    dest->data = slink_append(dest->data, NULL);
    }
  else
    {
    dest->data = slink_append(dest->data, tmpdata);
    pop->data_ref_incrementor(tmpdata);
    }

  return TRUE;
  }


/**********************************************************************
  ga_copy_chromosome()
  synopsis:	Copy one chromosome between entities.
		If these entities are in differing populations, no
		problems will occur provided that the chromosome
		datatypes match up.
  parameters:
  return:
  last updated: 18/12/00
 **********************************************************************/

boolean ga_copy_chromosome( population *pop, entity *dest, entity *src,
                            const int chromosome )
  {

  pop->chromosome_replicate(pop, src, dest, chromosome);

  return TRUE;
  }


/**********************************************************************
  ga_entity_copy_all_chromosomes()
  synopsis:	Copy genetic data between entity structures.
		If these entities are in differing populations, no
		problems will occur provided that the chromosome
		properties are identical.
  parameters:
  return:
  last updated: 20/12/00
 **********************************************************************/

boolean ga_entity_copy_all_chromosomes(population *pop, entity *dest, entity *src)
  {
  int		i;		/* Loop variable over all chromosomes. */

  /* Checks */
  if (!dest || !src) die("Null pointer to entity structure passed");

/*
 * Ensure destination structure is not likely be already in use.
 */
  if (dest->data) die("Why does this entity already contain data?");

/*
 * Copy genetic data.
 */
  for (i=0; i<pop->num_chromosomes; i++)
    {
    ga_copy_data(pop, dest, src, i);		/* Phenome. */
    ga_copy_chromosome(pop, dest, src, i);	/* Genome. */
    }

  return TRUE;
  }


/**********************************************************************
  ga_entity_copy_chromosome()
  synopsis:	Copy chromosome and structural data between entity
		structures.
  parameters:
  return:
  last updated: 22/01/01
 **********************************************************************/

boolean ga_entity_copy_chromosome(population *pop, entity *dest, entity *src, int chromo)
  {

/* Checks. */
  if (!dest || !src) die("Null pointer to entity structure passed");
  if (chromo<0 || chromo>=pop->num_chromosomes) die("Invalid chromosome number.");

/*
 * Ensure destination structure is not likely be already in use.
 */
  if (dest->data) die("Why does this entity already contain data?");

/*
 * Copy genetic and associated structural data (phenomic data).
 */
/*
  memcpy(dest->chromosome[chromo], src->chromosome[chromo],
           pop->len_chromosomes*sizeof(int));
*/
  ga_copy_data(pop, dest, src, chromo);
  ga_copy_chromosome(pop, dest, src, chromo);

  return TRUE;
  }


/**********************************************************************
  ga_entity_copy()
  synopsis:	Er..., copy entire entity structure.  This is safe
		for copying between popultions provided that they
		are compatible.
  parameters:
  return:
  last updated:	22/01/01
 **********************************************************************/

boolean ga_entity_copy(population *pop, entity *dest, entity *src)
  {

  ga_entity_copy_all_chromosomes(pop, dest, src);
  dest->fitness = src->fitness;

  return TRUE;
  }


/**********************************************************************
  ga_entity_clone()
  synopsis:	Clone an entity structure.
		Safe for cloning into a different population, provided
		that the populations are compatible.
  parameters:	population	*pop	Population.
		entity	*parent		The original entity.
  return:	entity	*dolly		The new entity.
  last updated:	07/07/01
 **********************************************************************/

entity *ga_entity_clone(population *pop, entity *parent)
  {
  entity	*dolly;		/* The clone. */

  dolly = ga_get_free_entity(pop);

  ga_entity_copy_all_chromosomes(pop, dolly, parent);
  dolly->fitness = parent->fitness;

  return dolly;
  }


/**********************************************************************
  Network communication (population/entity migration) functions.
 **********************************************************************/

/**********************************************************************
  ga_population_send_by_mask()
  synopsis:	Send selected entities from a population to another
		processor.  Only fitness and chromosomes sent.
  parameters:
  return:
  last updated: 31 Jan 2002
 **********************************************************************/

void ga_population_send_by_mask( population *pop, int dest_node, int num_to_send, boolean *send_mask )
  {
  int		i;
  int		count=0;
  unsigned int	len, max_len=0;		/* Length of buffer to send. */
  byte		*buffer=NULL;

/*
 * Send number of entities.
 */
  mpi_send(&num_to_send, 1, MPI_TYPE_INT, dest_node, GA_TAG_NUMENTITIES);

/* 
 * Slight knudge to determine length of buffer.  Should have a more
 * elegant approach for this.
 * Sending this length here should not be required at all.
 */
  len = (int) pop->chromosome_to_bytes(pop, pop->entity_iarray[0], &buffer, &max_len);
  mpi_send(&len, 1, MPI_TYPE_INT, dest_node, GA_TAG_ENTITYLEN);

/*
  printf("DEBUG: Node %d sending %d entities of length %d to %d\n",
           mpi_get_rank(), num_to_send, len, dest_node);
*/

/*
 * Send required entities individually.
 */
  for (i=0; i<pop->size && count<num_to_send; i++)
    {
    if (send_mask[i])
      {
/* printf("DEBUG: Node %d sending entity %d/%d (%d/%d) with fitness %f\n",
             mpi_get_rank(), count, num_to_send, i, pop->size, pop->entity_iarray[i]->fitness); */
      mpi_send(&(pop->entity_iarray[i]->fitness), 1, MPI_TYPE_DOUBLE, dest_node, GA_TAG_ENTITYFITNESS);
      if (len != (int) pop->chromosome_to_bytes(pop, pop->entity_iarray[i], &buffer, &max_len))
	die("Internal length mismatch");
      mpi_send(buffer, len, MPI_TYPE_BYTE, dest_node, GA_TAG_ENTITYCHROMOSOME);
      count++;
      }
    }

  if (count != num_to_send)
    die("Incorrect value for num_to_send");

/*
 * We only need to deallocate the buffer if it was allocated (i.e. if
 * the "chromosome_to_bytes" callback set max_len).
 */
  if (max_len) s_free(buffer);

/*  printf("DEBUG: Node %d finished sending\n", mpi_get_rank());*/

  return;
  }


/**********************************************************************
  ga_population_send_every()
  synopsis:	Send all entities from a population to another
		processor.  Only fitness and chromosomes sent.
  parameters:
  return:
  last updated: 31 Jan 2002
 **********************************************************************/

void ga_population_send_every( population *pop, int dest_node )
  {
  int		i;
  int		len;			/* Length of buffer to send. */
  unsigned int	max_len=0;		/* Maximum length of buffer. */
  byte		*buffer=NULL;

/*
 * Send number of entities.
 */
  mpi_send(&(pop->size), 1, MPI_TYPE_INT, dest_node, GA_TAG_NUMENTITIES);

/* 
 * Slight knudge to determine length of buffer.  Should have a more
 * elegant approach for this.
 * Sending this length here should not be required at all.
 */
  len = (int) pop->chromosome_to_bytes(pop, pop->entity_iarray[0], &buffer, &max_len);
  mpi_send(&len, 1, MPI_TYPE_INT, dest_node, GA_TAG_ENTITYLEN);

/*
 * Send all entities individually.
 */
  for (i=0; i<pop->size; i++)
    {
    mpi_send(&(pop->entity_iarray[i]->fitness), 1, MPI_TYPE_DOUBLE, dest_node, GA_TAG_ENTITYFITNESS);
    if (len != (int) pop->chromosome_to_bytes(pop, pop->entity_iarray[i], &buffer, &max_len))
      die("Internal length mismatch");
    mpi_send(buffer, len, MPI_TYPE_BYTE, dest_node, GA_TAG_ENTITYCHROMOSOME);
    }

/*
 * We only need to deallocate the buffer if it was allocated (i.e. if
 * the "chromosome_to_bytes" callback set max_len).
 */
  if (max_len) s_free(buffer);

  return;
  }


/**********************************************************************
  ga_population_append_receive()
  synopsis:	Recieve a set of entities from a population on another
		processor and append them to a current population.
		Only fitness and chromosomes received.
  parameters:
  return:
  last updated: 31 Jan 2002
 **********************************************************************/

void ga_population_append_receive( population *pop, int src_node )
  {
  int		i;
  int		len;			/* Length of buffer to receive. */
  byte		*buffer;		/* Receive buffer. */
  int		num_to_recv;		/* Number of entities to receive. */
  entity	*entity;		/* New entity. */

/*
 * Get number of entities to receive and the length of each.
 * FIXME: This length data shouldn't be needed!
 */
  mpi_receive(&num_to_recv, 1, MPI_TYPE_INT, src_node, GA_TAG_NUMENTITIES);
  mpi_receive(&len, 1, MPI_TYPE_INT, src_node, GA_TAG_ENTITYLEN);

/*
  printf("DEBUG: Node %d anticipating %d entities of length %d from %d\n",
           mpi_get_rank(), num_to_recv, len, src_node);
*/

  if (num_to_recv>0)
    {
    buffer = s_malloc(len*sizeof(byte));

/*
 * Receive all entities individually.
 */
    for (i=0; i<num_to_recv; i++)
      {
      entity = ga_get_free_entity(pop);
      mpi_receive(&(entity->fitness), 1, MPI_TYPE_DOUBLE, src_node, GA_TAG_ENTITYFITNESS);
      mpi_receive(buffer, len, MPI_TYPE_BYTE, src_node, GA_TAG_ENTITYCHROMOSOME);
      pop->chromosome_from_bytes(pop, entity, buffer);
/*      printf("DEBUG: Node %d received entity %d/%d (%d) with fitness %f\n",
             mpi_get_rank(), i, num_to_recv, pop->size, entity->fitness);
 */
      }

    s_free(buffer);
    }

/*  printf("DEBUG: Node %d finished receiving\n", mpi_get_rank());*/

  return;
  }


/**********************************************************************
  ga_population_new_receive()
  synopsis:	Recieve a population structure (excluding actual
  		entities) from another processor.
		Note that the callbacks wiil need to be subsequently
		defined by the user.
  parameters:
  return:
  last updated: 24 Jan 2002
 **********************************************************************/

population *ga_population_new_receive( int src_node )
  {
  population *pop=NULL;

  plog(LOG_FIXME, "Function not implemented");

  return pop;
  }


/**********************************************************************
  ga_population_receive()
  synopsis:	Recieve a population structure (including actual
  		entities) from another processor.
  parameters:
  return:
  last updated: 24 Jan 2002
 **********************************************************************/

population *ga_population_receive( int src_node )
  {
  population *pop;

  pop = ga_population_new_receive( src_node );
  ga_population_append_receive( pop, src_node );

  return pop;
  }


/**********************************************************************
  ga_population_send()
  synopsis:	Send population structure (excluding actual entities)
 		to another processor.
  parameters:
  return:
  last updated: 24 Jan 2002
 **********************************************************************/

void ga_population_send( population *pop, int dest_node )
  {
  plog(LOG_FIXME, "Function not implemented");

  return;
  }


/**********************************************************************
  ga_population_send_all()
  synopsis:	Send population structure (including all entities)
 		to another processor.
  parameters:
  return:
  last updated: 24 Jan 2002
 **********************************************************************/

void ga_population_send_all( population *pop, int dest_node )
  {

  ga_population_send(pop, dest_node);
  ga_population_send_every(pop, dest_node);

  return;
  }


#if 0
/**********************************************************************
  ga_marktosend_entity()
  synopsis:	Mark an entity to be sent to another subpopulation
		(i.e. jump to another processor).
  parameters:
  return:
  last updated: 22/09/00
 **********************************************************************/

void ga_marktosend_entity(int *send_mask)
  {
  }
#endif


#if 0
/**********************************************************************
  ga_multiproc_compare_entities()
  synopsis:	Synchronise processors and if appropriate transfer
		better solution to this processor.
		local will contain the optimum solution from local
		and localnew on all processors.
  parameters:
  return:
  last updated:	18/12/00
 **********************************************************************/

entity *ga_multiproc_compare_entities( population *pop, entity *localnew, entity *local )
  {
  double	global_max;		/* Maximum value across all nodes. */
  int		maxnode;		/* Node with maximum value. */
  int		*buffer=NULL;		/* Send/receive buffer. */
  int		*buffer_ptr=NULL;	/* Current position in end/receive buffer. */
  int		buffer_size;		/* Size of buffer. */
  int		j;			/* Loop over chromosomes. */
  entity	*tmpentity;		/* Entity ptr for swapping. */

  plog(LOG_FIXME, "Warning... untested code.");

  maxnode = mpi_find_global_max(MAX(localnew->fitness, local->fitness), &global_max);

  buffer_size = pop->num_chromosomes*pop->len_chromosomes;
  buffer_ptr = buffer = s_malloc(buffer_size*sizeof(int));

  if (maxnode == mpi_get_rank())
    {
    if (localnew->fitness > local->fitness)
      {
      tmpentity = local;
      local = localnew;
      localnew = tmpentity;
      }

    for (j=0; j<pop->num_chromosomes; j++)
      {
      memcpy(buffer_ptr, local->chromosome[j], pop->len_chromosomes*sizeof(int));
      buffer_ptr += pop->len_chromosomes;
      }

    mpi_distribute( buffer, buffer_size, MPI_TYPE_INT, maxnode, GA_TAG_BESTSYNC );
    }
  else
    {
    mpi_distribute( buffer, buffer_size, MPI_TYPE_INT, maxnode, GA_TAG_BESTSYNC );

    for (j=0; j<pop->num_chromosomes; j++)
      {
      memcpy(local->chromosome[j], buffer_ptr, pop->len_chromosomes*sizeof(int));
      buffer_ptr += pop->len_chromosomes;
      }

    pop->evaluate(pop, local);
    if (local->fitness != global_max)
      dief("Best scores don't match %f %f.", local->fitness, global_max);
    }

  s_free(buffer);

  return local;
  }


/**********************************************************************
  ga_sendrecv_entities()
  synopsis:	Make entities change subpopulations based on the
		previously set mask. (i.e. entities jump to
		another processor).
		Currently only sends the genetic data and rebuilds the
		structure.
		FIXME: Send structural data too.
		(This functionality should be provided by a user
		specified callback.)
  parameters:
  return:
  last updated: 22/09/00
 **********************************************************************/

boolean ga_sendrecv_entities( population *pop, int *send_mask, int send_count )
  {
  int		i, j;			/* Loop over all chromosomes in all entities. */
  int		next, prev;		/* Processor to send/receive entities with. */
  int		*buffer=NULL;		/* Send/receive buffer. */
  int		*buffer_ptr=NULL;	/* Current position in end/receive buffer. */
  int		recv_count;		/* Number of entities to receive. */
  int		recv_size, send_size=0;	/* Size of buffer. */
  int		index=0;		/* Index of entity to send. */
  entity	*immigrant;		/* New entity. */

  plog(LOG_FIXME, "Warning... untested code.");

/* Checks */
  if (!pop) die("Null pointer to population structure passed.");
  if (!send_mask) die("Null pointer to int array.");

  next = mpi_get_next_rank();
  prev = mpi_get_prev_rank();

/* Pack chromosomes into buffer. */
  if (send_count > 0)
    {
    send_size = send_count*pop->num_chromosomes*pop->len_chromosomes;
    if ( !(buffer=s_malloc(send_size*sizeof(int))) )
      die("Unable to allocate memory.");

    buffer_ptr = buffer;

    for (i=0; i<send_count; i++)
      {
      while ( *send_mask == 0 )
        {	/* Skipping structure. */
        send_mask++;
        index++;
        }

      for (j=0; j<pop->num_chromosomes; j++)
        {
        memcpy(buffer_ptr,
               pop->entity_iarray[index]->chromosome[j],
               pop->len_chromosomes*sizeof(int));
        buffer_ptr += pop->len_chromosomes;
        }

      send_mask++;	/* Ready for next loop */
      index++;
      }
    }

/* Send data to next node. */
  plog(LOG_DEBUG, "Sending %d to node %d.", send_count, next);
  mpi_send( &send_count, 1, MPI_TYPE_INT, next, GA_TAG_MIGRATIONINFO );

  if (send_count > 0)
    {
    plog(LOG_DEBUG, "Sending %d ints to node %d.", send_size, next);
    mpi_send( buffer, send_size, MPI_TYPE_INT, next, GA_TAG_MIGRATIONDATA );
    }

/*
  helga_start_timer();
*/

/* Recieve data from previous node. */
  plog(LOG_DEBUG, "Recieving messages from node %d.", prev);

  mpi_receive( &recv_count, 1, MPI_TYPE_INT, prev, GA_TAG_MIGRATIONINFO );

  plog(LOG_DEBUG, "Will be recieving %d entities = %d ints (%Zd bytes).",
            recv_count,
            recv_count*pop->num_chromosomes*pop->len_chromosomes,
            recv_count*pop->num_chromosomes*pop->len_chromosomes*sizeof(int));

  if (recv_count > 0)
    {
    recv_size = recv_count*pop->num_chromosomes*pop->len_chromosomes;
    if ( !(buffer=s_realloc(buffer, recv_size*sizeof(int))) )
      die("Unable to reallocate memory.");

    buffer_ptr = buffer;

    mpi_receive( buffer, recv_size, MPI_TYPE_INT, prev, GA_TAG_MIGRATIONDATA );

    for (i=0; i<recv_count; i++)
      {
      immigrant = ga_get_free_entity(pop);
      for (j=0; j<pop->num_chromosomes; j++)
        {
        memcpy(buffer_ptr,
               immigrant->chromosome[j],
               pop->len_chromosomes*sizeof(int));
        buffer_ptr += pop->len_chromosomes;
        }
      pop->evaluate(pop, immigrant);

/*
      plog(LOG_VERBOSE, "Immigrant has fitness %f", immigrant->fitness);
*/
      }
    }

/* How much time did we waste? */
/*
  helga_check_timer();
*/

  if (buffer) s_free(buffer);

  return TRUE;
  }
#endif


#if 0
/**********************************************************************
  ga_recv_entities()
  synopsis:	Handle immigration.
  parameters:
  return:
  last updated: 08/08/00
 **********************************************************************/

boolean ga_recv_entities(entity *migrant)
  {
  int		i;		/* Loop over all chromosomes */

  /* Checks */
  if (!migrant) die("Null pointer to entity structure passed");

  /* Ensure destination structure is ready */
  ga_entity_setup(migrant);

  for (i=0; i<pop->num_chromosomes; i++)
    {
    }

  return TRUE;
  }
#endif


/**********************************************************************
  Environmental operator function.
 **********************************************************************/

/**********************************************************************
  ga_optimise_entity()
  synopsis:	Optimise the entity's structure through systematic
		searching in the gene space.
		Should be default choice for "adaptation" function.
  parameters:
  return:
  last updated: 01/08/00
 **********************************************************************/

entity *ga_optimise_entity(population *pop, entity *unopt)
  {
  entity	*optimised;

  /* Checks */
  if (!unopt) die("Null pointer to entity structure passed.");

  plog(LOG_FIXME,
            "Code incomplete, using 20 iterations of the RMHC algorithm for now.");

/* FIXME: hard-coded values. */
  optimised = ga_random_mutation_hill_climbing( pop, unopt, 20 );

  plog(LOG_DEBUG, "Entity optimised from %f to %f.",
            unopt->fitness, optimised->fitness);

  return optimised;
  }


/**********************************************************************
  GA functions.
 **********************************************************************/

/**********************************************************************
  ga_population_set_parameters()
  synopsis:	Sets the GA parameters for a population.
  parameters:
  return:
  last updated:	23/04/01
 **********************************************************************/

void ga_population_set_parameters(	population	*pop,
					double	crossover,
					double	mutation,
					double	migration)
  {

  if ( !pop ) die("Null pointer to population structure passed.");

  plog( LOG_VERBOSE,
        "The population's GA parameters have been set. cro. %f mut. %f mig. %f",
        crossover, mutation, migration );

  pop->crossover_ratio = crossover;
  pop->mutation_ratio = mutation;
  pop->migration_ratio = migration;

  return;
  }


/**********************************************************************
  ga_transcend()
  synopsis:	Return a population structure to user for analysis or
		whatever.  But remove it from the population table.
		(Like ga_extinction, except doesn't purge memory.)
  parameters:
  return:
  last updated:	19/01/01
 **********************************************************************/

population *ga_transcend(unsigned int id)
  {
  plog(LOG_VERBOSE, "This population has achieved transcendance!");

  return (population *) table_remove_index(pop_table, id);
  }


/**********************************************************************
  ga_resurect()
  synopsis:	Restores a population structure into the population
		table from an external source.
  parameters:
  return:
  last updated:	19/01/01
 **********************************************************************/

unsigned int ga_resurect(population *pop)
  {
  plog(LOG_VERBOSE, "The population has been restored!");

  return table_add(pop_table, pop);
  }


/**********************************************************************
  ga_extinction()
  synopsis:	Purge all memory used by a population.
  parameters:
  return:
  last updated:	12/07/00
 **********************************************************************/

boolean ga_extinction(population *extinct)
  {
  int		i;		/* Loop over population members (existing or not). */
  int		id;		/* Internal index for this extinct population. */

  plog(LOG_VERBOSE, "This population is becoming extinct!");
/*
  ga_population_dump(extinct);
*/

  if (!extinct) die("Null pointer to population structure passed.");

/*
 * Remove this population from the population table.
 */
  THREAD_LOCK(pop_table_lock);
  id = table_remove_data(pop_table, extinct);
  THREAD_UNLOCK(pop_table_lock);

/*
 * Error check.
 */
  if (id == TABLE_ERROR_INDEX)
    die("Unable to find population structure in table.");

/*
 * Any user-data?
 */
  if (extinct->data)
    plog(LOG_WARNING, "User data field is not empty. (Potential memory leak)");

/*
 * Dereference/free everyting.
 */
  if (!ga_genocide(extinct, 0))
    {
    plog(LOG_NORMAL, "This population is already extinct!");
    }
  else
    {
    for (i=0; i<extinct->max_size; i++)
      {
      if (extinct->entity_array[i].chromosome)
        {
        extinct->chromosome_destructor(extinct, &(extinct->entity_array[i]));
        }
      }

    s_free(extinct->entity_array);
    s_free(extinct->entity_iarray);
    s_free(extinct);
    }

  return TRUE;
  }


/**********************************************************************
  ga_genocide()
  synopsis:	Kill entities to reduce population size down to
		specified value.
  parameters:
  return:
  last updated:	11/01/01
 **********************************************************************/

boolean ga_genocide(population *pop, int target_size)
  {
  if (!pop) return FALSE;

  plog(LOG_VERBOSE,
            "The population is being culled from %d to %d individuals!",
            pop->size, target_size);

/*
 * Dereference the structures relating to the least
 * fit population members.  (I assume the population has been
 * sorted by fitness.)
 */
  while (pop->size>target_size)
    {
    ga_entity_dereference_by_rank(pop, pop->size-1);
    }

  return TRUE;
  }


