/*
 *  dawg-file-traversal.h
 *
 *  Functions for traversing binary DAWGs created by dawgc
 */

/*
struct bdawg {
	int length; // number of edges in the binary structure
	int buffer[]; // pointer to the 0th edge
}
*/

#define WORD_BIT 0x80000000
#define LAST_SIBLING_BIT 0x40000000

// takes a binary edge and checks if it is the last sibling
#define is_last_edge(int_edge) (int_edge & LAST_SIBLING_BIT ? 1 : 0)

#define is_word_edge(int_edge) (int_edge & WORD_BIT ? 1 : 0)

#define edge_value(int_edge) ((int_edge >> 24) & 0x1F)

#define edge_offset(int_edge) (int_edge & 0x00FFFFFF)