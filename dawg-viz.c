/*
 *  dawg-viz.c
 *  dawg
 *
 *  Created by Bernard Sumption on 22/05/2011.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "dawg-viz.h"
#include "mutable-dawg.h"

void _do_graphviz_from_dawg(struct node * node, FILE * out) {
	if (node->visited) {
		return;
	}
	node->visited = 1;
	
	if (node->id == 0) {
		fprintf(out, "\tn%d [label=\"root\", shape=Mdiamond];\n", node->id);
	} else {
		fprintf(out, "\tn%d [label=\"%c%s\"];\n", node->id, index_to_char(node->value), node->is_word ? " *" : "");
	}

	
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->children[i]) {
			fprintf(out, "\tn%d -> n%d;\n", node->id, node->children[i]->id);
			_do_graphviz_from_dawg(node->children[i], out);
		}
	}
}


// output a graphviz descriptor for a DAWG
void graphviz_from_dawg(struct node * root, FILE * out) {
	unvisit_all_nodes(root);
	fprintf(out, "digraph G {\n");
	_do_graphviz_from_dawg(root, out);
	fprintf(out, "}\n");
	
}

