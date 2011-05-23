#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <assert.h>

#include "mutable-dawg.h"

//
// 
//

#define WORD_BUFF_SIZE WORD_LIMIT + 1

#define set0(X) memset(X, 0, sizeof(X))

#define nodes_are_equal(a, b) (\
a->hashcode == b->hashcode && \
a->value == b->value && \
a->is_word == b->is_word && \
memcmp(a->children, b->children, LETTER_COUNT * (int) sizeof(void*)) == 0)

void _calculate_hashcode(struct node * node) {
	int hash = node->value ^ (node->value << 5) ^ (node->value << 10) ^ (node->value << 15) ^ (node->value << 20) ^ (node->value << 25);
	hash += node->is_word;
	
	int * children_as_ints = (int*) node->children;
	int len = (sizeof(void*) * LETTER_COUNT) / sizeof(int); // number of integers worth of data in node.children
	for (int i=0; i<len; i++) {
		int val = children_as_ints[i];
		hash ^= val;
		hash = (hash << 5) | ((hash & 0xF8000000) >> 27); // 5 bit circular shift
	}
	
	node->hashcode = hash;
}

// mark every node->visited in a dawg or trie as 0
void unvisit_all_nodes(struct node * root) {
	root->visited = 0;
	for (int i=0; i<LETTER_COUNT; i++) {
		if (root->children[i]) {
			unvisit_all_nodes(root->children[i]);
		}
	}
}

// variables used in the creation of a dawg (using this instead of global variables makes the
// functions in this file reentrant and thread safe)
struct _dawg_context {
	
	int node_count;
	int edge_count;
	
	// counts[X] stores count of nodes with leaf_distance == X
	int counts[WORD_LIMIT];
	// offset[X] stores i+1 where i is the position in 'nodes' of the last node with leaf_distance == X
	int offsets[WORD_LIMIT];
	struct node ** nodes;
};

struct node * _new_node(unsigned char value, struct node * parent, struct _dawg_context * context) {
	struct node * n = calloc(1, sizeof(struct node));
	n->id = context->node_count++;
	n->value = value;
	n->trie_parent = parent;
	return n;
}

void _add_word_to_dawg(struct node * root, char * word, char * last_word, struct _dawg_context * context) {
	if (last_word[0] && strcmp(word, last_word) < 0) {
		fprintf(stderr, "Fatal error: words out of alphabetical order: \"%s\" then \"%s\"\n", last_word, word);
		exit(1);
	}
	int len = strlen(word);
	if (root->leaf_distance < len) {
		root->leaf_distance = len;
	}
	struct node * node = root;
	for (int i=0; i<len; i++) {
		int index = char_to_index(word[i]);
		if (!node->children[index]) {
			node->children[index] = _new_node(index, node, context);
			node->child_count++;
			context->edge_count++;
		}
		node = node->children[index];
		if (node->leaf_distance < len - i - 1) {
			node->leaf_distance = len - i - 1;
		}
	}
	node->is_word = 1;
	strcpy(last_word, word);
}

void _count_nodes_by_leaf_distance(struct node * node, struct _dawg_context * context) {
	if (node->id != 0) {
		context->counts[node->leaf_distance] ++;
	}
	_calculate_hashcode(node);
	if (node->child_count > 0) {
		for (int i=0; i<LETTER_COUNT; i++) {
			if (node->children[i]) {
				_count_nodes_by_leaf_distance(node->children[i], context);
			}
		}
	}
}
void _collect_nodes_by_leaf_distance(struct node * node, struct _dawg_context * context) {
	if (node->id != 0) {
		int relative = context->counts[node->leaf_distance] ++;
		int base = node->leaf_distance == 0 ? 0 : context->offsets[node->leaf_distance - 1];
		int pos = relative + base;
		assert(pos <= context->node_count);
		context->nodes[base + relative] = node;
	}
	if (node->child_count > 0) {
		for (int i=0; i<LETTER_COUNT; i++) {
			if (node->children[i]) {
				_collect_nodes_by_leaf_distance(node->children[i], context);
			}
		}
	}
}

