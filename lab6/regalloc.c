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

#define REG_NUM 6
#define STRLEN 40

static G_nodeList precolored = NULL;
static G_nodeList initial = NULL;
static G_nodeList simplifyWorkList = NULL;
static G_nodeList freezeWorkList = NULL;
static G_nodeList spillWorkList = NULL;
static G_nodeList spilledWorkList = NULL;
static G_nodeList coalescedNodes = NULL;
static G_nodeList coloredNodes = NULL;
static G_nodeList selectStack = NULL;

static G_table degree = NULL;
static G_table color = NULL;

static TAB_table usesDefs = NULL;

static Temp_tempList L(Temp_temp h, Temp_tempList t) {
	return Temp_TempList(h, t);
}

static int getColorFromTemp(Temp_temp temp) {
	if (temp == F_EAX())
		return 0;
	if (temp == F_EBX())
		return 1;
	if (temp == F_ECX())
		return 2;
	if (temp == F_EDX())
		return 3;
	if (temp == F_EDI())
		return 4;
	if (temp == F_ESI())
		return 5;
	if (temp == F_EBP())
		return 6;
	if (temp == F_ESP())
		return 7;

	return -1;
}

static bool isMachineReg(Temp_temp temp) {
	if (temp == F_EAX() |
		temp == F_EBX() |
		temp == F_ECX() |
		temp == F_EDX() |
		temp == F_EDI() |
		temp == F_ESI() |
		temp == F_ESP() |
		temp == F_EBP())
		return TRUE;
	else
		return FALSE;
}

static void build(struct Live_graph lg) {
	usesDefs = lg.usesDefs;
	degree = G_empty();
	color = G_empty();
	//TAB_table tempToNode = lg.tempToNode;
	G_nodeList nl = G_nodes(lg.graph);
	for (; nl; nl = nl->tail) {
		int* degreen = (int*)checked_malloc(sizeof(*degreen));
		int* colorn = (int*)checked_malloc(sizeof(*colorn));
		*colorn = getColorFromTemp((Temp_temp)G_nodeInfo(nl->head));
		//printf("build color %d temp %d\n", *colorn, Temp_int(G_nodeInfo(nl->head)));
		*degreen = G_degree(nl->head);
		G_enter(color, nl->head, colorn);
		G_enter(degree, nl->head, degreen);
		
		if (!isMachineReg((Temp_temp)G_nodeInfo(nl->head))) {
			if ((*degreen) < REG_NUM) {
				assert(!G_inNodeList(nl->head, simplifyWorkList));
				simplifyWorkList = G_NodeList(nl->head, simplifyWorkList);
			}
			else {
				assert(!G_inNodeList(nl->head, spillWorkList));
				spillWorkList = G_NodeList(nl->head, spillWorkList);
			}
		}
	}
}

static void decrementDegree(G_node n) {
	int *degreen = G_look(degree, n);
	*degreen = *degreen - 1;
	G_enter(degree, n, degreen);   // can delete
	if (*degreen == REG_NUM - 1 && !isMachineReg(G_nodeInfo(n))) {
		// TO DO: enableMoves
		spillWorkList =  G_subNodeFromList(n, spillWorkList);
		// TO DO: if moverealted(n) ......
		if (!G_inNodeList(n, simplifyWorkList))
			simplifyWorkList = G_NodeList(n, simplifyWorkList);
	}
}

static void simplify() {
	G_node simNode = simplifyWorkList->head;
	simplifyWorkList = simplifyWorkList->tail;
	assert(!G_inNodeList(simNode, simplifyWorkList));
	//assert(!G_inNodeList(simNode, selectStack));
	//printf("simplify add to select stack: temp %d\n", Temp_int(G_nodeInfo(simNode)));
	if (!G_inNodeList(simNode, selectStack))
		selectStack = G_NodeList(simNode, selectStack);

	G_nodeList nl = G_adj(simNode);
	assert(!G_inNodeList(simNode, nl));
	for (; nl; nl = nl->tail)
		decrementDegree(nl->head);
}

