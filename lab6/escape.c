#include <stdio.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "escape.h"
#include "table.h"

typedef struct escapeEntry_ *escapeEntry;

struct escapeEntry_ {
	int depth;
	bool* escape;
};

escapeEntry EscapeEntry(int depth, bool* escape) {
	escapeEntry e = (escapeEntry)checked_malloc(sizeof(*e));
	e->depth = depth;
	e->escape = escape;
	return e;
}

static void traverseExp(S_table env, int depth, A_exp e);
static void traverseDec(S_table env, int depth, A_dec d);
static void traverseVar(S_table env, int depth, A_var v);

void Esc_findEscape(A_exp exp) {
	traverseExp(S_empty(), 0, exp);
}

static void traverseExp(S_table env, int depth, A_exp e) {
	switch (e->kind) {
		case A_varExp: {
			traverseVar(env, depth, e->u.var); 
			return;
		}

		case A_callExp: {
			A_expList el = NULL;
			for (el = e->u.call.args; el; el = el->tail)
				traverseExp(env, depth, el->head);
			return;
		}

		case A_opExp: {
			traverseExp(env, depth, e->u.op.left);
			traverseExp(env, depth, e->u.op.right);
			return;
		}

		case A_recordExp: {
			A_efieldList fl = NULL;
			for (fl = e->u.record.fields; fl; fl = fl->tail)
				traverseExp(env, depth, fl->head->exp);

			return;
		}

		case A_seqExp: {
			A_expList el = NULL;
			for (el = e->u.seq; el; el = el->tail)
				traverseExp(env, depth, el->head);
			return;
		}

		case A_assignExp: {
			traverseVar(env, depth, e->u.assign.var);
			traverseExp(env, depth, e->u.assign.exp);
			return;
		}

		case A_ifExp: {
			traverseExp(env, depth, e->u.iff.test);
			traverseExp(env, depth, e->u.iff.then);
			if (e->u.iff.elsee)
				traverseExp(env, depth, e->u.iff.elsee);
			return;
		}

		case A_whileExp: {
			traverseExp(env, depth, e->u.whilee.test);
			traverseExp(env, depth, e->u.whilee.body);
			return;
		}

		case A_forExp: {
			traverseExp(env, depth, e->u.forr.lo);
			traverseExp(env, depth, e->u.forr.hi);
			S_beginScope(env);
			e->u.forr.escape = FALSE;
			escapeEntry es = EscapeEntry(depth, &(e->u.forr.escape));
			S_enter(env, e->u.forr.var, es);
			traverseExp(env, depth, e->u.forr.body);
			S_endScope(env);
			return;
		}

		case A_letExp: {
			A_decList dl = NULL;
			S_beginScope(env);
			for (dl = e->u.let.decs; dl; dl = dl->tail)
				traverseDec(env, depth, dl->head);

			traverseExp(env, depth, e->u.let.body);
			S_endScope(env);
			return;
		}

		case A_arrayExp: {
			traverseExp(env, depth, e->u.array.size);
			traverseExp(env, depth, e->u.array.init);
			return;
		}

		case A_nilExp:
		case A_intExp:
		case A_stringExp:
		case A_breakExp:
			return;
	}
	assert(0);
}

static void traverseDec(S_table env, int depth, A_dec d) {
	switch(d->kind) {
		case A_functionDec: {
			A_fundecList fl = NULL;
			for (fl = d->u.function; fl; fl = fl->tail) {
				S_beginScope(env);
				A_fieldList params;
				for (params = fl->head->params; params; params = params->tail) {
					params->head->escape = TRUE;
					escapeEntry e = EscapeEntry(depth, &(params->head->escape));
					S_enter(env, params->head->name, e);
				}

				traverseExp(env, depth + 1, fl->head->body);
				S_endScope(env);
			}
			return;
		}

		case A_varDec: {
			traverseExp(env, depth, d->u.var.init);
			d->u.var.escape = FALSE;
			escapeEntry e = EscapeEntry(depth, &(d->u.var.escape));
			S_enter(env, d->u.var.var, e);
			return;
		}

		case A_typeDec:
			return;
	}
}

static void traverseVar(S_table env, int depth, A_var v) {
	switch(v->kind) {
		case A_simpleVar: {
			escapeEntry e = S_look(env, v->u.simple);
			if (!e) assert(0);
			if (depth > e->depth)
				*(e->escape) = TRUE;
			return;
		}

		case A_fieldVar: {
			traverseVar(env, depth, v->u.field.var);
			return;
		}

		case A_subscriptVar: {
			traverseVar(env, depth, v->u.subscript.var);
			traverseExp(env, depth, v->u.subscript.exp);
			return;
		}
	}
	assert(0);

}
