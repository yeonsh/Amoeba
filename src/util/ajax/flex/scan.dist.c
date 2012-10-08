/*	@(#)scan.dist.c	1.2	96/02/27 13:00:19 */
#define YY_DEFAULT_ACTION YY_FATAL_ERROR( "flex scanner jammed" );
#define FLEX_USE_ECS
#define FLEX_USE_MECS
/* A lexical scanner generated by flex */

#include "flexskeldef.h"

# line 1 "scan.l"
#define INITIAL 0
/* scan.l - scanner for flex input */
/*
 * Copyright (c) 1987, the University of California
 * 
 * The United States Government has rights in this work pursuant to
 * contract no. DE-AC03-76SF00098 between the United States Department of
 * Energy and the University of California.
 * 
 * This program may be redistributed.  Enhancements and derivative works
 * may be created provided the new works, if made available to the general
 * public, are made available for use by anyone.
 */
# line 16 "scan.l"
#include "flexdef.h"
#include "parse.h"

#define ACTION_ECHO fprintf( temp_action_file, "%s", yytext )
#define MARK_END_OF_PROLOG fprintf( temp_action_file, "%%%% end of prolog\n" );

#undef YY_DECL
#define YY_DECL \
	int flexscan()

#define RETURNCHAR \
	yylval = yytext[0]; \
	return ( CHAR );

#define RETURNNAME \
	(void) strcpy( nmstr, yytext ); \
	return ( NAME );

#define PUT_BACK_STRING(str, start) \
	for ( i = strlen( str ) - 1; i >= start; --i ) \
	    unput(str[i])
#define SECT2 2
#define SECT2PROLOG 4
#define SECT3 6
#define CODEBLOCK 8
#define PICKUPDEF 10
#define SC 12
#define CARETISBOL 14
#define NUM 16
#define QUOTE 18
#define FIRSTCCL 20
#define CCL 22
#define ACTION 24
#define RECOVER 26
#define BRACEERROR 28
#define C_COMMENT 30
#define C_COMMENT_2 32
#define ACTION_COMMENT 34
#define ACTION_STRING 36
#define PERCENT_BRACE_ACTION 38
# line 53 "scan.l"
#define YY_JAM 226
#define YY_JAM_BASE 800
#define YY_TEMPLATE 227
static char l[227] =
    {   0,
       -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,
       -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,
       -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,
       -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,   -2,
       14,    7,   13,   11,    7,   12,   14,   14,   14,   10,
       46,   39,   40,   32,   46,   45,   30,   46,   46,   46,
       39,   28,   46,   45,   31,    0,   27,   99,    0,   21,
        0,   23,   22,   24,   52,   48,   49,   51,   53,   67,
       68,   65,   64,   66,   54,   56,   55,   54,   60,   59,
       60,   60,   62,   62,   62,   63,   76,   80,   79,   81,

       81,   74,   75,    0,   25,   70,   69,   17,   19,   18,
       89,   91,   90,   83,   85,   84,   92,   94,   95,   96,
       72,   72,   73,   72,    7,   11,    0,    7,    1,    0,
        2,    0,    8,    4,    5,    0,    3,   10,   39,   40,
        0,    0,   35,    0,    0,   97,   97,    0,   34,   33,
       34,    0,   39,   28,    0,    0,    0,   42,   38,   26,
        0,   23,   50,   51,   64,   98,   98,    0,   57,   58,
       61,   76,    0,   78,    0,   77,   15,   87,   83,   82,
       92,   93,   71,    1,    0,    9,    8,    0,    0,    6,
       36,    0,   37,   43,    0,    0,   97,   34,   34,   44,

       29,    0,   36,    0,   29,    0,   42,    0,   20,   98,
        0,   16,    0,   88,   71,    0,    0,   97,   98,    0,
        0,   97,   98,    4,    0,    0
    } ;

static char e[128] =
    {   0,
        1,    1,    1,    1,    1,    1,    1,    1,    2,    3,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    2,    1,    4,    5,    6,    7,    1,    8,    9,
        9,   10,    9,   11,   12,    9,   13,   14,   15,   15,
       15,   15,   15,   15,   15,   15,   15,    1,    1,   16,
        1,   17,    9,    1,   23,   22,   22,   22,   22,   22,
       22,   22,   22,   22,   22,   22,   22,   22,   22,   22,
       22,   24,   25,   26,   22,   22,   22,   27,   22,   22,
       18,   19,   20,   21,   22,    1,   23,   22,   22,   22,

       22,   22,   22,   22,   22,   22,   22,   22,   22,   22,
       22,   22,   22,   24,   25,   26,   22,   22,   22,   27,
       22,   22,   28,   29,   30,    1,    1
    } ;

static char m[31] =
    {   0,
        1,    2,    3,    4,    1,    1,    1,    5,    1,    6,
        1,    1,    5,    7,    7,    1,    1,    1,    8,    9,
        1,    7,    7,    7,    7,    7,    7,    5,    1,   10
    } ;

