/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_MACHINEINDEPENDENT_GLSLANG_TAB_CPP_H_INCLUDED
# define YY_YY_MACHINEINDEPENDENT_GLSLANG_TAB_CPP_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    ATTRIBUTE = 258,
    VARYING = 259,
    CONST = 260,
    BOOL = 261,
    FLOAT = 262,
    DOUBLE = 263,
    INT = 264,
    UINT = 265,
    INT64_T = 266,
    UINT64_T = 267,
    BREAK = 268,
    CONTINUE = 269,
    DO = 270,
    ELSE = 271,
    FOR = 272,
    IF = 273,
    DISCARD = 274,
    RETURN = 275,
    SWITCH = 276,
    CASE = 277,
    DEFAULT = 278,
    SUBROUTINE = 279,
    BVEC2 = 280,
    BVEC3 = 281,
    BVEC4 = 282,
    IVEC2 = 283,
    IVEC3 = 284,
    IVEC4 = 285,
    I64VEC2 = 286,
    I64VEC3 = 287,
    I64VEC4 = 288,
    UVEC2 = 289,
    UVEC3 = 290,
    UVEC4 = 291,
    U64VEC2 = 292,
    U64VEC3 = 293,
    U64VEC4 = 294,
    VEC2 = 295,
    VEC3 = 296,
    VEC4 = 297,
    MAT2 = 298,
    MAT3 = 299,
    MAT4 = 300,
    CENTROID = 301,
    IN = 302,
    OUT = 303,
    INOUT = 304,
    UNIFORM = 305,
    PATCH = 306,
    SAMPLE = 307,
    BUFFER = 308,
    SHARED = 309,
    COHERENT = 310,
    VOLATILE = 311,
    RESTRICT = 312,
    READONLY = 313,
    WRITEONLY = 314,
    DVEC2 = 315,
    DVEC3 = 316,
    DVEC4 = 317,
    DMAT2 = 318,
    DMAT3 = 319,
    DMAT4 = 320,
    NOPERSPECTIVE = 321,
    FLAT = 322,
    SMOOTH = 323,
    LAYOUT = 324,
    MAT2X2 = 325,
    MAT2X3 = 326,
    MAT2X4 = 327,
    MAT3X2 = 328,
    MAT3X3 = 329,
    MAT3X4 = 330,
    MAT4X2 = 331,
    MAT4X3 = 332,
    MAT4X4 = 333,
    DMAT2X2 = 334,
    DMAT2X3 = 335,
    DMAT2X4 = 336,
    DMAT3X2 = 337,
    DMAT3X3 = 338,
    DMAT3X4 = 339,
    DMAT4X2 = 340,
    DMAT4X3 = 341,
    DMAT4X4 = 342,
    ATOMIC_UINT = 343,
    SAMPLER1D = 344,
    SAMPLER2D = 345,
    SAMPLER3D = 346,
    SAMPLERCUBE = 347,
    SAMPLER1DSHADOW = 348,
    SAMPLER2DSHADOW = 349,
    SAMPLERCUBESHADOW = 350,
    SAMPLER1DARRAY = 351,
    SAMPLER2DARRAY = 352,
    SAMPLER1DARRAYSHADOW = 353,
    SAMPLER2DARRAYSHADOW = 354,
    ISAMPLER1D = 355,
    ISAMPLER2D = 356,
    ISAMPLER3D = 357,
    ISAMPLERCUBE = 358,
    ISAMPLER1DARRAY = 359,
    ISAMPLER2DARRAY = 360,
    USAMPLER1D = 361,
    USAMPLER2D = 362,
    USAMPLER3D = 363,
    USAMPLERCUBE = 364,
    USAMPLER1DARRAY = 365,
    USAMPLER2DARRAY = 366,
    SAMPLER2DRECT = 367,
    SAMPLER2DRECTSHADOW = 368,
    ISAMPLER2DRECT = 369,
    USAMPLER2DRECT = 370,
    SAMPLERBUFFER = 371,
    ISAMPLERBUFFER = 372,
    USAMPLERBUFFER = 373,
    SAMPLERCUBEARRAY = 374,
    SAMPLERCUBEARRAYSHADOW = 375,
    ISAMPLERCUBEARRAY = 376,
    USAMPLERCUBEARRAY = 377,
    SAMPLER2DMS = 378,
    ISAMPLER2DMS = 379,
    USAMPLER2DMS = 380,
    SAMPLER2DMSARRAY = 381,
    ISAMPLER2DMSARRAY = 382,
    USAMPLER2DMSARRAY = 383,
    SAMPLEREXTERNALOES = 384,
    SAMPLER = 385,
    SAMPLERSHADOW = 386,
    TEXTURE1D = 387,
    TEXTURE2D = 388,
    TEXTURE3D = 389,
    TEXTURECUBE = 390,
    TEXTURE1DARRAY = 391,
    TEXTURE2DARRAY = 392,
    ITEXTURE1D = 393,
    ITEXTURE2D = 394,
    ITEXTURE3D = 395,
    ITEXTURECUBE = 396,
    ITEXTURE1DARRAY = 397,
    ITEXTURE2DARRAY = 398,
    UTEXTURE1D = 399,
    UTEXTURE2D = 400,
    UTEXTURE3D = 401,
    UTEXTURECUBE = 402,
    UTEXTURE1DARRAY = 403,
    UTEXTURE2DARRAY = 404,
    TEXTURE2DRECT = 405,
    ITEXTURE2DRECT = 406,
    UTEXTURE2DRECT = 407,
    TEXTUREBUFFER = 408,
    ITEXTUREBUFFER = 409,
    UTEXTUREBUFFER = 410,
    TEXTURECUBEARRAY = 411,
    ITEXTURECUBEARRAY = 412,
    UTEXTURECUBEARRAY = 413,
    TEXTURE2DMS = 414,
    ITEXTURE2DMS = 415,
    UTEXTURE2DMS = 416,
    TEXTURE2DMSARRAY = 417,
    ITEXTURE2DMSARRAY = 418,
    UTEXTURE2DMSARRAY = 419,
    SUBPASSINPUT = 420,
    SUBPASSINPUTMS = 421,
    ISUBPASSINPUT = 422,
    ISUBPASSINPUTMS = 423,
    USUBPASSINPUT = 424,
    USUBPASSINPUTMS = 425,
    IMAGE1D = 426,
    IIMAGE1D = 427,
    UIMAGE1D = 428,
    IMAGE2D = 429,
    IIMAGE2D = 430,
    UIMAGE2D = 431,
    IMAGE3D = 432,
    IIMAGE3D = 433,
    UIMAGE3D = 434,
    IMAGE2DRECT = 435,
    IIMAGE2DRECT = 436,
    UIMAGE2DRECT = 437,
    IMAGECUBE = 438,
    IIMAGECUBE = 439,
    UIMAGECUBE = 440,
    IMAGEBUFFER = 441,
    IIMAGEBUFFER = 442,
    UIMAGEBUFFER = 443,
    IMAGE1DARRAY = 444,
    IIMAGE1DARRAY = 445,
    UIMAGE1DARRAY = 446,
    IMAGE2DARRAY = 447,
    IIMAGE2DARRAY = 448,
    UIMAGE2DARRAY = 449,
    IMAGECUBEARRAY = 450,
    IIMAGECUBEARRAY = 451,
    UIMAGECUBEARRAY = 452,
    IMAGE2DMS = 453,
    IIMAGE2DMS = 454,
    UIMAGE2DMS = 455,
    IMAGE2DMSARRAY = 456,
    IIMAGE2DMSARRAY = 457,
    UIMAGE2DMSARRAY = 458,
    STRUCT = 459,
    VOID = 460,
    WHILE = 461,
    IDENTIFIER = 462,
    TYPE_NAME = 463,
    FLOATCONSTANT = 464,
    DOUBLECONSTANT = 465,
    INTCONSTANT = 466,
    UINTCONSTANT = 467,
    INT64CONSTANT = 468,
    UINT64CONSTANT = 469,
    BOOLCONSTANT = 470,
    LEFT_OP = 471,
    RIGHT_OP = 472,
    INC_OP = 473,
    DEC_OP = 474,
    LE_OP = 475,
    GE_OP = 476,
    EQ_OP = 477,
    NE_OP = 478,
    AND_OP = 479,
    OR_OP = 480,
    XOR_OP = 481,
    MUL_ASSIGN = 482,
    DIV_ASSIGN = 483,
    ADD_ASSIGN = 484,
    MOD_ASSIGN = 485,
    LEFT_ASSIGN = 486,
    RIGHT_ASSIGN = 487,
    AND_ASSIGN = 488,
    XOR_ASSIGN = 489,
    OR_ASSIGN = 490,
    SUB_ASSIGN = 491,
    LEFT_PAREN = 492,
    RIGHT_PAREN = 493,
    LEFT_BRACKET = 494,
    RIGHT_BRACKET = 495,
    LEFT_BRACE = 496,
    RIGHT_BRACE = 497,
    DOT = 498,
    COMMA = 499,
    COLON = 500,
    EQUAL = 501,
    SEMICOLON = 502,
    BANG = 503,
    DASH = 504,
    TILDE = 505,
    PLUS = 506,
    STAR = 507,
    SLASH = 508,
    PERCENT = 509,
    LEFT_ANGLE = 510,
    RIGHT_ANGLE = 511,
    VERTICAL_BAR = 512,
    CARET = 513,
    AMPERSAND = 514,
    QUESTION = 515,
    INVARIANT = 516,
    PRECISE = 517,
    HIGH_PRECISION = 518,
    MEDIUM_PRECISION = 519,
    LOW_PRECISION = 520,
    PRECISION = 521,
    PACKED = 522,
    RESOURCE = 523,
    SUPERP = 524
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 66 "MachineIndependent/glslang.y" /* yacc.c:1909  */

    struct {
        glslang::TSourceLoc loc;
        union {
            glslang::TString *string;
            int i;
            unsigned int u;
            long long i64;
            unsigned long long u64;
            bool b;
            double d;
        };
        glslang::TSymbol* symbol;
    } lex;
    struct {
        glslang::TSourceLoc loc;
        glslang::TOperator op;
        union {
            TIntermNode* intermNode;
            glslang::TIntermNodePair nodePair;
            glslang::TIntermTyped* intermTypedNode;
        };
        union {
            glslang::TPublicType type;
            glslang::TFunction* function;
            glslang::TParameter param;
            glslang::TTypeLoc typeLine;
            glslang::TTypeList* typeList;
            glslang::TArraySizes* arraySizes;
            glslang::TIdentifierList* identifierList;
        };
    } interm;

#line 358 "MachineIndependent/glslang_tab.cpp.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int yyparse (glslang::TParseContext* pParseContext);

#endif /* !YY_YY_MACHINEINDEPENDENT_GLSLANG_TAB_CPP_H_INCLUDED  */
