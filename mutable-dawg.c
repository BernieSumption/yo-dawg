#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <assert.h>

#include "mutable-dawg.h"
#include "dawg-file-traversal.h"


#define WORD_BUFF_SIZE WORD_LIMIT + 1

#define set0(X) memset(X, 0, sizeof(X))

#define nodes_are_equal(a, b) (\
a->hashcode == b->hashcode && \
a->value == b->value && \
a->is_word == b->is_word && \
memcmp(a->edges, b->edges, LETTER_COUNT * (int) sizeof(void*)) == 0)

void _calculate_hashcode(struct vertex * node) {
	int hash = node->value ^ (node->value << 5) ^ (node->value << 10) ^ (node->value << 15) ^ (node->value << 20) ^ (node->value << 25);
	hash += node->is_word;
	
	int * children_as_ints = (int*) node->edges;
	int len = (sizeof(void*) * LETTER_COUNT) / sizeof(int); // number of integers worth of data in node.children
	for (int i=0; i<len; i++) {
		int val = children_as_ints[i];
		hash ^= val;
		hash = (hash << 5) | ((hash & 0xF8000000) >> 27); // 5 bit circular shift
	}
	
	node->hashcode = hash;
}

// mark every node->visited in a dawg or trie as 0
void unvisit_all_nodes(struct vertex * root) {
	root->visited = 0;
	for (int i=0; i<LETTER_COUNT; i++) {
		if (root->edges[i]) {
			unvisit_all_nodes(root->edges[i]);
		}
	}
}

// variables used in the creation of a dawg (using this instead of global variables makes the
// functions in this file reentrant and thread safe)
struct _dawg_context {
	
	int vertex_count;
	int edge_count;
	
	// counts[X] stores count of nodes with leaf_distance == X
	int counts[WORD_LIMIT];
	// offset[X] stores i+1 where i is the position in 'nodes' of the last node with leaf_distance == X
	int offsets[WORD_LIMIT];
	struct vertex ** nodes;
};

struct vertex * _new_node(unsigned char value, struct vertex * parent, struct _dawg_context * context) {
	struct vertex * n = calloc(1, sizeof(struct vertex));
	n->id = context->vertex_count++;
	n->value = value;
	n->trie_parent = parent;
	return n;
}

void _add_word_to_dawg(struct vertex * root, char * word, char * last_word, struct _dawg_context * context) {
	if (last_word[0] && strcmp(word, last_word) < 0) {
		fprintf(stderr, "Fatal error: words out of alphabetical order: \"%s\" then \"%s\"\n", last_word, word);
		exit(1);
	}
	int len = strlen(word);
	if (root->leaf_distance < len) {
		root->leaf_distance = len;
	}
	struct vertex * node = root;
	for (int i=0; i<len; i++) {
		int index = char_to_index(word[i]);
		if (!node->edges[index]) {
			node->edges[index] = _new_node(index, node, context);
			node->edge_count++;
		}
		node = node->edges[index];
		if (node->leaf_distance < len - i - 1) {
			node->leaf_distance = len - i - 1;
		}
	}
	node->is_word = 1;
	strcpy(last_word, word);
}

