/*
 *  dawg-viz.h
 * 
 *  Functions for 
 */

#include <stdio.h>

#include "mutable-dawg.h"

// output a graphviz descriptor for a DAWG
void graphviz_from_dawg(struct node * root, FILE * out);