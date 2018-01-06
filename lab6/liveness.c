#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "liveness.h"
#include "table.h"

static Live_moveList workListMoves = NULL;
static TAB_table moveList = NULL;	/* Map the from node to move instr related to it */ 
static TAB_table usesDefs = NULL;	/* number of times of temp def and use */
static TAB_table tempToNode = NULL; /* Map from temp to corresponding graph node */
static Temp_tempList allTemp = NULL;
static int** adjMatrix = NULL;

Live_moveList Live_MoveList(Live_move head, Live_moveList tail) {
	Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
	lm->head = head;
	lm->tail = tail;
	return lm;
}

Live_move Live_Move(G_node src, G_node dst, AS_instr i) {
	Live_move lm = (Live_move) checked_malloc(sizeof(*lm));
	lm->src = src;
	lm->dst = dst;
	lm->i = i;
	return lm;
}

bool L_inLiveMoveList(Live_move lm, Live_moveList lml) {
	for (; lml; lml = lml->tail) {
		if (lm->src == lml->head->src &&
			lm->dst == lml->head->dst &&
			lm->i == lml->head->i)
			return TRUE;
	}
	return FALSE;
}

Live_moveList L_subLiveMoveList(Live_move lm, Live_moveList lml) {
	if (!lml)
		return NULL;

	Live_moveList result = Live_MoveList(NULL, lml);
	lml = result;	
	while (lml->tail) {
		if (lml->tail->head->src == lm->src &&
			lml->tail->head->dst == lm->dst &&
			lml->tail->head->i == lm->i) { 
			lml->tail = lml->tail->tail;
			continue;
		}

		lml = lml->tail;
	}
	return result->tail;
}

Live_moveList L_union(Live_moveList a, Live_moveList b) {
	Live_moveList head = NULL, tail = NULL;
	for (; a; a = a->tail) {
		if (!head) {
			head = Live_MoveList(a->head, NULL);
			tail = head;
		}
		else {
			tail->tail = Live_MoveList(a->head, NULL);
			tail = tail->tail;
		}
	}
	for (; b; b = b->tail) {
		if (!L_inLiveMoveList(b->head, a)) {
			if (!head) {
				head = Live_MoveList(b->head, NULL);
				tail = head;
			}
			else {
				tail->tail = Live_MoveList(b->head, NULL);
				tail = tail->tail;
			}
		}
	} 
	return head;
}


static void enterLiveMap(G_table t, G_node flowNode, Temp_tempList temps) {
	G_enter(t, flowNode, temps);
}

static Temp_tempList lookupLiveMap(G_table t, G_node flowNode) {
	return (Temp_tempList)G_look(t, flowNode);
}

Temp_temp Live_gtemp(G_node n) {
	return (Temp_temp)G_nodeInfo(n);
}

static Temp_tempList newTempList(Temp_tempList l) {
	if (!l)
		return NULL;

	else if (l->tail == NULL) {
		Temp_tempList r = (Temp_tempList)checked_malloc(sizeof(*r));
		r->head = l->head;
		r->tail = l->tail;
		return r;
	}

	else {
		Temp_tempList t = newTempList(l->tail);
		Temp_tempList r = (Temp_tempList)checked_malloc(sizeof(*r));
		r->head = l->head;
		r->tail = l->tail;
		return r;
	}
}

static G_nodeList reverseNode(G_nodeList l) {
	if (!l)
		return NULL;
	G_nodeList r = G_NodeList(l->head, NULL);
	l = l->tail;
	for (; l; l = l->tail)
		r = G_NodeList(l->head, r);

	return r;
}

static Temp_tempList calIn(G_node n, Temp_tempList out) {
	Temp_tempList use = FG_use(n);
	Temp_tempList def = FG_def(n);
	Temp_tempList tmp = sub(out, def);
	Temp_tempList r = unionTempList(use, tmp);
	return r;
}

