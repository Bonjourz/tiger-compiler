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

static G_nodeList precolored = NULL;	/* no used */
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
static TAB_table moveList = NULL;
static G_table alias = NULL;

static Live_moveList coalescedMoves = NULL;
static Live_moveList constrainedMoves = NULL;
static Live_moveList frozenMoves = NULL;
static Live_moveList workListMoves = NULL;
static Live_moveList activeMoves = NULL;

static TAB_table usesDefs = NULL;
static TAB_table tempToNode = NULL;

static void simplify();
static void coalesce();
static void freeze();
static void selectSpill();
static void makeWorkList();
static G_nodeList adjacent(G_node n);
static Live_moveList nodeMoves(G_node n);
static bool moveRelated(G_node n);
static void decrementDegree(G_node n);
static void enableMoves(G_nodeList nl);
static void addWorkList(G_node n);
static bool OK(G_nodeList nl, G_node r);
static bool conservative(G_nodeList nl);
static G_node getAlias(G_node n);
static void combine(G_node u, G_node v);
static void freezeMoves(G_node u);

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

static G_nodeList adjacent(G_node n) {
	G_nodeList nl = G_adj(n);
	G_nodeList head = NULL, tail = NULL;
	for (; nl; nl = nl->tail) {
		if (!G_inNodeList(nl->head, selectStack) 
			&& !G_inNodeList(nl->head, coalescedNodes)) {
			if (!head) {
				head = G_NodeList(nl->head, NULL);
				tail = head;
			}
			else {
				tail->tail = G_NodeList(nl->head, NULL);
				tail = tail->tail;
			}
		}
	}
	return head;
}

static void build(struct Live_graph lg) {
	G_nodeList nl = G_nodes(lg.graph);
	for (; nl; nl = nl->tail) {
		//assert(!G_duplicate(G_adj(nl->head)));
		int* degreen = (int*)checked_malloc(sizeof(*degreen));
		int* colorn = (int*)checked_malloc(sizeof(*colorn));
		*colorn = getColorFromTemp((Temp_temp)G_nodeInfo(nl->head));
		*degreen = G_degree(nl->head);
		G_enter(color, nl->head, colorn);
		G_enter(degree, nl->head, degreen);
		
		if (!isMachineReg((Temp_temp)G_nodeInfo(nl->head))) {
			if ((*degreen) >= REG_NUM) {
				//assert(!G_inNodeList(nl->head, spillWorkList));
				spillWorkList = G_NodeList(nl->head, spillWorkList);
			}
			else if (moveRelated(nl->head)) {
				//assert(!G_inNodeList(nl->head, freezeWorkList));
				freezeWorkList = G_NodeList(nl->head, freezeWorkList);
			}
			else {
				//assert(!G_inNodeList(nl->head, simplifyWorkList));
				simplifyWorkList = G_NodeList(nl->head, simplifyWorkList);
			}
		}
	}
}

static void makeWorkList() {
	/* empty */
}

static Live_moveList nodeMoves(G_node n) {
	Live_moveList lml = TAB_look(moveList, n);
	Live_moveList head = NULL, tail = NULL;
	for (; lml; lml = lml->tail) {
		if (L_inLiveMoveList(lml->head, activeMoves) ||
			L_inLiveMoveList(lml->head, workListMoves)) {
			if (!head) {
				head = Live_MoveList(lml->head, NULL);
				tail = head;
			}
			else {
				tail->tail = Live_MoveList(lml->head, NULL);
				tail = tail->tail;
			}
		}
	}
	return head;
}

static bool moveRelated(G_node n) {
	return nodeMoves(n) != NULL;
}

static void decrementDegree(G_node n) {
	int *degreen = G_look(degree, n);
	*degreen = *degreen - 1;
	if (*degreen == REG_NUM - 1 && !isMachineReg(G_nodeInfo(n))) {
		enableMoves(G_union(G_NodeList(n, NULL), adjacent(n)));
		spillWorkList =  G_subNodeFromList(n, spillWorkList);
		if (moveRelated(n)) {
			if (!G_inNodeList(n, freezeWorkList))
				freezeWorkList = G_NodeList(n, freezeWorkList);
		}

		else {
			if (!G_inNodeList(n, simplifyWorkList))
				simplifyWorkList = G_NodeList(n, simplifyWorkList);
		}
	}
}

static void enableMoves(G_nodeList nl) {
	assert(!G_duplicate(nl));
	for (; nl; nl = nl->tail) {
		Live_moveList lml = nodeMoves(nl->head);
		for (; lml; lml = lml->tail) {
			Live_move lm = lml->head;
			if (L_inLiveMoveList(lm, activeMoves)) {
				activeMoves = L_subLiveMoveList(lml->head, activeMoves);
				workListMoves = Live_MoveList(lml->head, workListMoves);
			}
		}
	}
}

