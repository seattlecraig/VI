/****************************************************************************
*
* Description:  Text object support (iw, aw, i", a", i{, a{, etc.)
*
*               Text objects select ranges of text based on structural
*               boundaries (words, quotes, brackets, paragraphs).
*               They are used with operators: diw, ci", ya{, etc.
*
*               Each function sets range start/end (1-based columns)
*               with line_based and fix_range appropriately. The caller
*               (doOperator in editmain.c) then passes the range through
*               NormalizeRange and on to the operator function.
*
****************************************************************************/

#include "vi.h"

/*
 * charIsWord - test if character is a word character using the
 * current word definition (same logic as word.c charType).
 */
static bool charIsWord( char c )
{
    if( c == 0 ) {
        return( FALSE );
    }
    if( isspace( (unsigned char)c ) ) {
        return( FALSE );
    }
    return( TestIfCharInRange( c, WordDefn ) );
}

/*
 * charIsBigWord - test if character is a WORD character
 * (anything that isn't whitespace or end-of-line).
 */
static bool charIsBigWord( char c )
{
    if( c == 0 ) {
        return( FALSE );
    }
    return( !isspace( (unsigned char)c ) );
}

/*
 * TextObj_Word - select inner/around word or WORD at cursor.
 *
 * "inner" selects just the word characters.
 * "around" extends to include trailing whitespace (or leading
 * whitespace if the word is at the end of the line).
 */
vi_rc TextObj_Word( range *r, bool inner, bool big )
{
    int         sc, ec, len;
    char        *data;
    line        *cline;
    fcb         *cfcb;
    vi_rc       rc;
    bool        (*isWord)( char );

    if( CurrentLine == NULL ) {
        return( ERR_NO_FILE );
    }
    rc = CGimmeLinePtr( CurrentPos.line, &cfcb, &cline );
    if( rc != ERR_NO_ERR ) {
        return( rc );
    }

    isWord = big ? charIsBigWord : charIsWord;
    data = cline->data;
    len = cline->len;

    /* Current cursor column (0-based index into data[]) */
    sc = CurrentPos.column - 1;
    if( sc < 0 ) sc = 0;
    if( sc >= len ) sc = len - 1;
    if( len == 0 ) {
        return( ERR_NO_WORD_TO_FIND );
    }

    if( isWord( data[sc] ) ) {
        /* Cursor is on a word character — find word boundaries */
        while( sc > 0 && isWord( data[sc - 1] ) ) {
            sc--;
        }
        ec = CurrentPos.column - 1;
        while( ec < len - 1 && isWord( data[ec + 1] ) ) {
            ec++;
        }
    } else if( isspace( (unsigned char)data[sc] ) ) {
        /* Cursor is on whitespace — select the whitespace block */
        while( sc > 0 && isspace( (unsigned char)data[sc - 1] ) ) {
            sc--;
        }
        ec = CurrentPos.column - 1;
        while( ec < len - 1 && isspace( (unsigned char)data[ec + 1] ) ) {
            ec++;
        }
    } else {
        /* Cursor is on a delimiter/punctuation character */
        while( sc > 0 && !isWord( data[sc - 1] ) &&
               !isspace( (unsigned char)data[sc - 1] ) && data[sc - 1] != 0 ) {
            sc--;
        }
        ec = CurrentPos.column - 1;
        while( ec < len - 1 && !isWord( data[ec + 1] ) &&
               !isspace( (unsigned char)data[ec + 1] ) && data[ec + 1] != 0 ) {
            ec++;
        }
    }

    if( !inner ) {
        /* "around" — extend to include adjacent whitespace.
         * Prefer trailing whitespace; if none, take leading. */
        if( ec < len - 1 && isspace( (unsigned char)data[ec + 1] ) ) {
            while( ec < len - 1 && isspace( (unsigned char)data[ec + 1] ) ) {
                ec++;
            }
        } else if( sc > 0 && isspace( (unsigned char)data[sc - 1] ) ) {
            while( sc > 0 && isspace( (unsigned char)data[sc - 1] ) ) {
                sc--;
            }
        }
    }

    /* Convert back to 1-based for the range */
    r->start.line = CurrentPos.line;
    r->start.column = sc + 1;
    r->end.line = CurrentPos.line;
    r->end.column = ec + 1;
    r->line_based = FALSE;
    r->fix_range = FALSE;
    return( ERR_NO_ERR );
}