static short int b[276] =
    {   0,
        0,   26,   52,   80,  286,  285,    0,    0,  284,    1,
        3,    7,   99,  116,  265,  264,  141,  169,   11,   13,
        0,   22,   25,   47,  197,  225,  281,  280,    8,   10,
       32,   54,   66,   69,   75,   85,   88,   99,  110,  112,
      800,  280,  800,    0,   44,  800,  277,  104,  269,    0,
      800,  144,  800,  800,   71,  800,  800,  259,   83,  242,
      268,  800,  270,  266,  800,  271,    0,  800,  270,  800,
       33,    0,  270,  800,  800,  800,  242,    0,  800,  800,
      800,  800,   91,  800,  800,  800,  800,  114,  800,  800,
      116,  250,  800,    0,  136,  800,    0,  800,  800,  126,

      251,  800,  800,  257,  800,  800,  800,  150,  800,  246,
      151,  800,  245,    0,  800,  241,    0,  800,  800,    0,
      249,  156,  800,  145,  249,    0,  247,  162,  800,  246,
      800,  245,    0,  219,  800,  234,  800,    0,  167,  800,
      206,  229,  800,  147,  165,  800,  162,    0,    0,  800,
      284,  165,  313,  800,  178,  179,  184,    0,  800,  800,
      218,    0,  800,    0,  178,  800,  180,    0,  800,  800,
      800,    0,  190,  800,    0,  800,  216,  187,    0,  800,
        0,  800,    0,  800,  185,  800,    0,  139,  146,  800,
      800,  133,  800,  800,  188,  100,  197,    0,    0,  800,

      800,  210,  201,  213,  800,  212,    0,   97,  800,  203,
       91,  800,   74,  800,    0,   51,  216,  209,  225,   34,
      227,  800,  800,  800,  224,  800,  342,  352,  362,  372,
      382,  392,  402,  412,  422,  432,  442,  452,  462,  472,
      482,  492,  502,  512,   13,  522,  532,  542,   11,  552,
      562,  572,  582,  592,  602,    0,  612,  622,  632,  642,
      651,  661,  671,  681,  691,  701,  711,  721,  731,  740,
      750,  760,  770,  780,  790
    } ;

static short int d[276] =
    {   0,
      227,  227,  228,  228,  229,  229,  230,  230,  231,  231,
      232,  232,  233,  233,  226,  226,  234,  234,  235,  235,
      236,  236,  237,  237,  238,  238,  239,  239,  226,  226,
      240,  240,  241,  241,  242,  242,  243,  243,  244,  244,
      226,  226,  226,  245,  246,  226,  247,  248,  226,  249,
      226,  226,  226,  226,  226,  226,  226,  250,  251,  252,
      253,  226,  226,  226,  226,  229,  254,  226,  231,  226,
      231,  255,  226,  226,  226,  226,  226,  256,  226,  226,
      226,  226,  226,  226,  226,  226,  226,  251,  226,  226,
      257,  258,  226,  259,  251,  226,  260,  226,  226,  261,

      226,  226,  226,  239,  226,  226,  226,  240,  226,  226,
      241,  226,  226,  262,  226,  226,  263,  226,  226,  264,
      244,  244,  226,  244,  226,  245,  246,  246,  226,  247,
      226,  265,  266,  226,  226,  267,  226,  249,  226,  226,
      226,  268,  226,  250,  250,  226,  226,  251,  269,  226,
      269,  253,  253,  226,  253,  253,  270,  271,  226,  226,
      272,  255,  226,  256,  226,  226,  226,  257,  226,  226,
      226,  260,  261,  226,  261,  226,  273,  274,  262,  226,
      263,  226,  275,  226,  265,  226,  266,  226,  267,  226,
      226,  268,  226,  226,  250,  250,  226,  269,  151,  226,

      226,  253,  253,  270,  226,  270,  271,  272,  226,  226,
      273,  226,  274,  226,  275,  226,  250,  226,  226,  226,
      250,  226,  226,  226,  250,-32767,  226,  226,  226,  226,
      226,  226,  226,  226,  226,  226,  226,  226,  226,  226,
      226,  226,  226,  226,  226,  226,  226,  226,  226,  226,
      226,  226,  226,  226,  226,  226,  226,  226,  226,  226,
      226,  226,  226,  226,  226,  226,  226,  226,  226,  226,
      226,  226,  226,  226,  226
    } ;

