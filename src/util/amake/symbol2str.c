/*	@(#)symbol2str.c	1.2	94/04/07 14:54:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "Lpars.h"

char *
symbol2str(tok)
	int tok;
{
	static char buf[2] = { '\0', '\0' };

	if (040 <= tok && tok < 0177) {
		buf[0] = tok;
		buf[1] = '\0';
		return buf;
	}
	switch (tok) {
	case ID :
		return "identifier";
	case ID_STRING :
		return "string";
	case REFID :
		return "variable";
	case INTEGER :
		return "%integer";
	case EQ :
		return "==";
	case NOT_EQ :
		return "<>";
	case ARROW :
		return "=>";
	case AND :
		return "%and";
	case BOOLEAN :
		return "%boolean";
	case CLUSTER :
		return "%cluster";
	case COMPUTED :
		return "%computed";
	case CONDITION :
		return "%condition";
	case CONFORM :
		return "%conform";
	case DECLARE :
		return "%declare";
	case DERIVE :
		return "%derive";
	case DEFAULT :
		return "%default";
	case DIAG :
		return "%diag";
	case DO :
		return "%do";
	case EXPORT :
		return "%export";
	case FFALSE :
		return "%false";
	case GENERIC :
		return "%generic";
	case IF :
		return "%if";
	case IGNORE :
		return "%ignore";
	case IMPORT :
		return "%import";
	case IN :
		return "%in";
	case INCLUDE :
		return "%include";
	case INSTANCE :
		return "%instance";
	case LIST :
		return "%list";
	case NOT :
		return "%not";
	case OR :
		return "%or";
	case OUT :
		return "%out";
	case POST :
		return "%post";
	case PRE :
		return "%pre";
	case RETURN :
		return "%return";
	case SOURCES :
		return "%sources";
	case STRING :
		return "string";
	case TARGETS :
		return "%targets";
	case TMP :
		return "%tmp";
	case TOOL :
		return "%tool";
	case TRIGGER :
		return "%trigger";
	case TTRUE :
		return "%true";
	case UNKNOWN :
		return "%unknown";
	case USE :
		return "%use";
	case WHEN :
		return "%when";
	case '\n':
	case '\f':
	case '\v':
	case '\r':
	case '\t':
		buf[0] = tok;
		return buf;
	default:
		return "bad token";
	}
}