/*
 * TextObj_Quote - select inner/around quoted string.
 *
 * Handles ", ', and ` quotes. Searches the current line for
 * a matching pair of quote characters that contains the cursor.
 * If the cursor is not inside quotes, looks for the next quoted
 * string on the line.
 */
vi_rc TextObj_Quote( range *r, bool inner, char qchar )
{
    int         col, len, qstart, qend;
    char        *data;
    line        *cline;
    fcb         *cfcb;
    vi_rc       rc;

    if( CurrentLine == NULL ) {
        return( ERR_NO_FILE );
    }
    rc = CGimmeLinePtr( CurrentPos.line, &cfcb, &cline );
    if( rc != ERR_NO_ERR ) {
        return( rc );
    }

    data = cline->data;
    len = cline->len;
    col = CurrentPos.column - 1;

    /*
     * Strategy: find all quote pairs on the line. A pair is an
     * opening quote followed by a closing quote (not escaped).
     * Find the pair that contains the cursor, or if none does,
     * find the first pair after the cursor.
     */
    qstart = -1;
    qend = -1;

    {
        int     i = 0;
        while( i < len ) {
            /* Skip escaped characters */
            if( data[i] == '\\' && i + 1 < len ) {
                i += 2;
                continue;
            }
            if( data[i] == qchar ) {
                /* Found opening quote — find its close */
                int open = i;
                i++;
                while( i < len ) {
                    if( data[i] == '\\' && i + 1 < len ) {
                        i += 2;
                        continue;
                    }
                    if( data[i] == qchar ) {
                        /* Found a pair: open..i */
                        if( col >= open && col <= i ) {
                            /* Cursor is inside this pair */
                            qstart = open;
                            qend = i;
                            goto found;
                        }
                        if( open > col && qstart == -1 ) {
                            /* First pair after cursor */
                            qstart = open;
                            qend = i;
                            goto found;
                        }
                        break;
                    }
                    i++;
                }
            }
            i++;
        }
    }

found:
    if( qstart == -1 || qend == -1 ) {
        return( ERR_FIND_NOT_FOUND );
    }

    if( inner ) {
        /* Between the quotes, not including them */
        r->start.column = qstart + 2;  /* 1-based, past the opening quote */
        r->end.column = qend;          /* 1-based, before the closing quote */
    } else {
        /* Including the quotes */
        r->start.column = qstart + 1;  /* 1-based */
        r->end.column = qend + 1;      /* 1-based */
    }
    r->start.line = CurrentPos.line;
    r->end.line = CurrentPos.line;
    r->line_based = FALSE;
    r->fix_range = FALSE;

    /* Handle empty quotes for inner: ci"" on empty string */
    if( inner && r->start.column > r->end.column ) {
        /* Empty inside — set zero-width range at the opening quote.
         * The operator will delete nothing and just enter insert mode
         * at the right position (for 'c'), or do nothing (for 'd'). */
        r->end.column = r->start.column - 1;
    }

    return( ERR_NO_ERR );
}

/*
 * findMatchingBracket - scan for the matching bracket, handling nesting.
 *
 * Searches forward from 'startcol' on 'startline' for 'close' while
 * tracking nested 'open' characters. Returns the position in *eline/*ecol.
 * Also searches backward if the cursor is on the close bracket.
 *
 * Columns are 0-based indices into line data.
 */
