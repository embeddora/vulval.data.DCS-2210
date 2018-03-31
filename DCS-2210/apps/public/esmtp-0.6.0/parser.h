/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     IDENTITY = 258,
     DEFAULT = 259,
     HOSTNAME = 260,
     USERNAME = 261,
     PASSWORD = 262,
     STARTTLS = 263,
     CERTIFICATE_PASSPHRASE = 264,
     PRECONNECT = 265,
     POSTCONNECT = 266,
     MDA = 267,
     QUALIFYDOMAIN = 268,
     HELO = 269,
     FORCE = 270,
     SENDER = 271,
     REVERSE_PATH = 272,
     MAP = 273,
     DISABLED = 274,
     ENABLED = 275,
     REQUIRED = 276,
     STRING = 277,
     NUMBER = 278
   };
#endif
/* Tokens.  */
#define IDENTITY 258
#define DEFAULT 259
#define HOSTNAME 260
#define USERNAME 261
#define PASSWORD 262
#define STARTTLS 263
#define CERTIFICATE_PASSPHRASE 264
#define PRECONNECT 265
#define POSTCONNECT 266
#define MDA 267
#define QUALIFYDOMAIN 268
#define HELO 269
#define FORCE 270
#define SENDER 271
#define REVERSE_PATH 272
#define MAP 273
#define DISABLED 274
#define ENABLED 275
#define REQUIRED 276
#define STRING 277
#define NUMBER 278




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 62 "parser.y"
typedef union YYSTYPE {
    int number;
    char *sval;
} YYSTYPE;
/* Line 1447 of yacc.c.  */
#line 89 "parser.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



