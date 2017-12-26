/*
 * color.h - Data structures and function prototypes for coloring algorithm
 *             to determine register allocation.
 */

#ifndef COLOR_H
#define COLOR_H

struct COL_result {G_table color; G_nodeList spilledWorkList;};
struct COL_result COL_color(G_graph ig, Live_moveList workListMoves, TAB_table moveList, 
	TAB_table usesDefs);

#endif
