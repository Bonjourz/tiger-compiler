#ifndef LIVENESS_H
#define LIVENESS_H
#include "table.h"
typedef struct Live_move_* Live_move;
struct Live_move_{
	G_node src, dst;
	AS_instr i;
};

typedef struct Live_moveList_ *Live_moveList;
struct Live_moveList_ {
	Live_move head;
	Live_moveList tail;
};

Live_moveList Live_MoveList(Live_move head, Live_moveList tail);
Live_move Live_Move(G_node src, G_node dst, AS_instr i);
bool L_inLiveMoveList(Live_move lm, Live_moveList lml);
Live_moveList L_subLiveMoveList(Live_move lm, Live_moveList lml);
Live_moveList L_union(Live_moveList a, Live_moveList b);


struct Live_graph {
	G_graph graph;
	Live_moveList workListMoves;
	TAB_table moveList;
	TAB_table usesDefs;
	TAB_table tempToNode;
	int **adjMatrix;
};
Temp_temp Live_gtemp(G_node n);

struct Live_graph Live_liveness(G_graph flow);

#endif
