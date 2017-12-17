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

static Live_moveList worklistMoves = NULL;
static TAB_table moveList = NULL;
static TAB_table usesDefs = NULL;
static TAB_table tempToNode = NULL;


void showOutTable(G_graph flow, TAB_table out) {
	Temp_map map = Temp_empty();
	initTempMap(map);
	printf("showouttablebegin================\n");
	G_nodeList l = G_nodes(flow);
	for (; l; l = l->tail) {
		Temp_tempList tl = G_look(out, l->head);
		AS_instr i = (AS_instr)G_nodeInfo(l->head);
		AS_printInstrList(stdout, AS_InstrList(i, NULL), Temp_layerMap(map, Temp_name()));
		for(; tl; tl = tl->tail)
			printf(" %d", Temp_int(tl->head));

		printf("\n");
	}
	printf("showouttableend================\n");
}



static Temp_tempList allTemp = NULL;

Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
	Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
	lm->src = src;
	lm->dst = dst;
	lm->tail = tail;
	return lm;
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

/* Whether 'tmp' in 'l' or not */

static Temp_tempList calIn(G_node n, Temp_tempList out) {
	Temp_tempList use = FG_use(n);
	Temp_tempList def = FG_def(n);
	Temp_tempList tmp = sub(out, def);
	Temp_tempList r = unionTempList(use, tmp);/*
	if (G_key(n) == 15) {
		Temp_tempList l;
		tmp = sub(out, def);
		r = unionTempList(use, tmp);
		printf("\nuse: ");
		for(; use; use = use->tail)
			printf(" %d", Temp_int(use->head));
		printf("\ndef: ");
		for(; def; def = def->tail)
			printf(" %d", Temp_int(def->head));
		printf("\nout: ");
		for(; out; out = out->tail)
			printf(" %d", Temp_int(out->head));
		printf("\ntmp: ");
		for(l = tmp; l; l = l->tail)
			printf(" %d", Temp_int(l->head));
		printf("\nresult: ");
		for(l = r; l; l = l->tail)
			printf(" %d", Temp_int(l->head));
		printf("\n===============\n");
	}
	else {
		tmp = sub(out, def);
		r = unionTempList(use, tmp);
	}*/
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

void addToMoveList(G_node use, G_node def) {
	/* Empty */
}

static G_graph makeInterferenceGraph(G_graph flow, G_table out_table) {
	G_graph interferenceGraph = G_Graph();
	moveList = TAB_empty();
	tempToNode = TAB_empty();
	usesDefs = TAB_empty();

	Temp_tempList allTemp = constructTempGraph(flow, interferenceGraph, tempToNode);
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
			addToMoveList(usen, defn);	// TO DO : add coalesce
			for (tl = liveTemp; tl; tl = tl->tail) {
				if (tl->head != use) {
					G_node node = (G_node)TAB_look(tempToNode, tl->head);
					if(defn != node)
						G_addEdge(defn, node);
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
					if(defn != node)
						G_addEdge(defn, node);
				}
			}
		}
	}
	// TO DO: Add coalesce function
	return interferenceGraph;
}

struct Live_graph Live_liveness(G_graph flow) {
	G_table in_table = G_empty();
	G_table out_table = G_empty();
	G_nodeList nodes = G_nodes(flow);
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
				/*
				if (G_key(l->head) == 16) {
					Temp_tempList tmp = out_n;
					for (; tmp; tmp = tmp->tail)
						printf(" %d ", Temp_int(tmp->head));
					printf("\n");
				}*/
				enterLiveMap(out_table, l->head, out_n);
				finish = FALSE;
			}
		}
	}

	//TAB_dump(TAB_table t, void (*show)(void *key, void *value))
	//showOutTable(flow, out_table);

	G_graph interferenceGraph = makeInterferenceGraph(flow, out_table);
	struct Live_graph lg;
	lg.graph = interferenceGraph;
	lg.moves = NULL;
	lg.tempToNode = tempToNode;
	lg.usesDefs = usesDefs;
	return lg;
}