// renumbering nodes in traversal order ensures that the node id is equal to the index
// of the node in the final CDAWG
void _renumber_nodes_in_cdawg_order(struct node * node, struct _dawg_context * context) {
	node->visited = 1;
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->children[i]) {
			context->edge_count++;
			if (!node->children[i]->visited) {
				node->children[i]->id = context->node_count++;
			}
		}
	}
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->children[i] && !node->children[i]->visited) {
			_renumber_nodes_in_cdawg_order(node->children[i], context);
		}
	}
}

// Convert a trie to a dawg and return the nuber of nodes eliminated
// Summary of process:
// 1. First take the set of all leaf nodes
// 2. Within this set, find sets of identical nodes. Two nodes are considered
//    identical if they have the same value and the same set of child nodes
// 3. Merge sets of identical nodes into one node by nominating one node as the
//    sole node, then rewiring the parents of the other nodes to point to the sole node
// 4. Repeat this process with nodes 1 level up from leaves, then 2 levels up etc until
//    the root is reached
int _convert_trie_to_dawg(struct node * root, struct _dawg_context * context) {
	
	// sort nodes by distance from leaf
	struct node ** nodes_by_depth = calloc(context->node_count, sizeof(struct node*));
	set0(nodes_by_depth);
	context->nodes = nodes_by_depth;
	
	_count_nodes_by_leaf_distance(root, context);
	int offset = 0;
	for (int i=0; i<WORD_LIMIT; i++) {
		offset += context->counts[i];
		context->offsets[i] = offset;
		context->counts[i] = 0;
	}
	_collect_nodes_by_leaf_distance(root, context);
	assert(context->offsets[15] == context->node_count - 1); // -1 because we don't collect the root node
	
	int table_size = context->node_count * 1.5;
	struct node ** hash_table = calloc(table_size, sizeof(struct node*));
	
	int merged = 0;
	
	// apply merging process
	for (int i=0; i<WORD_LIMIT; i++) {
		int from = i == 0 ? 0 : context->offsets[i-1];
		int to = context->offsets[i];
		for (int j=from; j<to; j++) {
			struct node * node = context->nodes[j];
			
			// find node sole node, if one exists already
			struct node * sole_node = 0;
			int pos = node->hashcode % table_size;
			int tmp = 0;
			while (hash_table[pos]) {
				if (nodes_are_equal(node, hash_table[pos])) {
					sole_node = hash_table[pos];
					break;
				}
				pos = (pos + 1) % table_size;
				tmp++;
			}
			if (sole_node) {
				// merge this node with the sole node
				assert(node != sole_node);
				assert(node->trie_parent->children[node->value] == node);
				node->trie_parent->children[node->value] = sole_node;
				_calculate_hashcode(node->trie_parent);
				free(node);
				merged++;
			} else {
				// consider this the sole node
				hash_table[pos] = node;
			}
		}
	}
	
	context->node_count = 1;
	context->edge_count = 1;
	_renumber_nodes_in_cdawg_order(root, context);
	
	free(hash_table);
	free(nodes_by_depth);
	context->nodes = 0;
	return merged;
}

struct dawg * dawg_from_word_file(FILE *dict) {
	assert(dict);
	
	struct _dawg_context context;
	memset(&context, 0, sizeof(context));
	
	// read file line by line, adding words into a trie
	char last_word[WORD_BUFF_SIZE];
	set0(last_word);
	char word[256];
	set0(word);
	
	struct node * root = _new_node(0, 0, &context);
	int lineNo = 0;
	while (fgets(word, sizeof(word), dict)) {
		if (word[strlen(word) - 1] != '\n' && !feof(dict)) { // word was too long
			fprintf(stderr, "Skipping line %d. Word is longer than %d characters\n", lineNo, WORD_LIMIT);
			do {
				fgets(word, sizeof(word), dict);
			} while (word[strlen(word) - 1] != '\n');
			continue;
		}
		lineNo++;
		for (int i=0; word[i]; i++) {
			if (isspace(word[i])) {
				word[i] = '\0';
			} else if (isupper(word[i])) {
				word[i] = tolower(word[i]);
			} else if (!islower(word[i])) {
				fprintf(stderr, "Skipping line %d: \"%s\". Illegal character '%c' at position %d\n", lineNo, word, word[i], i);
			}
		}
		if (strlen(word) > WORD_LIMIT) {
			fprintf(stderr, "Skipping line %d: \"%s\". Word is longer than %d characters\n", lineNo, word, WORD_LIMIT);
			continue;
		}
		assert(strlen(word) <= 16);
		_add_word_to_dawg(root, word, last_word, &context);
	}
	