static short int n[831] =
    {   0,
      226,   42,   43,   70,   73,   74,  164,   71,   73,   74,
      106,   90,  106,   86,   87,   86,   87,  138,   91,  126,
       92,   44,   44,   44,   44,   44,   44,   45,   46,   88,
       47,   88,   48,   90,  109,   70,   94,  107,   49,  107,
       91,  110,   92,   95,   96,  128,  129,   50,   50,   50,
       50,   50,   50,   52,   53,   54,  109,   55,   94,  224,
       56,   56,  161,  110,   56,   95,   96,   57,  112,   58,
       59,  112,  143,  143,  220,  113,  214,  115,  113,   60,
       56,   61,   62,   54,  116,   55,   63,  115,   56,   56,
      118,  119,   64,  212,  116,   57,  147,   58,   59,  209,

       65,  118,  119,  148,  165,  165,  120,   60,   56,   76,
      133,  122,  123,  122,  123,   77,  124,  120,  124,  144,
       78,   78,   78,   78,   78,   78,   76,  147,  134,  167,
      135,  136,   77,  174,  148,  193,  168,   78,   78,   78,
       78,   78,   78,   81,  175,  139,  140,  226,  190,  147,
      141,   82,  226,  226,   83,   83,  148,  122,  226,  226,
      226,  216,  124,  128,  129,  145,  194,  201,  139,  140,
       84,   81,  142,  141,  183,  197,  197,  202,  195,   82,
      201,  201,   83,   83,  144,  196,  205,  186,  159,  214,
      202,  165,  165,  210,  210,  142,  206,  174,   84,   98,

       99,  217,  217,  201,  100,  203,  145,  194,  175,  101,
      218,  218,  201,  202,  205,  205,  219,  219,  212,  226,
      209,  192,  222,  222,  102,  206,  103,   98,   99,  221,
      221,  193,  100,  191,  145,  194,  190,  101,  223,  223,
      225,  225,  145,  194,  188,  145,  194,  186,  131,  184,
      125,  226,  102,  180,  103,  150,  150,  178,  177,  105,
      176,  170,  163,  151,  151,  151,  151,  151,  151,  153,
      154,   73,   70,   67,  155,  159,  158,  145,  137,  131,
      156,  125,  105,  105,   79,   79,   70,   67,   67,  226,
      226,  226,  226,  226,  226,  226,  157,  199,  199,  226,

      226,  226,  226,  226,  226,  199,  199,  199,  199,  199,
      199,  226,  226,  200,  153,  154,  226,  226,  226,  155,
      226,  226,  226,  226,  226,  156,  226,  226,  226,  226,
      226,  226,  226,  226,  226,  226,  226,  226,  226,  226,
      226,  157,   41,   41,   41,   41,   41,   41,   41,   41,
       41,   41,   51,   51,   51,   51,   51,   51,   51,   51,
       51,   51,   66,   66,   66,   66,   66,   66,   66,   66,
       66,   66,   68,   68,   68,   68,   68,   68,   68,   68,
       68,   68,   69,   69,   69,   69,   69,   69,   69,   69,
       69,   69,   72,   72,   72,   72,   72,   72,   72,   72,

       72,   72,   75,   75,  226,   75,   75,   75,   75,   75,
       75,   75,   80,   80,   80,   80,   80,   80,   80,   80,
       80,   80,   85,   85,   85,   85,   85,   85,   85,   85,
       85,   85,   89,   89,  226,   89,   89,   89,   89,   89,
       89,   89,   93,   93,  226,   93,   93,   93,   93,   93,
       93,   93,   97,   97,   97,   97,   97,   97,   97,   97,
       97,   97,  104,  104,  104,  104,  104,  104,  104,  104,
      104,  104,  108,  108,  108,  108,  108,  108,  108,  108,
      108,  108,  111,  111,  111,  111,  111,  111,  111,  111,
      111,  111,  114,  114,  114,  114,  114,  114,  114,  114,

      114,  114,  117,  117,  117,  117,  117,  117,  117,  117,
      117,  117,  121,  121,  121,  121,  121,  121,  121,  121,
      121,  121,  127,  127,  127,  127,  127,  127,  127,  127,
      127,  127,  130,  130,  130,  130,  130,  130,  130,  130,
      130,  130,  132,  132,  132,  132,  132,  132,  132,  132,
      132,  132,  144,  144,  226,  144,  144,  144,  144,  144,
      226,  144,  146,  146,  226,  146,  146,  146,  146,  146,
      146,  146,  149,  149,  226,  149,  149,  149,  149,  149,
      149,  149,  152,  152,  152,  152,  152,  152,  152,  152,
      152,  152,  160,  226,  226,  160,  160,  160,  160,  160,

      160,  160,  162,  162,  226,  162,  162,  162,  162,  162,
      162,  162,  166,  166,  226,  166,  166,  166,  166,  166,
      166,  166,  169,  169,  226,  169,  169,  169,  169,  169,
      169,  169,  171,  171,  226,  171,  171,  171,  171,  171,
      226,  171,  172,  172,  226,  226,  226,  172,  172,  172,
      172,  173,  173,  226,  173,  173,  173,  173,  173,  173,
      173,  179,  179,  226,  179,  179,  226,  179,  179,  179,
      179,  181,  181,  226,  226,  181,  181,  181,  226,  181,
      181,  182,  182,  226,  182,  182,  182,  182,  182,  182,
      182,  185,  185,  185,  185,  185,  185,  185,  185,  185,

      185,  187,  187,  226,  187,  187,  187,  187,  187,  187,
      187,  189,  189,  189,  189,  189,  189,  189,  189,  189,
      189,  192,  192,  192,  192,  192,  192,  192,  192,  192,
      192,  198,  198,  226,  198,  198,  198,  198,  198,  198,
      204,  204,  204,  204,  204,  204,  204,  204,  204,  204,
      207,  207,  226,  207,  207,  207,  207,  207,  207,  207,
      208,  208,  208,  208,  208,  208,  208,  208,  208,  208,
      211,  211,  211,  211,  211,  211,  211,  211,  211,  211,
      213,  213,  213,  213,  213,  213,  213,  213,  213,  213,
      215,  215,  226,  215,  215,  215,  215,  215,  215,  215,

      226,  226,  226,  226,  226,  226,  226,  226,  226,  226,
      226,  226,  226,  226,  226,  226,  226,  226,  226,  226,
      226,  226,  226,  226,  226,  226,  226,  226,  226,  226
    } ;

