#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "liveness.h"
#include "color.h"
#include "regalloc.h"
#include "table.h"
#include "flowgraph.h"


#define STRLEN 40

static AS_instrList rewriteProgram(F_frame f, AS_instrList il, G_nodeList spilledWorkList);
static void generateColorMap(Temp_map colorMap, G_graph ng, G_table color);
static AS_instrList deleteMoves(AS_instrList il, G_table color, TAB_table tempToNode);


static Temp_tempList L(Temp_temp h, Temp_tempList t) {
	return Temp_TempList(h, t);
}

static AS_instrList rewriteProgram(F_frame f, AS_instrList il, G_nodeList spilledWorkList) {
	//assert(spilledWorkList);
	Temp_temp targetTemp = (Temp_temp)G_nodeInfo(spilledWorkList->head);

	int off = F_makeSpill(f);
	char *out1 = (char *)checked_malloc(STRLEN);
	char *out2 = (char *)checked_malloc(STRLEN);
	sprintf(out1, "movl `s0, %d(`s1)", -off * 4);		// store
	sprintf(out2, "movl %d(`s0), `d0", -off * 4);		// fetch
	AS_instrList r = AS_InstrList(NULL, il);			// The first is dummy
	il = r;
	while(il->tail) {
		AS_instr i = il->tail->head;

		if (i->kind == I_OPER) {
			Temp_tempList dstl = i->u.OPER.dst;
			Temp_tempList srcl = i->u.OPER.src;
			if (findTempFromList(targetTemp, dstl) && findTempFromList(targetTemp, srcl)) {
				Temp_temp newTemp = Temp_newtemp();
				for (; dstl; dstl = dstl->tail) {
					if (dstl->head == targetTemp)
						dstl->head = newTemp;
				}
				for (; srcl; srcl = srcl->tail) {
					if (srcl->head == targetTemp)
						srcl->head = newTemp;
				}
				AS_instrList newif = AS_InstrList(AS_Oper(out2, L(newTemp, NULL), L(F_FP(), NULL), NULL),
					il->tail);
				AS_instrList newie = AS_InstrList(AS_Oper(out1, NULL, L(newTemp, L(F_FP(), NULL)), NULL),
					il->tail->tail);
				il->tail = newif;
				il->tail->tail->tail = newie;
				continue;
			}

			else if (findTempFromList(targetTemp, dstl)) {
				Temp_temp newTemp = Temp_newtemp();
				for (; dstl; dstl = dstl->tail) {
					if (dstl->head == targetTemp)
						dstl->head = newTemp;
				}
				//store
				AS_instr newi = AS_Oper(out1, NULL, L(newTemp, L(F_FP(), NULL)), NULL);
				AS_instrList nl = AS_InstrList(newi, il->tail->tail);
				il->tail->tail = nl;
				continue;
			}

			else if (findTempFromList(targetTemp, srcl)) {
				// extract the targetTemp from the srcl
				Temp_temp newTemp = Temp_newtemp();
				for (; srcl; srcl = srcl->tail) {
					if (srcl->head == targetTemp)
						srcl->head = newTemp;
				}
				AS_instr newi = AS_Oper(out2, L(newTemp, NULL), L(F_FP(), NULL), NULL);
				AS_instrList nl = AS_InstrList(newi, il->tail);
				il->tail = nl;
				continue;
			}
		}

		else if (i->kind == I_MOVE) {
			Temp_tempList dstl = i->u.OPER.dst;
			Temp_tempList srcl = i->u.OPER.src;
			if (findTempFromList(targetTemp, dstl)) {
				AS_instr newi = AS_Oper(out1, NULL, L(srcl->head, L(F_FP(), NULL)), NULL);
				il->tail->head = newi;
				continue;
			}

			else if (findTempFromList(targetTemp, srcl)) {
				// fetch
				AS_instr newi = AS_Oper(out2, dstl, L(F_FP(), NULL), NULL);
				il->tail->head = newi;
				continue;
			}
		}

		il = il->tail;
	}

	return r->tail;
}

static void generateColorMap(Temp_map colorMap, G_graph ng, G_table color) {
	G_nodeList nl = G_nodes(ng);
	for (; nl; nl = nl->tail) {
		Temp_temp temp = (Temp_temp)G_nodeInfo(nl->head);
		int* colorn = G_look(color, nl->head);
		switch(*colorn) {
			case 0: Temp_enter(colorMap, temp, "%eax"); break;
			case 1: Temp_enter(colorMap, temp, "%ebx"); break;
			case 2: Temp_enter(colorMap, temp, "%ecx"); break;
			case 3: Temp_enter(colorMap, temp, "%edx"); break;
			case 4: Temp_enter(colorMap, temp, "%edi"); break;
			case 5: Temp_enter(colorMap, temp, "%esi"); break;
			case 6:	Temp_enter(colorMap, temp, "%ebp"); break;
			case 7: Temp_enter(colorMap, temp, "%esp"); break;
			default: assert(0);
		}
	}
}

static AS_instrList deleteMoves(AS_instrList il, G_table color, TAB_table tempToNode) {
	AS_instrList result = AS_InstrList(NULL, il);
	il = result;
	while(il->tail) {
		AS_instr i = il->tail->head;
		if (i->kind == I_MOVE) {
			G_node usen = TAB_look(tempToNode, i->u.MOVE.src->head);
			G_node defn = TAB_look(tempToNode, i->u.MOVE.dst->head);
			int *colorn1 = G_look(color, usen);
			int *colorn2 = G_look(color, defn);

			if (*colorn1 == *colorn2) {
				il->tail = il->tail->tail;
				continue;
			}
		}
		il = il->tail;
	}
	return result->tail;
}

struct RA_result RA_regAlloc(F_frame f, AS_instrList ilIn) {
	AS_instrList il = ilIn;
	bool done = FALSE;
	struct Live_graph lg;
	struct COL_result col_ret;


	while (!done) {
		/*Temp_map m = Temp_empty();
		initTempMap(m);
		AS_printInstrList (stdout, ilIn,
                       Temp_layerMap(m, Temp_name()));*/
		G_graph flow = FG_AssemFlowGraph(il, f);
		lg = Live_liveness(flow);
		col_ret = COL_color(lg.graph, lg.workListMoves, lg.moveList, lg.usesDefs, lg.adjMatrix);
		
		if (col_ret.spilledWorkList) {
			//while(col_ret.spilledWorkList) {
				il = rewriteProgram(f, il, col_ret.spilledWorkList);
				//col_ret.spilledWorkList = col_ret.spilledWorkList->tail;
			//}
		}
		
		else
			done = TRUE;
	}

	Temp_map colorMap = Temp_empty();
	generateColorMap(colorMap, lg.graph, col_ret.color);

	il = deleteMoves(il, col_ret.color, lg.tempToNode);
	struct RA_result ret;
	ret.coloring = colorMap;
	ret.il = il;
	return ret;
}