static void assignColors() {
	while(selectStack != NULL) {
		G_node node = selectStack->head;
		/*
		int* c = G_look(color, node);
		printf("assignColors: temp %d color %d\n", Temp_int(G_nodeInfo(node)), *c);
		G_nodeList gnl = selectStack->tail;
		for (; gnl; gnl = gnl->tail) {
			printf(" temp:%d", Temp_int(G_nodeInfo(gnl->head)));
		}
		printf("\n");*/
		selectStack = G_subNodeFromList(node, selectStack);
		assert(!G_inNodeList(node, selectStack));
		
		int colorSet[REG_NUM + 2] = {1, 1, 1, 1, 1, 1, 0, 0};
		G_nodeList adj = G_adj(node);
		for (; adj; adj = adj->tail) {
			int *colorAdj = G_look(color, adj->head);
			// TO DO: move nodes
			if (*colorAdj != -1)
				colorSet[*colorAdj] = 0;
		}
		int colorChoose = -1, i = 0;
		for (; i < 6; i++) {
			if (colorSet[i] == 1) {
				colorChoose = i;
				break;
			}
		}

		if (colorChoose == -1) {
			int* i = G_look(color, node);
			//printf("colorchoose: temp %d color %d\n", Temp_int(G_nodeInfo(node)), *i);
			spilledWorkList = G_NodeList(node, spilledWorkList);
		}

		else {
			int* colorc = (int*)checked_malloc(sizeof(*colorc));
			*colorc = colorChoose;
			G_enter(color, node, colorc);
			coloredNodes = G_NodeList(node, coloredNodes);
		}
	}
}

static G_node chooseSpill() {
	// TO DO: select the least use and most degree
	G_node node = spillWorkList->head;
	int* degreen = G_look(degree, spillWorkList->head);
	int* numn = TAB_look(usesDefs, (Temp_temp)G_nodeInfo(spillWorkList->head));
	double rank = ((double)*numn) / ((double)*degreen);
	//printf("num %d degree %d rank%lf, temp%d\n", *numn, *degreen, rank, Temp_int(G_nodeInfo(spillWorkList->head)));
	G_nodeList nl = NULL;
	
	for (nl = spillWorkList->tail; nl; nl = nl->tail) {
		degreen = G_look(degree, nl->head);
		numn = TAB_look(usesDefs, (Temp_temp)G_nodeInfo(nl->head));
		double ranktmp = *numn / *degreen;
		if (ranktmp < rank) {
			//printf("temp%d degree%d num%d\n", Temp_int(G_nodeInfo(nl->head)), *degreen, *numn);
			rank = ranktmp;
			node = nl->head;
		}
	}
	spillWorkList = spillWorkList->tail;
	return node;
}

static void selectSpill() {
	G_node node = chooseSpill();
	//printf("select spill: temp %d\n", Temp_int(G_nodeInfo(node)));
	if (!G_inNodeList(node, simplifyWorkList))
		simplifyWorkList = G_NodeList(node, simplifyWorkList);
}

static AS_instrList rewriteProgram(F_frame f, AS_instrList il) {
	// First find the temp to spill
	assert(spilledWorkList);
	Temp_temp targetTemp = (Temp_temp)G_nodeInfo(spilledWorkList->head);
	//if (spilledWorkList) spilledWorkList = spilledWorkList->tail;
	int *colorn = G_look(color, spilledWorkList->head);
	printf("this target: %d, color: %d, frame:%s\n", Temp_int(targetTemp), *colorn, S_name(F_name(f)));

	int off = F_makeSpill(f);
	char *out1 = (char *)checked_malloc(STRLEN);
	char *out2 = (char *)checked_malloc(STRLEN);
	sprintf(out1, "movl `s0, $%d(`s1)", -off * 4);		// store
	sprintf(out2, "movl $%d(`s0), `d0", -off * 4);		// fetch
	AS_instrList r = AS_InstrList(NULL, il);			// The first is dummy
	il = r;
	for (; il->tail; il = il->tail) {
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
			}
		}

		else if (i->kind == I_MOVE) {
			Temp_tempList dstl = i->u.OPER.dst;
			Temp_tempList srcl = i->u.OPER.src;
			if (findTempFromList(targetTemp, dstl)) {
				//printf("fuckyounaive\n");
				// store
				//printf("move temp:%d %d\n", Temp_int(srcl->head), off);
				AS_instr newi = AS_Oper(out1, NULL, L(srcl->head, L(F_FP(), NULL)), NULL);
				//AS_instrList nl = AS_InstrList(newi, il->tail->tail);
				il->tail->head = newi;
				
				/*
				Temp_map tempmap = Temp_empty();
 				initTempMap(tempmap);
 				printf("=====AFTERIMOV+++++");
 				AS_printInstrList(stdout, il, Temp_layerMap(tempmap, Temp_name()));
 				printf("=====ENDIMOV+++++");
 				printf("fucknext %d %d\n", Temp_int(il->tail->tail->head->u.OPER.dst->head),
 					Temp_int(il->tail->tail->head->u.OPER.src->head));*/
			}

			else if (findTempFromList(targetTemp, srcl)) {
				//printf("fuckyouangry\n");
				// fetch
				AS_instr newi = AS_Oper(out2, dstl, L(F_FP(), NULL), NULL);
				//AS_instrList nl = AS_InstrList(newi, il->tail->tail);
				il->tail->head = newi;
			}
		}
	}

	return r->tail;
}

