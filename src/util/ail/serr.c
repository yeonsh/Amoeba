/*	@(#)serr.c	1.2	94/04/07 14:38:43 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */


/*
 *	This file contains the first lines of
 *	the Serr-function, whose body is generated
 *	by mkerr. It returns an error message
 *	deduced from the y.output-file generated
 *	by yacc.
 */
char *
Serr(state)
    int state;
{
    switch (state) {
    default: return "This errormessage indicates a parser bug\n";
    case 293:
    case 288:
    case 276:
    case 259:
    case 236:
    case 232:
    case 216:
    case 195:
    case 132:
    case 104:
    case 64:
    case 14:
    case 10:
    case 9: return "identifier expected\n";
    case 229:
    case 228:
    case 227:
    case 226:
    case 225:
    case 7:
    case 6:
    case 5:
    case 4:
    case 3: return "\";\" expected\n";
    case 287:
    case 239:
    case 103:
    case 27: return "type expected\n";
    case 280:
    case 267:
    case 260:
    case 254:
    case 248:
    case 204:
    case 198:
    case 151:
    case 133:
    case 124:
    case 123:
    case 122:
    case 121:
    case 120:
    case 119:
    case 118:
    case 117:
    case 116:
    case 115:
    case 114:
    case 113:
    case 112:
    case 111:
    case 110:
    case 109:
    case 87:
    case 82:
    case 81:
    case 80:
    case 79:
    case 39:
    case 38: return "expression expected\n";
    case 1: return "definition expected\n";
    case 15: return "identifier or \"{\" expected\n";
    case 16: return "identifier or \"{\" expected\n";
    case 17: return "identifier or \"{\" expected\n";
    case 24: return "\"=\" expected\n";
    case 28: return "\"with\" expected\n";
    case 37: return "\"{\" expected\n";
    case 40: return "\"{\" expected\n";
    case 42: return "identifier or \"(\" or \"*\" expected\n";
    case 63: return "\"}\" or \",\" expected\n";
    case 66: return "\"..\" expected\n";
    case 94: return "identifier or \"(\" or \"*\" expected\n";
    case 96: return "identifier or \"(\" or \"*\" expected\n";
    case 100: return "\":\" expected\n";
    case 125: return "identifier or integer or \"(\" or @ expected\n";
    case 126: return "identifier or integer or \"(\" or @ expected\n";
    case 127: return "identifier or integer or \"(\" or @ expected\n";
    case 134: return "\"||\" or \")\" expected\n";
    case 135: return "\";\" or \"}\" expected\n";
    case 138: return "identifier or \"(\" or \"*\" expected\n";
    case 141: return "\")\" expected\n";
    case 142: return " \"]\" or expression expected\n";
    case 143: return "\")\" expected\n";
    case 147: return "\";\" or \",\" expected\n";
    case 153: return "\"]\" expected\n";
    case 173: return "\")\" expected\n";
    case 182: return " \"]\" or \"||\" or \":\" expected\n";
    case 186: return "\",\" expected\n";
    case 190: return "identifier or \"(\" or \"*\" expected\n";
    case 194: return "stringconstant expected\n";
    case 200: return "\")\" or \",\" expected\n";
    case 205: return "\",\" expected\n";
    case 207: return "identifier or \"(\" or \"*\" expected\n";
    case 209: return "\"const\" or ENUM or MARSHAL or \"rights\" or STRUCT or TYPEDEF or UNION or identifier or \";\" or \"}\" expected\n";
    case 210: return "\";\" or \",\" expected\n";
    case 212: return "\";\" or \",\" expected\n";
    case 218: return " \"]\" or \"||\" expected\n";
    case 234: return "stringconstant expected\n";
    case 247: return "\"(\" expected\n";
    case 250: return "\"=\" expected\n";
    case 257: return " \"]\" or \",\" expected\n";
    case 262: return "\")\" or \",\" expected\n";
    case 263: return "\")\" or \",\" expected\n";
    case 269: return "\"=\" expected\n";
    case 281: return "\")\" or \",\" expected\n";
    case 302: return "identifier or \"(\" or \"*\" expected\n";
    } /* End of switch */
} /* Serr */