static Temp_tempList calOut(G_nodeList succ, G_table in_table) {
	Temp_tempList tl = NULL;
	G_nodeList l = NULL;
	for (l = succ; l; l = l->tail) {
		Temp_tempList outl = (Temp_tempList)lookupLiveMap(in_table, l->head);
		tl = unionTempList(tl, outl);
	}

	return tl;
}

static Temp_tempList constructTempGraph(G_graph flow, G_graph g, TAB_table tempToNode) {
	Temp_tempList allTemp = Temp_TempList(F_EAX(),
							Temp_TempList(F_EBX(),
							Temp_TempList(F_ECX(),
							Temp_TempList(F_EDX(),
							Temp_TempList(F_EDI(),
							Temp_TempList(F_ESI(),
							Temp_TempList(F_EBP(),
							Temp_TempList(F_ESP(), NULL))))))));
	Temp_tempList tl = NULL;
	for (tl = allTemp; tl; tl = tl->tail) {
		int* num = (int *)checked_malloc(sizeof(*num));
		*num = 0;
		TAB_enter(usesDefs, tl->head, num);

		TAB_enter(tempToNode, tl->head, G_Node(g, tl->head));
	}

	G_nodeList nl = G_nodes(flow);
	for (; nl; nl = nl->tail) {
		Temp_tempList def = FG_def(nl->head);
		Temp_tempList use = FG_use(nl->head);
		for (tl = def; tl; tl = tl->tail) {
			if (!findTempFromList(tl->head, allTemp)) {
				int* num = (int *)checked_malloc(sizeof(*num));
				*num = 0;
				TAB_enter(usesDefs, tl->head, num);

				TAB_enter(tempToNode, tl->head, G_Node(g, tl->head));
				allTemp = Temp_TempList(tl->head, allTemp);
			}
		}
		for (tl = use; tl; tl = tl->tail) {
			if (!findTempFromList(tl->head, allTemp)) {
				int* num = (int *)checked_malloc(sizeof(*num));
				*num = 0;
				TAB_enter(usesDefs, tl->head, num);

				TAB_enter(tempToNode, tl->head, G_Node(g, tl->head));
				allTemp = Temp_TempList(tl->head, allTemp);
			}
		}
	}
	return allTemp;
}

void addToMoveList(G_node use, G_node def, AS_instr i) {
	Live_move lm = Live_Move(use, def, i);
	workListMoves = Live_MoveList(lm, workListMoves);

	Live_moveList lmlu = TAB_look(moveList, use);
	lmlu = Live_MoveList(lm, lmlu);
	TAB_enter(moveList, use, lmlu);

	Live_moveList lmld = TAB_look(moveList, def);
	lmld = Live_MoveList(lm, lmld);
	TAB_enter(moveList, def, lmld);
}

static void initMatrix(G_graph interferenceGraph) {
	int count = 0;
	G_nodeList nl;
	for (nl = G_nodes(interferenceGraph); nl; nl = nl->tail)
		count++;
	
	//printf("count: %d\n", count);
	int i;
	adjMatrix = (int**)checked_malloc(sizeof(int *) * count);
	
	for (i = 0; i < count; i++) {
		adjMatrix[i] = (int*)checked_malloc(sizeof(int) * count);
		int j;
		
		for (j = 0; j < count; j++)
			adjMatrix[i][j] = 0;
	}
}