	fprintf(stderr, "Created trie with %d vertices and %d edges\n", context.node_count, context.edge_count);
	
	int trie_edge_count = context.edge_count;
	_convert_trie_to_dawg(root, &context);
	
	fprintf(stderr, "Converted to DAWG with %d vertices and %d edges, eliminating %d%% of edges\n", context.node_count, context.edge_count, 100 - ((context.edge_count * 100) / trie_edge_count));
	
	struct dawg * dawg = calloc(1, sizeof(struct dawg *));
	dawg->node_count = context.node_count;
	dawg->root = root;
	
	return dawg;
	
}


//
// WORD FILE GENERATION
//


void _do_word_file_from_dawg(struct node * node, FILE * out, char * acc, int depth) {
	assert(depth < WORD_BUFF_SIZE);
	if (node->is_word) {
		fprintf(out, "%s\n", acc);
	}
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->children[i]) {
			acc[depth] = index_to_char(i);
			_do_word_file_from_dawg(node->children[i], out, acc, depth+1);
		}
		acc[depth] = '\0';
	}
}

void word_file_from_dawg(struct dawg * dawg, FILE * out) {
	char acc[WORD_BUFF_SIZE];
	memset(&acc, 0, sizeof(acc));
	_do_word_file_from_dawg(dawg->root, out, acc, 0);
}


//
// CDAWG FILE GENERATION
//

// write a cdawg file. Each node is a single byte, optionally followed by a variable
// length pointer to the first child.
//
// single byte interpretation
//
// The first byte is as follows (bit 1 == most significant bit):
// bit 1: pointer flag. If 1, this byte is followed by a pointer to the first child.
//        If 0, the first child is directly after this node in the next byte position.
// bit 2: word flag. If 1, this node represents the last character of a word.
// bit 3: last sibling flag. If 1, this node is the last in a set of siblings. If 0
//        there is another sibling after this node.
// bits 4-7: A 5 bit integer containing the value of the
//
// pointer interpretation:
//
// pointers are base 128 bit varints, as used by Google protocol buffers.
// http://code.google.com/apis/protocolbuffers/docs/encoding.html#varints
// A pointer with a value of 0 indicates that the node has no children
//
void _do_write_cdawg(struct node * node, FILE * out, int * counter) {
	assert(node->id == (*counter)++);
	if (node->visited) {
		return;
	}
	node->visited = 1;
	
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->children[i]) {
			//fprintf(out, "\tn%d -> n%d;\n", node->id, node->children[i]->id);
			_do_write_cdawg(node->children[i], out, counter);
		}
	}
}

void _collect_nodes_in_cdawg_order(struct node * node, struct node ** nodes) {
	if (node->visited) return;
	node->visited = 1;
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->children[i] && !node->children[i]->visited) {
			// process
		}
	}
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->children[i] && !node->children[i]->visited) {
			_collect_nodes_in_cdawg_order(node, nodes);
		}
	}
}

// write a CDAWG to a file
void write_cdawg(struct dawg * dawg, FILE * out) {
	unvisit_all_nodes(dawg->root);
	
//	struct node ** nodes = calloc(dawg->node_count, sizeof(struct node *));
	
//	int counter = 0;
//	_do_write_cdawg(dawg->root, out, &counter);
	
//	free(nodes);
}

//
// GRAPH VISUALISATION
//

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
void graphviz_from_node(struct node * root, FILE * out) {
	unvisit_all_nodes(root);
	fprintf(out, "digraph G {\n");
	_do_graphviz_from_dawg(root, out);
	fprintf(out, "}\n");
}





