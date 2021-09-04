/*************************************************************************/
/*                                                                       */
/*  Copyright (c) 1994 Stanford University                               */
/*                                                                       */
/*  All rights reserved.                                                 */
/*                                                                       */
/*  Permission is given to use, copy, and modify this software for any   */
/*  non-commercial purpose as long as this copyright notice is not       */
/*  removed.  All other uses, including redistribution in whole or in    */
/*  part, are forbidden without prior written permission.                */
/*                                                                       */
/*  This software is provided with absolutely no warranty and no         */
/*  support.                                                             */
/*                                                                       */
/*************************************************************************/

#include "defs.h"
#include "memory.h"
#include "box.h"
#include "partition_grid.h"
#include "cost_zones.h"

#define NUM_DIRECTIONS 4

typedef enum { RIGHT, LEFT, UP, DOWN } direction;

static int Child_Sequence[NUM_DIRECTIONS][NUM_OFFSPRING] = 
{
   { 0, 1, 2, 3 },
   { 2, 3, 0, 1 },
   { 0, 3, 2, 1 },
   { 2, 1, 0, 3 },
};
static int Direction_Sequence[NUM_DIRECTIONS][NUM_OFFSPRING] =
{
   { UP, RIGHT, RIGHT, DOWN },
   { DOWN, LEFT, LEFT, UP },
   { RIGHT, UP, UP, LEFT },
   { LEFT, DOWN, DOWN, RIGHT },
};

void ComputeSubTreeCosts(int my_id, box *b);
void CostZonesHelper(int my_id, box *b, int work, direction dir);


void
CostZones (int my_id)
{
   int i;
   box *b;
  
   PartitionIterate(my_id, ComputeSubTreeCosts, BOTTOM);
   BARRIER(G_Memory->synch, Number_Of_Processors);
   Local[my_id].Total_Work = Grid->subtree_cost;
   Local[my_id].Min_Work = ((Local[my_id].Total_Work / Number_Of_Processors)
			   * my_id);
   if (my_id == (Number_Of_Processors - 1))
      Local[my_id].Max_Work = Local[my_id].Total_Work;
   else
      Local[my_id].Max_Work = (Local[my_id].Min_Work
			      + (Local[my_id].Total_Work
				 / Number_Of_Processors));
   InitPartition(my_id);
   CostZonesHelper(my_id, Grid, 0, RIGHT);
   BARRIER(G_Memory->synch, Number_Of_Processors);
}


void
ComputeSubTreeCosts (int my_id, box *b)
{
   box *pb;
   box *sb;
   int i;
   box *cb;

   if (b->type == PARENT) {
      /* Begin wait in a synchronization condition */
      asm volatile ("jal sesc_change_epoch\njal sesc_acquire_begin":::"ra");
      if(b->interaction_synch != b->num_children)
	tls_acquire_retry();
      asm volatile ("jal sesc_acquire_end":::"ra");
      /* End of wait in a synchronization operation */
      /* Replaced code:
      while (b->interaction_synch != b->num_children) {
      }
      */
   }
   b->interaction_synch = 0;
   ComputeCostOfBox(my_id, b);
   b->subtree_cost += b->cost;
   pb = b->parent;
   if (pb != NULL) {
      /* Begin atomic synchronization operation */
      asm volatile ("jal sesc_change_epoch\njal sesc_acquire_begin":::"ra");
      /* Replaced code:
      ALOCK(G_Memory->lock_array, pb->exp_lock_index);
      */
      pb->subtree_cost += b->subtree_cost;
      pb->interaction_synch += 1;
      /* Replaced code:
      AULOCK(G_Memory->lock_array, pb->exp_lock_index);
      */
      asm volatile ("jal sesc_acquire_end\njal sesc_change_epoch":::"ra");
      /* End atomic synchronization operation */
   }
}


void
CostZonesHelper (int my_id, box *b, int work, direction dir)
{
   box *cb;
   int i;
   int parent_cost;
   int *next_child;
   int *child_dir;

   if (b->type == CHILDLESS) {
      if (work >= Local[my_id].Min_Work)
	 InsertBoxInPartition(my_id, b);
   }
   else {
      next_child = Child_Sequence[dir];
      child_dir = Direction_Sequence[dir];
      for (i = 0; (i < NUM_OFFSPRING) && (work < Local[my_id].Max_Work);
	   i++) {
	 cb = b->children[next_child[i]];
	 if (cb != NULL) {
	    if ((work + cb->subtree_cost) >= Local[my_id].Min_Work)
	       CostZonesHelper(my_id, cb, work, child_dir[i]);
	    work += cb->subtree_cost;
	 }
	 if (i == 2) {
	    if ((work >= Local[my_id].Min_Work)
		&& (work < Local[my_id].Max_Work))
	       InsertBoxInPartition(my_id, b);
	    work += b->cost;
	 }
      }
   }

}


#undef DOWN
#undef UP
#undef LEFT
#undef RIGHT
#undef NUM_DIRECTIONS