void _count_nodes_by_leaf_distance(struct vertex * node, struct _dawg_context * context) {
	if (node->id != 0) {
		context->counts[node->leaf_distance] ++;
	}
	_calculate_hashcode(node);
	if (node->edge_count > 0) {
		for (int i=0; i<LETTER_COUNT; i++) {
			if (node->edges[i]) {
				_count_nodes_by_leaf_distance(node->edges[i], context);
			}
		}
	}
}
void _collect_nodes_by_leaf_distance(struct vertex * node, struct _dawg_context * context) {
	if (node->id != 0) {
		int relative = context->counts[node->leaf_distance] ++;
		int base = node->leaf_distance == 0 ? 0 : context->offsets[node->leaf_distance - 1];
		int pos = relative + base;
		assert(pos <= context->vertex_count);
		context->nodes[base + relative] = node;
	}
	if (node->edge_count > 0) {
		for (int i=0; i<LETTER_COUNT; i++) {
			if (node->edges[i]) {
				_collect_nodes_by_leaf_distance(node->edges[i], context);
			}
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
int _convert_trie_to_dawg(struct vertex * root, struct _dawg_context * context) {
	
	// sort nodes by distance from leaf
	struct vertex ** nodes_by_depth = calloc(context->vertex_count, sizeof(struct vertex*));
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
	assert(context->offsets[15] == context->vertex_count - 1); // -1 because we don't collect the root node
	
	int table_size = context->vertex_count * 1.5;
	struct vertex ** hash_table = calloc(table_size, sizeof(struct vertex*));
	
	int merged = 0;
	
	// apply merging process
	for (int i=0; i<WORD_LIMIT; i++) {
		int from = i == 0 ? 0 : context->offsets[i-1];
		int to = context->offsets[i];
		for (int j=from; j<to; j++) {
			struct vertex * node = context->nodes[j];
			
			// find node sole node, if one exists already
			struct vertex * sole_node = 0;
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
				assert(node->trie_parent->edges[node->value] == node);
				node->trie_parent->edges[node->value] = sole_node;
				_calculate_hashcode(node->trie_parent);
				free(node);
				context->nodes[j] = 0;
				merged++;
			} else {
				// consider this the sole node
				hash_table[pos] = node;
			}
		}
	}
	
	// renumber vertices with ids that decease further away from leaves. This ensures
	// that no vertex will have an ID higher than any of its parents
	int original_vertex_count = context->vertex_count;
	context->vertex_count = 1;
	context->edge_count = root->edge_count;
	for (int i=original_vertex_count-1; i>=0; i--) {
		if (context->nodes[i]) {
			context->nodes[i]->id = context->vertex_count++;
			context->edge_count += context->nodes[i]->edge_count;
		}
	}
	
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
	
	struct vertex * root = _new_node(0, 0, &context);
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
	
	fprintf(stderr, "Created trie with %d vertices/edges\n", context.vertex_count);
	
	int trie_node_count = context.vertex_count;
	_convert_trie_to_dawg(root, &context);
	
	fprintf(stderr, "Converted to DAWG with %d vertices (reduction of %d%%) and %d edges (reduction of %d%%)\n",
			context.vertex_count, 100 - ((context.vertex_count * 100) / trie_node_count),
			context.edge_count, 100 - ((context.edge_count * 100) / trie_node_count));
	
	struct dawg * dawg = calloc(1, sizeof(struct dawg *));
	dawg->node_count = context.vertex_count;
	dawg->root = root;
	
	return dawg;
}


//
// WORD FILE GENERATION
//

void _do_print_word_file(struct vertex * node, FILE * out, char * acc, int depth) {
	assert(depth < WORD_BUFF_SIZE);
	if (node->is_word) {
		fprintf(out, "%s\n", acc);
	}
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->edges[i]) {
			acc[depth] = index_to_char(i);
			_do_print_word_file(node->edges[i], out, acc, depth+1);
		}
		acc[depth] = '\0';
	}
}

void print_word_file(struct vertex * root, FILE * out) {
	char acc[WORD_BUFF_SIZE];
	memset(&acc, 0, sizeof(acc));
	_do_print_word_file(root, out, acc, 0);
}

//
// BINARY FILE GENERATION
//

// write a dawg file. Each vertex is represented simply as a list of edges. Each edge is
// a 32 bit integer, with a couple of flags, a value and a pointer to the following vertex.
//
// interpretation
//
// bit 0: word flag. If 1, this node represents the last character of a word.
// bit 1: last sibling flag. If 1, this edge is the last edge in the vertex
// bits 3-7: A 5 bit integer containing the value of the edge
// bits 8-31: a 24 bit integer containing an offset from the start of the binary
//            file to the following vertex, or 0 if the following vertex has no edges
//

void _flatten_vertices(struct vertex * node, struct vertex ** nodes, int node_count) {
	assert(node->id < node_count);
	nodes[node->id] = node;
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->edges[i]) {
			_flatten_vertices(node->edges[i], nodes, node_count);
		}
	}
}

#define MAX_VERTEX_BINARY_SIZE 4


// write a DAWG to a file
void binary_file_from_dawg(struct dawg * dawg, FILE * out, int text) {
	assert(out);
	unvisit_all_nodes(dawg->root);
	
	int node_count = dawg->node_count;
	
	struct vertex ** nodes = calloc(node_count, sizeof(struct vertex *));
	
	_flatten_vertices(dawg->root, nodes, node_count);	
	
	int file_offset = 0;
	for (int i=0; i<node_count; i++) {
		nodes[i]->file_offset = file_offset;
		file_offset += nodes[i]->edge_count;
	}
	
	if (text) fprintf(out, "int DAWG_TABLE[] = {");
	for (int i=0; i<node_count; i++) {
		struct vertex * node = nodes[i];
		assert(node);
		if (node->edge_count == 0) {
			continue;
		}
		if (text) fprintf(out, "\n\t/* DAWG_TABLE[%d], vertex #%d */\n\t", node->file_offset, node->id);
		int children = 0;
		for (int j=0; j<LETTER_COUNT; j++) {
			struct vertex * edge_to = node->edges[j];
			if (edge_to) {
				children++;
				unsigned int edge_int = 0;
				if (edge_to->is_word) {
					edge_int |= WORD_BIT;
				}
				if (children == node->edge_count) {
					edge_int |= LAST_SIBLING_BIT;
				}
				assert(edge_to->value <= 0x1F); // value fits in 5 bits
				edge_int |= edge_to->value << 24; // store value in bits 4-8
				if (edge_to->edge_count) {
					assert(edge_to->file_offset <= 0x00FFFFFF); // offset fits in 24 bits
					edge_int |= edge_to->file_offset; // store offset in bits 9-32;
				}
				if (text) {
					fprintf(out, "0x%08X, ", edge_int);
				} else {
					fwrite(&edge_int, sizeof(edge_int), 1, out);
				}

			}
		}
	}
	if (text) fprintf(out, "\n};");
	
	//	_do_write_cdawg(dawg->root, out, &counter);
	
	free(nodes);
}