static void addWorkList(G_node n) {
	if (!isMachineReg(G_nodeInfo(n)) && !moveRelated(n)) {
		int *degreen = G_look(degree, n);

		if (*degreen < REG_NUM) {
			freezeWorkList = G_subNodeFromList(n, freezeWorkList);
			simplifyWorkList = G_NodeList(n, simplifyWorkList);
		}
	}
}

static void coalesce() {
	Live_move lm = workListMoves->head;
	workListMoves = workListMoves->tail;
	G_node x = getAlias(lm->src);
	G_node y = getAlias(lm->dst);
	G_node u, v;
	if (isMachineReg(G_nodeInfo(y))) {
		u = y;
		v = x;
	}
	else {
		u = x;
		v = y;
	}
	if (u == v) {
		coalescedMoves = Live_MoveList(lm, coalescedMoves);
		addWorkList(u);
	}
	else if (isMachineReg(G_nodeInfo(v)) || G_inNodeList(u, G_adj(v))) {
		constrainedMoves = Live_MoveList(lm, constrainedMoves);
		addWorkList(u);
		addWorkList(v);
	}
	else if ((isMachineReg(G_nodeInfo(u)) && OK(adjacent(v), u)) ||
		(!isMachineReg(G_nodeInfo(u)) && conservative(G_union(adjacent(v), adjacent(u))))) {
		coalescedMoves = Live_MoveList(lm, coalescedMoves);
		combine(u, v);
		addWorkList(u);
	}
	else
		activeMoves = Live_MoveList(lm, activeMoves);
}

static bool OK(G_nodeList nl, G_node r) {
	for (; nl; nl = nl->tail) {
		int *degreen = G_look(degree, nl->head);
		if (*degreen < REG_NUM)
			continue;

		if (isMachineReg((Temp_temp)G_nodeInfo(nl->head)))
			continue;

		if (G_inNodeList(nl->head, G_adj(r)))
			continue;

		return FALSE;
	}

	return TRUE;
}



static bool conservative(G_nodeList nl) {
	assert(!G_duplicate(nl));
	int count = 0;
	for (; nl; nl = nl->tail) {
		int *degreen = G_look(degree, nl->head);
		if (*degreen >= REG_NUM)
			count++;
	}
	return count < REG_NUM;
}

static G_node getAlias(G_node n) {
	if (G_inNodeList(n, coalescedNodes)) {
		G_node a = G_look(alias, n);
		assert(a);
		return getAlias(a);
	}
	assert(n);
	return n;
}

static void combine(G_node u, G_node v) {
	if (G_inNodeList(v, freezeWorkList))
		freezeWorkList = G_subNodeFromList(v, freezeWorkList);
	else
		spilledWorkList = G_subNodeFromList(v, spilledWorkList);
	
	coalescedNodes = G_NodeList(v, coalescedNodes);
	// alias[v] = u;
	G_enter(alias, v, u);
	// nodeMoves[u] = nodeMoves[u] + nodemoves[v]
	Live_moveList lmlu = TAB_look(moveList, u);
	Live_moveList lmlv = TAB_look(moveList, v);
	lmlu = L_union(lmlu, lmlv);
	TAB_enter(moveList, u, lmlu);

	G_nodeList nl = adjacent(v);
	for (; nl; nl = nl->tail) {
		int *degreen = G_look(degree, u);
		*degreen = *degreen + 1;
		G_addEdge(nl->head, u);
	}

	int *degreen = G_look(degree, u);
	if (*degreen > REG_NUM && G_inNodeList(u, freezeWorkList)) {
		freezeWorkList = G_subNodeFromList(u, freezeWorkList);
		assert(!G_inNodeList(u, spilledWorkList));
		spilledWorkList = G_NodeList(u, spilledWorkList);
	}
}

static void freeze() {
	G_node u = freezeWorkList->head;
	freezeWorkList = freezeWorkList->tail;
	assert(!G_inNodeList(u, simplifyWorkList));
	simplifyWorkList = G_NodeList(u, simplifyWorkList);
	freezeMoves(u);
}

static void freezeMoves(G_node u) {
	Live_moveList lml = nodeMoves(u);
	for (; lml; lml = lml->tail) {
		G_node x = lml->head->src;
		G_node y = lml->head->dst;
		G_node v = NULL;
		if (getAlias(x) == getAlias(y))
			v = getAlias(x);
		else
			v = getAlias(y);

		activeMoves = L_subLiveMoveList(lml->head, activeMoves);
		frozenMoves = Live_MoveList(lml->head, frozenMoves);
		if (nodeMoves(v) == NULL) {
			int *degreen = G_look(degree, v);
			if (*degreen < REG_NUM) {
				freezeWorkList = G_subNodeFromList(v, freezeWorkList);
				simplifyWorkList = G_NodeList(v, simplifyWorkList);
			}
		}
	}
}