static G_graph makeInterferenceGraph(G_graph flow, G_table out_table) {
	G_graph interferenceGraph = G_Graph();

	Temp_tempList allTemp = constructTempGraph(flow, interferenceGraph, tempToNode);
	initMatrix(interferenceGraph);
	Temp_tempList tl = NULL;
	
	G_nodeList nl = G_nodes(flow);
	for (; nl; nl = nl->tail) {
		Temp_tempList liveTemp = (Temp_tempList)G_look(out_table, nl->head);
		AS_instr i = (AS_instr)G_nodeInfo(nl->head);		

		if (i->kind == I_MOVE) {
			Temp_tempList defl = FG_def(nl->head);
			Temp_tempList usel = FG_use(nl->head);
			for (tl = defl; tl; tl = tl->tail) {
				int* num = TAB_look(usesDefs, tl->head);
				*num = *num + 1;
			}
			for (tl = usel; tl; tl = tl->tail) {
				int* num = TAB_look(usesDefs, tl->head);
				*num = *num + 1; 
			}

			Temp_temp def = FG_def(nl->head)->head;
			Temp_temp use = FG_use(nl->head)->head;
			G_node defn = (G_node)TAB_look(tempToNode, def);
			G_node usen = (G_node)TAB_look(tempToNode, use);
			assert(use != def);
			addToMoveList(usen, defn, i);
			for (tl = liveTemp; tl; tl = tl->tail) {
				if (tl->head != use) {
					G_node node = (G_node)TAB_look(tempToNode, tl->head);
					if(defn != node) {
						G_addEdge(defn, node);
						//printf("%d, %d\n",G_key(defn),G_key(node));
						adjMatrix[G_key(defn)][G_key(node)] = 1;
						adjMatrix[G_key(node)][G_key(defn)] = 1;
					}
				}
			}
		}

		else if (i->kind == I_OPER) {
			Temp_tempList defl = FG_def(nl->head);
			Temp_tempList usel = FG_use(nl->head);
			for (tl = defl; tl; tl = tl->tail) {
				int* num = TAB_look(usesDefs, tl->head);
				*num = *num + 1; 
			}
			for (tl = usel; tl; tl = tl->tail) {
				int* num = TAB_look(usesDefs, tl->head);
				*num = *num + 1; 
			}

			for (; defl; defl = defl->tail) {
				G_node defn = (G_node)TAB_look(tempToNode, defl->head);
				for (tl = liveTemp; tl; tl = tl->tail) {
					G_node node = (G_node)TAB_look(tempToNode, tl->head);
					if(defn != node) {
						G_addEdge(defn, node);
						adjMatrix[G_key(defn)][G_key(node)] = 1;
						adjMatrix[G_key(node)][G_key(defn)] = 1;
					}
				}
			}
		}
	}

	return interferenceGraph;
}

G_table constructOutTable(G_graph flow) {
	G_table in_table = G_empty();
	G_table out_table = G_empty();
	G_nodeList nodes = reverseNode(G_nodes(flow));
	G_nodeList l = NULL;
	for (l = nodes; l; l = l->tail) {
		enterLiveMap(in_table, l->head, NULL);
		enterLiveMap(out_table, l->head, NULL);
	}

	bool finish = FALSE;
	while (!finish) {
		finish = TRUE;
		for (l = nodes; l; l = l->tail) {
			Temp_tempList in = (Temp_tempList)lookupLiveMap(in_table, l->head);
			Temp_tempList out = (Temp_tempList)lookupLiveMap(out_table, l->head);
			Temp_tempList in_n = calIn(l->head, out);
			Temp_tempList out_n = calOut(G_succ(l->head), in_table);
			if (!same(in, in_n)) {
				enterLiveMap(in_table, l->head, in_n);
				finish = FALSE;
			}
			if (!same(out, out_n)) {
				enterLiveMap(out_table, l->head, out_n);
				finish = FALSE;
			}
		}
	}
	return out_table;
}

struct Live_graph Live_liveness(G_graph flow) {
	workListMoves = NULL;
	adjMatrix = NULL;
	moveList = TAB_empty();
	usesDefs = TAB_empty();
	tempToNode = TAB_empty();
	allTemp = NULL;

	G_table out_table = constructOutTable(flow);
	G_graph interferenceGraph = makeInterferenceGraph(flow, out_table);
	struct Live_graph lg;
	lg.graph = interferenceGraph;
	lg.workListMoves = workListMoves;
	lg.moveList = moveList;
	lg.tempToNode = tempToNode;
	lg.usesDefs = usesDefs;
	lg.adjMatrix = adjMatrix;
	return lg;
}


