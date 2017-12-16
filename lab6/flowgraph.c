#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "errormsg.h"
#include "table.h"

Temp_tempList FG_def(G_node n) {
	AS_instr in = (AS_instr)G_nodeInfo(n);
	switch(in->kind) {
		case I_OPER:
			return in->u.OPER.dst;

		case I_LABEL:
			return NULL;

		case I_MOVE:
			return in->u.MOVE.dst;
	}
	assert(0);
	return NULL;
}

Temp_tempList FG_use(G_node n) {
	AS_instr in = (AS_instr)G_nodeInfo(n);
	switch(in->kind) {
		case I_OPER:
			return in->u.OPER.src;

		case I_LABEL:
			return NULL;

		case I_MOVE:
			return in->u.MOVE.src;
	}
	assert(0);
	return NULL;
}

bool FG_isMove(G_node n) {
	AS_instr in = (AS_instr)G_nodeInfo(n);
	if (in->kind == I_MOVE)
		return TRUE;
	else
		return FALSE;
}

void Instr_Print(void* input) {
	AS_instr i = (AS_instr)input;
	switch(i->kind) {
		case I_OPER:
			printf("%s", i->u.OPER.assem);
			break;
		case I_MOVE:
			printf("%s", i->u.MOVE.assem);
			break;
		case I_LABEL:
			printf("%s", i->u.LABEL.assem);
			break;
	} 
}

G_graph FG_AssemFlowGraph(AS_instrList il, F_frame f) {
	G_graph g = G_Graph();
	TAB_table table_label = TAB_empty();
	AS_instrList l = NULL;
	for (l = il; l; l = l->tail) {
		AS_instr in = l->head;
		G_node node = G_Node(g, in);
		if (in->kind == I_LABEL) {
			assert(in->u.LABEL.label);
			TAB_enter(table_label, in->u.LABEL.label, node);
		}

	}

	G_nodeList nl = G_nodes(g);
	for (l = il; l && nl; l = l->tail, nl = nl->tail) {
		AS_instr in = l->head;
		G_node node = nl->head;
		if (in->kind == I_OPER && in->u.OPER.jumps != NULL) {
			assert((Temp_tempList)in->u.OPER.jumps->labels);
			Temp_tempList jumps = (Temp_tempList)in->u.OPER.jumps->labels;
			G_node node_dst = (G_node)TAB_look(table_label, jumps->head);
			assert(node_dst);
			G_addEdge(node, node_dst);
			/* for cjump */
			if (jumps->tail) {
				jumps = jumps->tail;
				node_dst = (G_node)TAB_look(table_label, jumps->head);
				assert(node_dst);
				G_addEdge(node, node_dst);
			}
		}

		else {
			if (l->tail)
				G_addEdge(node, nl->tail->head);
		}
	}
	//G_show(stdout, G_nodes(g), Instr_Print);
	return g;
}