static void simplify() {
	G_node simNode = simplifyWorkList->head;
	simplifyWorkList = simplifyWorkList->tail;
	//assert(!G_inNodeList(simNode, simplifyWorkList));
	//assert(!G_inNodeList(simNode, selectStack));
	if (!G_inNodeList(simNode, selectStack))
		selectStack = G_NodeList(simNode, selectStack);

	G_nodeList nl = adjacent(simNode);
	assert(!G_inNodeList(simNode, nl));
	for (; nl; nl = nl->tail)
		decrementDegree(nl->head);
}

static void assignColors() {
	while(selectStack != NULL) {
		G_node node = selectStack->head;
		selectStack = G_subNodeFromList(node, selectStack);
		assert(!G_inNodeList(node, selectStack));
		
		int colorSet[REG_NUM + 2] = {1, 1, 1, 1, 1, 1, 0, 0};
		G_nodeList adj = G_adj(node);

		for (; adj; adj = adj->tail) {
			G_node aliasn = getAlias(adj->head);
			int *colorAdj = G_look(color, aliasn);
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
			spilledWorkList = G_NodeList(node, spilledWorkList);
		}

		else {
			int* colorc = (int*)checked_malloc(sizeof(*colorc));
			*colorc = colorChoose;
			G_enter(color, node, colorc);
			coloredNodes = G_NodeList(node, coloredNodes);
		}
	}

	G_nodeList nl = coalescedNodes;
	for (; nl; nl = nl->tail) {
		G_node a = getAlias(nl->head);
		int *colorc = G_look(color, a);
		G_enter(color, nl->head, colorc);
	}
	
}

static G_node chooseSpill() {
	G_node node = spillWorkList->head;
	int* degreen = G_look(degree, node);
	int* numn = TAB_look(usesDefs, (Temp_temp)G_nodeInfo(node));
	double rank = ((double)*numn) / ((double)*degreen);
	G_nodeList nl = NULL;
	
	for (nl = spillWorkList->tail; nl; nl = nl->tail) {
		degreen = G_look(degree, nl->head);
		numn = TAB_look(usesDefs, (Temp_temp)G_nodeInfo(nl->head));
		double ranktmp = ((double)*numn) / ((double)*degreen);
		if (ranktmp < rank) {
			rank = ranktmp;
			node = nl->head;
		}
	}

	spillWorkList = G_subNodeFromList(node, spillWorkList);
	return node;
}

static void selectSpill() {
	G_node m = chooseSpill();
	G_nodeList nl = spillWorkList;
	if (!G_inNodeList(m, simplifyWorkList))
		simplifyWorkList = G_NodeList(m, simplifyWorkList);
	freezeMoves(m);
}

static AS_instrList rewriteProgram(F_frame f, AS_instrList il) {
	assert(spilledWorkList);
	Temp_temp targetTemp = (Temp_temp)G_nodeInfo(spilledWorkList->head);
	int *colorn = G_look(color, spilledWorkList->head);

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
			case 6:	Temp_enter(colorMap, temp, "%ebp"); break;
			case 7: Temp_enter(colorMap, temp, "%esp"); break;
			default: assert(0);
		}
	}
}

static AS_instrList deleteMoves(AS_instrList il) {
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


	while (!done) {
		precolored = NULL;
		initial = NULL;
		simplifyWorkList = NULL;
		freezeWorkList = NULL;
		spillWorkList = NULL;
		spilledWorkList = NULL;
		coalescedNodes = NULL;
		coloredNodes = NULL;
		selectStack = NULL;

		alias = G_empty();
		degree = G_empty();
		color = G_empty();

		coalescedMoves = NULL;
		constrainedMoves = NULL;
		frozenMoves = NULL;
		workListMoves = NULL;
		activeMoves = NULL;

		//livenessAnalysis();
		G_graph flow = FG_AssemFlowGraph(il, f);
		lg = Live_liveness(flow);
		moveList = lg.moveList;
		usesDefs = lg.usesDefs;
		workListMoves = lg.workListMoves;
		tempToNode = lg.tempToNode;
		build(lg);
		makeWorkList();
		
		while (simplifyWorkList || workListMoves || 
			freezeWorkList || spillWorkList) {
			if (simplifyWorkList)
				simplify();

			else if (workListMoves)
				coalesce();

			else if (freezeWorkList)
				freeze();

			else if (spillWorkList)
				selectSpill();
		}
		
		assignColors(); 

		if (spilledWorkList != NULL)
			il = rewriteProgram(f, il);
		
		else
			done = TRUE;
	}

	Temp_map colorMap = Temp_empty();
	generateColorMap(colorMap, lg.graph);

	il = deleteMoves(il);
	struct RA_result ret;
	ret.coloring = colorMap;
	ret.il = il;
	return ret;
}