static void generateColorMap(Temp_map colorMap, G_graph ng) {
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
			case 6:
			case 7: break;
			default: assert(0);
		}
	}
}

struct RA_result RA_regAlloc(F_frame f, AS_instrList ilIn) {
	AS_instrList il = ilIn;
	bool done = FALSE;
	struct Live_graph lg;

	//debug
	int count = 0;
	//debug

	while (!done) {
		count++;
		precolored = NULL;
		initial = NULL;
		simplifyWorkList = NULL;
		freezeWorkList = NULL;
		spillWorkList = NULL;
		spilledWorkList = NULL;
		coalescedNodes = NULL;
		coloredNodes = NULL;
		selectStack = NULL;

		degree = NULL;
		color = NULL;

		usesDefs = NULL;

		//livenessAnalysis();
		G_graph flow = FG_AssemFlowGraph(il, f);
		lg = Live_liveness(flow);
		build(lg);

		/*
		//debug b
		G_nodeList d = NULL;
		printf("=========begindisplay==========\n");
		printf("simplifyworklist\n");
		for (d = simplifyWorkList; d; d = d->tail)
			printf("temp: %d ", Temp_int(G_nodeInfo(d->head)));
		printf("\n");
		printf("spillworklist\n");
		for (d = spillWorkList; d; d = d->tail)
			printf("temp: %d ", Temp_int(G_nodeInfo(d->head)));
		printf("\n");
		printf("select stack\n");
		for (d = selectStack; d; d = d->tail)
			printf("temp: %d ", Temp_int(G_nodeInfo(d->head)));
		printf("\n");
		printf("=====beforework=======\n");
		//debug e*/

		while (simplifyWorkList != NULL || spillWorkList != NULL) {
			if (simplifyWorkList != NULL)
				simplify();

			else if (spillWorkList != NULL)
				selectSpill();

			/*
			// debug b
			printf("begindisplay==========\n");
			printf("simplifyworklist\n");
			for (d = simplifyWorkList; d; d = d->tail)
				printf("temp: %d ", Temp_int(G_nodeInfo(d->head)));
			printf("\n");
			printf("spillworklist\n");
			for (d = spillWorkList; d; d = d->tail)
				printf("temp: %d ", Temp_int(G_nodeInfo(d->head)));
			printf("\n");
			printf("select stack\n");
			for (d = selectStack; d; d = d->tail)
				printf("temp: %d ", Temp_int(G_nodeInfo(d->head)));
			printf("\n");
			printf("=====beforework=======\n");
			// debug e*/

		}
		
		assignColors(); 
		/*
		if (!strncmp(S_name(F_name(f)), "quicksort", 5)) {
			printf("begin instr==================\n");
			Temp_map map = Temp_empty();
			initTempMap(map);
			AS_printInstrList(stdout, il, Temp_layerMap(map, Temp_name()));
			printf("end instr==================\n");
			if (count == 3) break;
			G_graph g = lg.graph;
			G_nodeList nl = G_nodes(g);
			for (; nl; nl = nl->tail) {
				int *colorn = G_look(color, nl->head);
				printf("\nTemp: %d color: %d", Temp_int(G_nodeInfo(nl->head)), *colorn);
			}
			printf("\n");
		//break;
		}*/

		if (spilledWorkList != NULL)
			il = rewriteProgram(f, il);
		
		else
			done = TRUE;
	}

	Temp_map colorMap = Temp_empty();
	generateColorMap(colorMap, lg.graph);
	struct RA_result ret;
	ret.coloring = colorMap;
	ret.il = il;
	return ret;
}