static vi_rc findEnclosingBrackets( char open, char close,
                                     linenum *sline, int *scol,
                                     linenum *eline, int *ecol )
{
    int         depth, col, len;
    linenum     ln;
    line        *cline;
    fcb         *cfcb;
    vi_rc       rc;
    char        *data;
    linenum     lastline;

    CFindLastLine( &lastline );

    /*
     * Search backward from cursor for the opening bracket
     */
    depth = 1;
    ln = CurrentPos.line;
    rc = CGimmeLinePtr( ln, &cfcb, &cline );
    if( rc != ERR_NO_ERR ) return( rc );
    col = CurrentPos.column - 2;  /* 0-based, start one before cursor */
    data = cline->data;
    len = cline->len;

    /* If cursor is ON the open bracket, use it directly */
    if( col + 1 >= 0 && col + 1 < len && data[col + 1] == open ) {
        *sline = ln;
        *scol = col + 1;
        depth = 0;
    }
    if( depth > 0 ) {
        while( TRUE ) {
            while( col >= 0 ) {
                if( data[col] == close ) {
                    depth++;
                } else if( data[col] == open ) {
                    depth--;
                    if( depth == 0 ) {
                        *sline = ln;
                        *scol = col;
                        goto found_open;
                    }
                }
                col--;
            }
            ln--;
            if( ln < 1 ) {
                return( ERR_FIND_NOT_FOUND );
            }
            rc = CGimmeLinePtr( ln, &cfcb, &cline );
            if( rc != ERR_NO_ERR ) return( rc );
            data = cline->data;
            len = cline->len;
            col = len - 1;
        }
    }

found_open:
    /*
     * Search forward from the opening bracket for the closing bracket
     */
    depth = 1;
    ln = *sline;
    rc = CGimmeLinePtr( ln, &cfcb, &cline );
    if( rc != ERR_NO_ERR ) return( rc );
    data = cline->data;
    len = cline->len;
    col = *scol + 1;

    while( TRUE ) {
        while( col < len ) {
            if( data[col] == open ) {
                depth++;
            } else if( data[col] == close ) {
                depth--;
                if( depth == 0 ) {
                    *eline = ln;
                    *ecol = col;
                    return( ERR_NO_ERR );
                }
            }
            col++;
        }
        ln++;
        if( ln > lastline ) {
            return( ERR_FIND_NOT_FOUND );
        }
        rc = CGimmeLinePtr( ln, &cfcb, &cline );
        if( rc != ERR_NO_ERR ) return( rc );
        data = cline->data;
        len = cline->len;
        col = 0;
    }
}

/*
 * TextObj_Bracket - select inner/around bracket pair.
 *
 * Handles (), {}, [], <>. Finds the enclosing bracket pair
 * around the cursor (handles nesting and multi-line).
 *
 * "inner" selects between the brackets (exclusive).
 * "around" includes the brackets themselves.
 */
vi_rc TextObj_Bracket( range *r, bool inner, char open, char close )
{
    linenum     sline, eline;
    int         scol, ecol;
    vi_rc       rc;

    if( CurrentLine == NULL ) {
        return( ERR_NO_FILE );
    }

    rc = findEnclosingBrackets( open, close, &sline, &scol, &eline, &ecol );
    if( rc != ERR_NO_ERR ) {
        return( rc );
    }

    if( inner ) {
        /* Between the brackets, not including them */
        if( sline == eline ) {
            /* Same line: just inside the brackets */
            r->start.line = sline;
            r->start.column = scol + 2;    /* 1-based, past opening bracket */
            r->end.line = eline;
            r->end.column = ecol;          /* 1-based, before closing bracket */
            r->line_based = FALSE;
        } else {
            /* Multi-line: select from line after opening bracket
             * to line before closing bracket (line-based). If the
             * opening bracket is the last char on its line and the
             * closing bracket is the first non-blank on its line,
             * use line-based selection of the lines between. */
            r->start.line = sline;
            r->start.column = scol + 2;    /* past opening bracket */
            r->end.line = eline;
            r->end.column = ecol;          /* before closing bracket */
            r->line_based = FALSE;
        }
    } else {
        /* Including the brackets */
        r->start.line = sline;
        r->start.column = scol + 1;       /* 1-based */
        r->end.line = eline;
        r->end.column = ecol + 1;         /* 1-based */
        r->line_based = FALSE;
    }

    r->fix_range = FALSE;

    /* Handle empty brackets for inner */
    if( inner && r->start.line == r->end.line &&
        r->start.column > r->end.column ) {
        r->end.column = r->start.column - 1;
    }

    return( ERR_NO_ERR );
}