static short int c[831] =
    {   0,
        0,    1,    1,   10,   11,   11,  256,   10,   12,   12,
       29,   21,   30,   19,   19,   20,   20,  249,   21,  245,
       21,    1,    1,    1,    1,    1,    1,    2,    2,   19,
        2,   20,    2,   22,   31,   71,   23,   29,    2,   30,
       22,   31,   22,   23,   23,   45,   45,    2,    2,    2,
        2,    2,    2,    3,    3,    3,   32,    3,   24,  220,
        3,    3,   71,   32,    3,   24,   24,    3,   33,    3,
        3,   34,   55,   55,  216,   33,  213,   35,   34,    3,
        3,    4,    4,    4,   35,    4,    4,   36,    4,    4,
       37,   37,    4,  211,   36,    4,   59,    4,    4,  208,

        4,   38,   38,   59,   83,   83,   37,    4,    4,   13,
       48,   39,   39,   40,   40,   13,   39,   38,   40,  196,
       13,   13,   13,   13,   13,   13,   14,   88,   48,   91,
       48,   48,   14,  100,   88,  192,   91,   14,   14,   14,
       14,   14,   14,   17,  100,   52,   52,  124,  189,   95,
       52,   17,  108,  111,   17,   17,   95,  122,  122,  108,
      111,  188,  122,  128,  128,  144,  144,  152,  139,  139,
       17,   18,   52,  139,  124,  147,  147,  152,  145,   18,
      155,  156,   18,   18,  145,  145,  157,  185,  156,  178,
      155,  165,  165,  167,  167,  139,  157,  173,   18,   25,

       25,  195,  195,  203,   25,  155,  195,  195,  173,   25,
      197,  197,  202,  203,  206,  204,  210,  210,  177,  202,
      161,  206,  218,  218,   25,  204,   25,   26,   26,  217,
      217,  142,   26,  141,  217,  217,  136,   26,  219,  219,
      221,  221,  225,  225,  134,  221,  221,  132,  130,  127,
      125,  121,   26,  116,   26,   60,   60,  113,  110,  104,
      101,   92,   77,   60,   60,   60,   60,   60,   60,   61,
       61,   73,   69,   66,   61,   64,   63,   58,   49,   47,
       61,   42,   28,   27,   16,   15,    9,    6,    5,    0,
        0,    0,    0,    0,    0,    0,   61,  151,  151,    0,

        0,    0,    0,    0,    0,  151,  151,  151,  151,  151,
      151,    0,    0,  151,  153,  153,    0,    0,    0,  153,
        0,    0,    0,    0,    0,  153,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
        0,  153,  227,  227,  227,  227,  227,  227,  227,  227,
      227,  227,  228,  228,  228,  228,  228,  228,  228,  228,
      228,  228,  229,  229,  229,  229,  229,  229,  229,  229,
      229,  229,  230,  230,  230,  230,  230,  230,  230,  230,
      230,  230,  231,  231,  231,  231,  231,  231,  231,  231,
      231,  231,  232,  232,  232,  232,  232,  232,  232,  232,

      232,  232,  233,  233,    0,  233,  233,  233,  233,  233,
      233,  233,  234,  234,  234,  234,  234,  234,  234,  234,
      234,  234,  235,  235,  235,  235,  235,  235,  235,  235,
      235,  235,  236,  236,    0,  236,  236,  236,  236,  236,
      236,  236,  237,  237,    0,  237,  237,  237,  237,  237,
      237,  237,  238,  238,  238,  238,  238,  238,  238,  238,
      238,  238,  239,  239,  239,  239,  239,  239,  239,  239,
      239,  239,  240,  240,  240,  240,  240,  240,  240,  240,
      240,  240,  241,  241,  241,  241,  241,  241,  241,  241,
      241,  241,  242,  242,  242,  242,  242,  242,  242,  242,

      242,  242,  243,  243,  243,  243,  243,  243,  243,  243,
      243,  243,  244,  244,  244,  244,  244,  244,  244,  244,
      244,  244,  246,  246,  246,  246,  246,  246,  246,  246,
      246,  246,  247,  247,  247,  247,  247,  247,  247,  247,
      247,  247,  248,  248,  248,  248,  248,  248,  248,  248,
      248,  248,  250,  250,    0,  250,  250,  250,  250,  250,
        0,  250,  251,  251,    0,  251,  251,  251,  251,  251,
      251,  251,  252,  252,    0,  252,  252,  252,  252,  252,
      252,  252,  253,  253,  253,  253,  253,  253,  253,  253,
      253,  253,  254,    0,    0,  254,  254,  254,  254,  254,

      254,  254,  255,  255,    0,  255,  255,  255,  255,  255,
      255,  255,  257,  257,    0,  257,  257,  257,  257,  257,
      257,  257,  258,  258,    0,  258,  258,  258,  258,  258,
      258,  258,  259,  259,    0,  259,  259,  259,  259,  259,
        0,  259,  260,  260,    0,    0,    0,  260,  260,  260,
      260,  261,  261,    0,  261,  261,  261,  261,  261,  261,
      261,  262,  262,    0,  262,  262,    0,  262,  262,  262,
      262,  263,  263,    0,    0,  263,  263,  263,    0,  263,
      263,  264,  264,    0,  264,  264,  264,  264,  264,  264,
      264,  265,  265,  265,  265,  265,  265,  265,  265,  265,

      265,  266,  266,    0,  266,  266,  266,  266,  266,  266,
      266,  267,  267,  267,  267,  267,  267,  267,  267,  267,
      267,  268,  268,  268,  268,  268,  268,  268,  268,  268,
      268,  269,  269,    0,  269,  269,  269,  269,  269,  269,
      270,  270,  270,  270,  270,  270,  270,  270,  270,  270,
      271,  271,    0,  271,  271,  271,  271,  271,  271,  271,
      272,  272,  272,  272,  272,  272,  272,  272,  272,  272,
      273,  273,  273,  273,  273,  273,  273,  273,  273,  273,
      274,  274,  274,  274,  274,  274,  274,  274,  274,  274,
      275,  275,    0,  275,  275,  275,  275,  275,  275,  275,

      226,  226,  226,  226,  226,  226,  226,  226,  226,  226,
      226,  226,  226,  226,  226,  226,  226,  226,  226,  226,
      226,  226,  226,  226,  226,  226,  226,  226,  226,  226
    } ;

static int unput();

/* these declarations have to come after the section 1 code or lint gets
 * confused about whether the variables are used
 */
FILE *yyin = stdin, *yyout = stdout;

/* these variables are all declared out here so that section 3 code can
 * manipulate them
 */
static int yy_start, yy_b_buf_p, yy_c_buf_p, yy_e_buf_p;
static int yy_saw_eof, yy_init = 1;

/* yy_ch_buf has to be 1 character longer than YY_BUF_SIZE, since when
 * setting up yytext we can try to put a '\0' just past the end of the
 * matched text
 */
static char yy_ch_buf[YY_BUF_SIZE + 1];
static int yy_st_buf[YY_BUF_SIZE];
static char yy_hold_char;
char *yytext;
static int yyleng;