//
// BINARY FILE DECOMPILATION
//

// recursive function to add binary nodes into a dawg structure

void _add_binary_node_to_dawg(unsigned int * binary_node, int offset, struct vertex * node, int total_read) {
	unsigned int i=0, edge;
	do {
		assert(offset + i < total_read);
		edge = binary_node[offset + i];
		unsigned char value = edge_value(edge);
		assert(!node->edges[value]);
		struct vertex * new_node = calloc(1, sizeof(struct vertex));
		new_node->value = value;
		new_node->is_word = is_word_edge(edge);
		node->edges[value] = new_node;
		int child_offset = edge_offset(edge);
		if (child_offset) {
			_add_binary_node_to_dawg(binary_node, child_offset, node->edges[value], total_read);
		}
		i++;
	} while (!is_last_edge(edge));
}

struct vertex * trie_from_binary_file(FILE * in) {
	// copy file to buffer
	int buffer_size = 256*256, total_read = 0;
	unsigned int * buffer = malloc(buffer_size * sizeof(unsigned int));
	do {
		total_read += fread(buffer + total_read, sizeof(unsigned int), buffer_size - total_read, in);
		if (total_read == buffer_size) {
			buffer_size *= 2;
			unsigned int * new_buffer = malloc(buffer_size * sizeof(unsigned int));
			memcpy(new_buffer, buffer, total_read * sizeof(unsigned int));
			free(buffer);
			buffer = new_buffer;
		}
	} while (!feof(in));
	/*
	
	unsigned int node_counter = 0, total_nodes;
	struct vertex ** nodes = calloc(total_read, sizeof(struct vertex *));
	struct vertex ** terminals = calloc(LETTER_COUNT, sizeof(struct vertex));
	for (int i=0; i<total_read;) {
		if (!nodes[i]) {
			nodes[i] = calloc(1, sizeof(struct vertex));
		}
		total_nodes++;
		struct vertex * node = nodes[i];
		int edge;
		do {
			edge = buffer[i];
			unsigned int offset = edge_offset(edge);
			if (offset) {
				if (!nodes[offset]) {
					nodes[offset] = calloc(1, sizeof(struct vertex));
					node_counter++;
				}
				node->edges[value] = nodes[offset];
			}
			unsigned char value = edge_value(edge);
			assert(offset > total_nodes);
			nodes[offset]->value = value;
			i++;
			if (i >= total_read) break;
		} while (!is_last_edge(edge));
	}
	
	struct dawg * dawg = calloc(1, sizeof(struct dawg));
	dawg->root = nodes[0];
	dawg->node_count = total_nodes;*/
	
	struct vertex * root = calloc(1, sizeof(struct vertex));

	_add_binary_node_to_dawg(buffer, 0, root, total_read);
		
	return root;
}

//
// GRAPH VISUALISATION
//

void _do_graphviz_from_dawg(struct vertex * node, FILE * out) {
	if (node->visited) {
		return;
	}
	node->visited = 1;
	
	if (node->id == 0) {
		fprintf(out, "\tn%d [label=\"root\", shape=Mdiamond];\n", node->id);
	} else {
		fprintf(out, "\tn%d [label=\"%d: %c%s\"];\n", node->id, node->id, index_to_char(node->value), node->is_word ? " *" : "");
	}
	
	
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->edges[i]) {
			fprintf(out, "\tn%d -> n%d;\n", node->id, node->edges[i]->id);
			_do_graphviz_from_dawg(node->edges[i], out);
		}
	}
}


// output a graphviz descriptor for a DAWG
void graphviz_from_node(struct vertex * root, FILE * out) {
	unvisit_all_nodes(root);
	fprintf(out, "digraph G {\n");
	_do_graphviz_from_dawg(root, out);
	fprintf(out, "}\n");
}