/*
 * TextObj_Paragraph - select inner/around paragraph.
 *
 * A paragraph is a block of non-blank lines. Blank lines
 * are the boundaries.
 *
 * "inner" selects the non-blank lines.
 * "around" includes trailing blank lines.
 */
vi_rc TextObj_Paragraph( range *r, bool inner )
{
    linenum     sline, eline, lastline;
    line        *cline;
    fcb         *cfcb;
    vi_rc       rc;

    if( CurrentLine == NULL ) {
        return( ERR_NO_FILE );
    }
    CFindLastLine( &lastline );

    /* Find the start of the paragraph (first non-blank line
     * going backward from cursor, or first blank line block) */
    sline = CurrentPos.line;

    /* Check if we're on a blank line — if so, select the blank block */
    rc = CGimmeLinePtr( sline, &cfcb, &cline );
    if( rc != ERR_NO_ERR ) return( rc );

    if( cline->len == 0 ) {
        /* On a blank line: select the contiguous blank lines */
        while( sline > 1 ) {
            rc = CGimmeLinePtr( sline - 1, &cfcb, &cline );
            if( rc != ERR_NO_ERR ) break;
            if( cline->len != 0 ) break;
            sline--;
        }
        eline = CurrentPos.line;
        while( eline < lastline ) {
            rc = CGimmeLinePtr( eline + 1, &cfcb, &cline );
            if( rc != ERR_NO_ERR ) break;
            if( cline->len != 0 ) break;
            eline++;
        }
    } else {
        /* On a non-blank line: find paragraph boundaries */
        while( sline > 1 ) {
            rc = CGimmeLinePtr( sline - 1, &cfcb, &cline );
            if( rc != ERR_NO_ERR ) break;
            if( cline->len == 0 ) break;
            sline--;
        }
        eline = CurrentPos.line;
        while( eline < lastline ) {
            rc = CGimmeLinePtr( eline + 1, &cfcb, &cline );
            if( rc != ERR_NO_ERR ) break;
            if( cline->len == 0 ) break;
            eline++;
        }

        if( !inner ) {
            /* Include trailing blank lines */
            while( eline < lastline ) {
                rc = CGimmeLinePtr( eline + 1, &cfcb, &cline );
                if( rc != ERR_NO_ERR ) break;
                if( cline->len != 0 ) break;
                eline++;
            }
        }
    }

    r->start.line = sline;
    r->start.column = 1;
    r->end.line = eline;
    r->end.column = LineLength( eline );
    if( r->end.column == 0 ) r->end.column = 1;
    r->line_based = TRUE;
    r->fix_range = FALSE;
    return( ERR_NO_ERR );
}

/*
 * dispatchTextObject - called from doOperator when the user types
 * 'i' or 'a' after an operator. Reads the next keystroke to determine
 * which text object, then dispatches to the appropriate function.
 *
 * Returns ERR_NO_ERR with range populated, or an error code.
 */
vi_rc dispatchTextObject( range *r, bool inner, vi_key objchar )
{
    switch( objchar ) {
    case 'w':
        return( TextObj_Word( r, inner, FALSE ) );
    case 'W':
        return( TextObj_Word( r, inner, TRUE ) );
    case '"':
        return( TextObj_Quote( r, inner, '"' ) );
    case '\'':
        return( TextObj_Quote( r, inner, '\'' ) );
    case '`':
        return( TextObj_Quote( r, inner, '`' ) );
    case '(':
    case ')':
    case 'b':
        return( TextObj_Bracket( r, inner, '(', ')' ) );
    case '{':
    case '}':
    case 'B':
        return( TextObj_Bracket( r, inner, '{', '}' ) );
    case '[':
    case ']':
        return( TextObj_Bracket( r, inner, '[', ']' ) );
    case '<':
    case '>':
        return( TextObj_Bracket( r, inner, '<', '>' ) );
    case 'p':
        return( TextObj_Paragraph( r, inner ) );
    default:
        return( ERR_INVALID_OP );
    }
}