YY_DECL
    {
    int yy_n_chars, yy_lp, yy_iii, yy_buf_pos, yy_act;


    static int bracelevel, didadef;
    int i, cclval;
    char nmdef[MAXLINE], myesc();


    if ( yy_init )
	{
	YY_INIT;
	yy_start = 1;
	yy_init = 0;
	}

    goto get_next_token;

do_action:
    for ( ; ; )
	{
	YY_DO_BEFORE_ACTION

#ifdef FLEX_DEBUG
	fprintf( stderr, "--accepting rule #%d\n", yy_act );
#endif
	switch ( yy_act )
	    {
case 1:
# line 58 "scan.l"
++linenum; ECHO; /* indented code */
	YY_BREAK
case 2:
# line 59 "scan.l"
++linenum; ECHO; /* treat as a comment */
	YY_BREAK
case 3:
# line 60 "scan.l"
ECHO; BEGIN(C_COMMENT);
	YY_BREAK
case 4:
# line 61 "scan.l"
return ( SCDECL );
	YY_BREAK
case 5:
# line 62 "scan.l"
return ( XSCDECL );
	YY_BREAK
case 6:
# line 63 "scan.l"
++linenum; line_directive_out( stdout ); BEGIN(CODEBLOCK);
	YY_BREAK
case 7:
# line 64 "scan.l"
return ( WHITESPACE );
	YY_BREAK
case 8:
# line 66 "scan.l"
{
			sectnum = 2;
			line_directive_out( stdout );
			BEGIN(SECT2PROLOG);
			return ( SECTEND );
			}
	YY_BREAK
case 9:
# line 73 "scan.l"
{
			fprintf( stderr,
			     "old-style lex command at line %d ignored:\n\t%s",
				 linenum, yytext );
			++linenum;
			}
	YY_BREAK
case 10:
# line 80 "scan.l"
{
			(void) strcpy( nmstr, yytext );
			didadef = false;
			BEGIN(PICKUPDEF);
			}
	YY_BREAK
case 11:
# line 86 "scan.l"
RETURNNAME;
	YY_BREAK
case 12:
# line 87 "scan.l"
++linenum; /* allows blank lines in section 1 */
	YY_BREAK
case 13:
# line 88 "scan.l"
++linenum; return ( '\n' );
	YY_BREAK
case 14:
# line 89 "scan.l"
synerr( "illegal character" ); BEGIN(RECOVER);
	YY_BREAK
case 15:
# line 92 "scan.l"
ECHO; BEGIN(0);
	YY_BREAK
case 16:
# line 93 "scan.l"
++linenum; ECHO; BEGIN(0);
	YY_BREAK
case 17:
# line 94 "scan.l"
ECHO;
	YY_BREAK
case 18:
# line 95 "scan.l"
ECHO;
	YY_BREAK
case 19:
# line 96 "scan.l"
++linenum; ECHO;
	YY_BREAK
case 20:
# line 98 "scan.l"
++linenum; BEGIN(0);
	YY_BREAK
case 21:
# line 99 "scan.l"
++linenum; ECHO;
	YY_BREAK
case 22:
# line 101 "scan.l"
/* separates name and definition */
	YY_BREAK
case 23:
# line 103 "scan.l"
{
			(void) strcpy( nmdef, yytext );

			for ( i = strlen( nmdef ) - 1;
			      i >= 0 &&
			      nmdef[i] == ' ' || nmdef[i] == '\t';
			      --i )
			    ;

			nmdef[i + 1] = '\0';

                        ndinstal( nmstr, nmdef );
			didadef = true;
			}
	YY_BREAK
case 24:
# line 118 "scan.l"
{
			if ( ! didadef )
			    synerr( "incomplete name definition" );
			BEGIN(0);
			++linenum;
			}
	YY_BREAK
case 25:
# line 125 "scan.l"
++linenum; BEGIN(0); RETURNNAME;
	YY_BREAK
case 26:
YY_DO_BEFORE_SCAN; /* undo effects of setting up yytext */
yy_c_buf_p -= 1;
YY_DO_BEFORE_ACTION; /* set up yytext again */
# line 128 "scan.l"
{
			++linenum;
			ACTION_ECHO;
			MARK_END_OF_PROLOG;
			BEGIN(SECT2);
			}
	YY_BREAK
case 27:
# line 135 "scan.l"
++linenum; ACTION_ECHO;
	YY_BREAK
case 28:
# line 137 "scan.l"
++linenum; /* allow blank lines in section 2 */
	YY_BREAK
	/* this horrible mess of a rule matches indented lines which
	 * do not contain "/*".  We need to make the distinction because
	 * otherwise this rule will be taken instead of the rule which
	 * matches the beginning of comments like this one
	 */
case 29:
# line 144 "scan.l"
{
			synerr( "indented code found outside of action" );
			++linenum;
			}
	YY_BREAK
case 30:
# line 149 "scan.l"
BEGIN(SC); return ( '<' );
	YY_BREAK
case 31:
# line 150 "scan.l"
return ( '^' );
	YY_BREAK
case 32:
# line 151 "scan.l"
BEGIN(QUOTE); return ( '"' );
	YY_BREAK
case 33:
YY_DO_BEFORE_SCAN; /* undo effects of setting up yytext */
yy_c_buf_p = yy_b_buf_p;
YY_DO_BEFORE_ACTION; /* set up yytext again */
# line 152 "scan.l"
BEGIN(NUM); return ( '{' );
	YY_BREAK
case 34:
# line 153 "scan.l"
BEGIN(BRACEERROR);
	YY_BREAK
case 35:
YY_DO_BEFORE_SCAN; /* undo effects of setting up yytext */
yy_c_buf_p = yy_b_buf_p;
YY_DO_BEFORE_ACTION; /* set up yytext again */
# line 154 "scan.l"
return ( '$' );
	YY_BREAK
case 36:
# line 156 "scan.l"
{
			bracelevel = 1;
			BEGIN(PERCENT_BRACE_ACTION);
			return ( '\n' );
			}
	YY_BREAK
case 37:
# line 161 "scan.l"
++linenum; return ( '\n' );
	YY_BREAK
case 38:
# line 163 "scan.l"
ACTION_ECHO; BEGIN(C_COMMENT_2);
	YY_BREAK
case 39:
# line 165 "scan.l"
{ /* needs to be separate from following rule due to
			   * bug with trailing context
			   */
			bracelevel = 0;
			BEGIN(ACTION);
			return ( '\n' );
			}
	YY_BREAK
case 40:
YY_DO_BEFORE_SCAN; /* undo effects of setting up yytext */
yy_c_buf_p -= 1;
YY_DO_BEFORE_ACTION; /* set up yytext again */
# line 173 "scan.l"
{
			bracelevel = 0;
			BEGIN(ACTION);
			return ( '\n' );
			}
	YY_BREAK
case 41:
# line 179 "scan.l"
++linenum; return ( '\n' );
	YY_BREAK
case 42:
# line 181 "scan.l"
{
			/* guarantee that the SECT3 rule will have something
			 * to match
			 */
			yyless(1);
			sectnum = 3;
			BEGIN(SECT3);
			return ( EOF ); /* to stop the parser */
			}
	YY_BREAK
case 43:
# line 191 "scan.l"
{
			(void) strcpy( nmstr, yytext );

			/* check to see if we've already encountered this ccl */
			if ( (cclval = ccllookup( nmstr )) )
			    {
			    yylval = cclval;
			    ++cclreuse;
			    return ( PREVCCL );
			    }
			else
			    {
			    /* we fudge a bit.  We know that this ccl will
			     * soon be numbered as lastccl + 1 by cclinit
			     */
			    cclinstal( nmstr, lastccl + 1 );

			    /* push back everything but the leading bracket
			     * so the ccl can be rescanned
			     */
			    PUT_BACK_STRING(nmstr, 1);

			    BEGIN(FIRSTCCL);
			    return ( '[' );
			    }
			}
	YY_BREAK
case 44:
# line 218 "scan.l"
{
			register char *nmdefptr;
			char *ndlookup();

			(void) strcpy( nmstr, yytext );
			nmstr[yyleng - 1] = '\0';  /* chop trailing brace */

			/* lookup from "nmstr + 1" to chop leading brace */
			if ( ! (nmdefptr = ndlookup( nmstr + 1 )) )
			    synerr( "undefined {name}" );

			else
			    { /* push back name surrounded by ()'s */
			    unput(')');
			    PUT_BACK_STRING(nmdefptr, 0);
			    unput('(');
			    }
			}
	YY_BREAK
case 45:
# line 237 "scan.l"
return ( yytext[0] );
	YY_BREAK
case 46:
# line 238 "scan.l"
RETURNCHAR;
	YY_BREAK
case 47:
# line 239 "scan.l"
++linenum; return ( '\n' );
	YY_BREAK
case 48:
# line 242 "scan.l"
return ( ',' );
	YY_BREAK
case 49:
# line 243 "scan.l"
BEGIN(SECT2); return ( '>' );
	YY_BREAK
case 50:
YY_DO_BEFORE_SCAN; /* undo effects of setting up yytext */
yy_c_buf_p = yy_b_buf_p;
YY_DO_BEFORE_ACTION; /* set up yytext again */
# line 244 "scan.l"
BEGIN(CARETISBOL); return ( '>' );
	YY_BREAK
case 51:
# line 245 "scan.l"
RETURNNAME;
	YY_BREAK
case 52:
# line 246 "scan.l"
synerr( "bad start condition name" );
	YY_BREAK
case 53:
# line 248 "scan.l"
BEGIN(SECT2); return ( '^' );
	YY_BREAK
case 54:
# line 251 "scan.l"
RETURNCHAR;
	YY_BREAK
case 55:
# line 252 "scan.l"
BEGIN(SECT2); return ( '"' );
	YY_BREAK
case 56:
# line 254 "scan.l"
{
			synerr( "missing quote" );
			BEGIN(SECT2);
			++linenum;
			return ( '"' );
			}
	YY_BREAK
case 57:
YY_DO_BEFORE_SCAN; /* undo effects of setting up yytext */
yy_c_buf_p = yy_b_buf_p;
YY_DO_BEFORE_ACTION; /* set up yytext again */
# line 262 "scan.l"
BEGIN(CCL); return ( '^' );
	YY_BREAK
case 58:
YY_DO_BEFORE_SCAN; /* undo effects of setting up yytext */
yy_c_buf_p = yy_b_buf_p;
YY_DO_BEFORE_ACTION; /* set up yytext again */
# line 263 "scan.l"
return ( '^' );
	YY_BREAK
case 59:
# line 264 "scan.l"
BEGIN(CCL); yylval = '-'; return ( CHAR );
	YY_BREAK
case 60:
# line 265 "scan.l"
BEGIN(CCL); RETURNCHAR;
	YY_BREAK
case 61:
YY_DO_BEFORE_SCAN; /* undo effects of setting up yytext */
yy_c_buf_p = yy_b_buf_p;
YY_DO_BEFORE_ACTION; /* set up yytext again */
# line 267 "scan.l"
return ( '-' );
	YY_BREAK
case 62:
# line 268 "scan.l"
RETURNCHAR;
	YY_BREAK
case 63:
# line 269 "scan.l"
BEGIN(SECT2); return ( ']' );
	YY_BREAK
case 64:
# line 272 "scan.l"
{
			yylval = myctoi( yytext );
			return ( NUMBER );
			}
	YY_BREAK
case 65:
# line 277 "scan.l"
return ( ',' );
	YY_BREAK
case 66:
# line 278 "scan.l"
BEGIN(SECT2); return ( '}' );
	YY_BREAK
case 67:
# line 280 "scan.l"
{
			synerr( "bad character inside {}'s" );
			BEGIN(SECT2);
			return ( '}' );
			}
	YY_BREAK
case 68:
# line 286 "scan.l"
{
			synerr( "missing }" );
			BEGIN(SECT2);
			++linenum;
			return ( '}' );
			}
	YY_BREAK
case 69:
# line 294 "scan.l"
synerr( "bad name in {}'s" ); BEGIN(SECT2);
	YY_BREAK
case 70:
# line 295 "scan.l"
synerr( "missing }" ); ++linenum; BEGIN(SECT2);
	YY_BREAK
case 71:
# line 298 "scan.l"
bracelevel = 0;
	YY_BREAK
case 72:
# line 299 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 73:
# line 300 "scan.l"
{
			++linenum;
			ACTION_ECHO;
			if ( bracelevel == 0 )
			    {
			    fputs( "\tYY_BREAK\n", temp_action_file );
			    BEGIN(SECT2);
			    }
			}
	YY_BREAK
case 74:
# line 310 "scan.l"
ACTION_ECHO; ++bracelevel;
	YY_BREAK
case 75:
# line 311 "scan.l"
ACTION_ECHO; --bracelevel;
	YY_BREAK
case 76:
# line 312 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 77:
# line 313 "scan.l"
ACTION_ECHO; BEGIN(ACTION_COMMENT);
	YY_BREAK
case 78:
# line 314 "scan.l"
ACTION_ECHO; /* character constant */
	YY_BREAK
case 79:
# line 315 "scan.l"
ACTION_ECHO; BEGIN(ACTION_STRING);
	YY_BREAK
case 80:
# line 316 "scan.l"
{
			++linenum;
			ACTION_ECHO;
			if ( bracelevel == 0 )
			    {
			    fputs( "\tYY_BREAK\n", temp_action_file );
			    BEGIN(SECT2);
			    }
			}
	YY_BREAK
case 81:
# line 325 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 82:
# line 327 "scan.l"
ACTION_ECHO; BEGIN(ACTION);
	YY_BREAK
case 83:
# line 328 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 84:
# line 329 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 85:
# line 330 "scan.l"
++linenum; ACTION_ECHO;
	YY_BREAK
case 86:
# line 331 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 87:
# line 333 "scan.l"
ACTION_ECHO; BEGIN(SECT2);
	YY_BREAK
case 88:
# line 334 "scan.l"
++linenum; ACTION_ECHO; BEGIN(SECT2);
	YY_BREAK
case 89:
# line 335 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 90:
# line 336 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 91:
# line 337 "scan.l"
++linenum; ACTION_ECHO;
	YY_BREAK
case 92:
# line 339 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 93:
# line 340 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 94:
# line 341 "scan.l"
++linenum; ACTION_ECHO;
	YY_BREAK
case 95:
# line 342 "scan.l"
ACTION_ECHO; BEGIN(ACTION);
	YY_BREAK
case 96:
# line 343 "scan.l"
ACTION_ECHO;
	YY_BREAK
case 97:
# line 346 "scan.l"
{
			yylval = myesc( yytext );
			return ( CHAR );
			}
	YY_BREAK
case 98:
# line 351 "scan.l"
{
			yylval = myesc( yytext );
			BEGIN(CCL);
			return ( CHAR );
			}
	YY_BREAK
case 99:
# line 358 "scan.l"
{
			register int numchars;

			/* black magic - we know the names of a flex scanner's
			 * internal variables.  We cap the input buffer with
			 * an end-of-string and dump it to the output.
			 */
			YY_DO_BEFORE_SCAN; /* recover from setting up yytext */

#ifdef FLEX_FAST_SKEL
			fputs( yy_c_buf_p + 1, stdout );
#else
			yy_ch_buf[yy_e_buf_p + 1] = '\0';

			/* ignore the first character; it's the second '%'
			 * put back by the yyless(1) above
			 */
			fputs( yy_ch_buf + yy_c_buf_p + 1, stdout );
#endif

			/* if we don't do this, the data written by write()
			 * can get overwritten when stdout is finally flushed
			 */
			(void) fflush( stdout );

			while ( (numchars = read( fileno(yyin), yy_ch_buf,
						  YY_BUF_MAX )) > 0 )
			    (void) write( fileno(stdout), yy_ch_buf, numchars );
	
			if ( numchars < 0 )
			    flexerror( "fatal read error in section 3" );

			return ( EOF );
			}
	YY_BREAK

case YY_NEW_FILE:
break; /* begin reading from new file */

case YY_DO_DEFAULT:
YY_DEFAULT_ACTION;
break;

case YY_END_TOK:
return ( YY_END_TOK );

default:
YY_FATAL_ERROR( "fatal flex scanner internal error" );
	    }

get_next_token:
	{
	register int yy_curst;
	register char yy_sym;

	YY_DO_BEFORE_SCAN

	/* set up to begin running DFA */

	yy_curst = yy_start;

	if ( yy_ch_buf[yy_c_buf_p] == '\n' )
	    ++yy_curst;

	/* yy_b_buf_p points to the position in yy_ch_buf
	 * of the start of the current run.
	 */

	yy_b_buf_p = yy_c_buf_p + 1;

	do /* until the machine jams */
	    {
	    if ( yy_c_buf_p == yy_e_buf_p )
		{ /* need more input */
		if ( yy_e_buf_p >= YY_BUF_LIM )
		    { /* not enough room to do another read */
		    /* see if we can make some room for more chars */

		    yy_n_chars = yy_e_buf_p - yy_b_buf_p;

		    if ( yy_n_chars >= 0 )
			/* shift down buffer to make room */
			for ( yy_iii = 0; yy_iii <= yy_n_chars; ++yy_iii )
			    {
			    yy_buf_pos = yy_b_buf_p + yy_iii;
			    yy_ch_buf[yy_iii] = yy_ch_buf[yy_buf_pos];
			    yy_st_buf[yy_iii] = yy_st_buf[yy_buf_pos];
			    }

		    yy_b_buf_p = 0;
		    yy_e_buf_p = yy_n_chars;

		    if ( yy_e_buf_p >= YY_BUF_LIM )
			YY_FATAL_ERROR( "flex input buffer overflowed" );

		    yy_c_buf_p = yy_e_buf_p;
		    }

		else if ( yy_saw_eof )
		    {
saweof:		    if ( yy_b_buf_p > yy_e_buf_p )
			{
			if ( yywrap() )
			    {
			    yy_act = YY_END_TOK;
			    goto do_action;
			    }
			
			else
			    {
			    YY_INIT;
			    yy_act = YY_NEW_FILE;
			    goto do_action;
			    }
			}

		    else /* do a jam to eat up more input */
			{
#ifndef FLEX_INTERACTIVE_SCANNER
			/* we're going to decrement yy_c_buf_p upon doing
			 * the jam.  In this case, that's wrong, since
			 * it points to the last non-jam character.  So
			 * we increment it now to counter the decrement.
			 */
			++yy_c_buf_p;
#endif
			break;
			}
		    }

		YY_INPUT( (yy_ch_buf + yy_c_buf_p + 1), yy_n_chars,
			  YY_MAX_LINE );

		if ( yy_n_chars == YY_NULL )
		    {
		    if ( yy_saw_eof )
	YY_FATAL_ERROR( "flex scanner saw EOF twice - shouldn't happen" );
		    yy_saw_eof = 1;
		    goto saweof;
		    }

		yy_e_buf_p += yy_n_chars;
		}

	    ++yy_c_buf_p;

#ifdef FLEX_USE_ECS
	    yy_sym = e[yy_ch_buf[yy_c_buf_p]];
#else
	    yy_sym = yy_ch_buf[yy_c_buf_p];
#endif

#ifdef FLEX_FULL_TABLE
	    yy_curst = n[yy_curst][yy_sym];

#else /* get next state from compressed table */

	    while ( c[b[yy_curst] + yy_sym] != yy_curst )
		{
		yy_curst = d[yy_curst];

#ifdef FLEX_USE_MECS
		/* we've arrange it so that templates are never chained
		 * to one another.  This means we can afford make a
		 * very simple test to see if we need to convert to
		 * yy_sym's meta-equivalence class without worrying
		 * about erroneously looking up the meta-equivalence
		 * class twice
		 */

		if ( yy_curst >= YY_TEMPLATE )
		    yy_sym = m[yy_sym];
#endif
		}

	    yy_curst = n[b[yy_curst] + yy_sym];

#endif

	    yy_st_buf[yy_c_buf_p] = yy_curst;

	    }
#ifdef FLEX_INTERACTIVE_SCANNER
	while ( b[yy_curst] != YY_JAM_BASE );
#else
	while ( yy_curst != YY_JAM );
	--yy_c_buf_p; /* put back character we jammed on */

#endif

	if ( yy_c_buf_p >= yy_b_buf_p )
	    { /* we matched some text */
	    yy_curst = yy_st_buf[yy_c_buf_p];
	    yy_lp = l[yy_curst];

#ifdef FLEX_REJECT_ENABLED
find_rule: /* we branch to this label when doing a REJECT */
#endif

	    for ( ; ; ) /* until we find what rule we matched */
		{
#ifdef FLEX_REJECT_ENABLED
		if ( yy_lp && yy_lp < l[yy_curst + 1] )
		    {
		    yy_act = a[yy_lp];
		    goto do_action; /* "continue 2" */
		    }
#else
		if ( yy_lp )
		    {
		    yy_act = yy_lp;
		    goto do_action; /* "continue 2" */
		    }
#endif

		if ( --yy_c_buf_p < yy_b_buf_p )
		    break;

		yy_curst = yy_st_buf[yy_c_buf_p];
		yy_lp = l[yy_curst];
		}
	    }

	/* if we got this far, then we didn't find any accepting
	 * states
	 */

	/* so that the default applies to the first char read */
	++yy_c_buf_p;

	yy_act = YY_DO_DEFAULT;
	}
	}

    /*NOTREACHED*/
    }


static int unput( c )
char c;

    {
    YY_DO_BEFORE_SCAN; /* undo effects of setting up yytext */

    if ( yy_c_buf_p == 0 )
	{
	register int i;
	register int yy_buf_pos = YY_BUF_MAX;

	for ( i = yy_e_buf_p; i >= yy_c_buf_p; --i )
	    {
	    yy_ch_buf[yy_buf_pos] = yy_ch_buf[i];
	    yy_st_buf[yy_buf_pos] = yy_st_buf[i];
	    --yy_buf_pos;
	    }

	yy_c_buf_p = YY_BUF_MAX - yy_e_buf_p;
	yy_e_buf_p = YY_BUF_MAX;
	}

    if ( yy_c_buf_p <= 0 )
	YY_FATAL_ERROR( "flex scanner push-back overflow" );

    if ( yy_c_buf_p >= yy_b_buf_p && yy_ch_buf[yy_c_buf_p] == '\n' )
	yy_ch_buf[yy_c_buf_p - 1] = '\n';

    yy_ch_buf[yy_c_buf_p--] = c;

    YY_DO_BEFORE_ACTION; /* set up yytext again */
    }


static int input()

    {
    int c;

    YY_DO_BEFORE_SCAN

    if ( yy_c_buf_p == yy_e_buf_p )
	{ /* need more input */
	int yy_n_chars;

	/* we can throw away the entire current buffer */
	if ( yy_saw_eof )
	    {
	    if ( yywrap() )
		return ( EOF );

	    YY_INIT;
	    }

	yy_b_buf_p = 0;
	YY_INPUT( yy_ch_buf, yy_n_chars, YY_MAX_LINE );

	if ( yy_n_chars == YY_NULL )
	    {
	    yy_saw_eof = 1;

	    if ( yywrap() )
		return ( EOF );

	    YY_INIT;

	    return ( input() );
	    }

	yy_c_buf_p = -1;
	yy_e_buf_p = yy_n_chars - 1;
	}

    c = yy_ch_buf[++yy_c_buf_p];

    YY_DO_BEFORE_ACTION;

    return ( c );
    }
# line 392 "scan.l"
