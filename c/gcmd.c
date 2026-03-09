/****************************************************************************
*
* Description:  g-prefix command implementations (gg, gd, gu, gU, gJ, gv, gf)
*
****************************************************************************/


#include "vi.h"
#include "win.h"
#include <assert.h>
#include <ctype.h>

/*
 * caseConvertOneLine: convert case for each character on the given line
 * from start_col (base 0) to end_col (base 0).
 * mode: 0 = lowercase, 1 = uppercase
 */
static vi_rc caseConvertOneLine( linenum line_num, int start_col, int end_col, int mode )
{
    line        *line;
    fcb         *fcb;
    int         num_cols, i;
    char        *s;
    vi_rc       rc;

    rc = CGimmeLinePtr( line_num, &fcb, &line );
    if( rc == ERR_NO_ERR ) {
        assert( end_col < line->len && end_col >= start_col );
        num_cols = end_col - start_col + 1;
        s = &line->data[start_col];
        for( i = 0; i < num_cols; i++ ) {
            if( mode == 0 ) {
                *s = tolower( *s );
            } else {
                *s = toupper( *s );
            }
            s++;
        }
    }
    return( rc );
}

/*
 * caseConvertToEndOfLine: convert case from start_col to end of line
 */
static vi_rc caseConvertToEndOfLine( linenum line, int start, int mode )
{
    int         len;
    vi_rc       rc;

    rc = ERR_NO_ERR;
    len = LineLength( line );
    if( len ) {
        rc = caseConvertOneLine( line, start, len - 1, mode );
    }
    return( rc );
}

/*
 * DoCaseConvert - convert case of text over a range.
 * mode: 0 = lowercase (gu), 1 = uppercase (gU)
 *
 * Modeled directly on ChangeCase() in op.c.
 */
static vi_rc DoCaseConvert( range *r, int mode )
{
    linenum     curr;
    vi_rc       rc;
    long        total;
    char        *msg;

    UndoReplaceLines( r->start.line, r->end.line );
    if( r->start.line == r->end.line ) {
        rc = caseConvertOneLine( r->start.line, r->start.column, r->end.column, mode );
        msg = MSG_CHARACTERS;
        total = r->end.column - r->start.column + 1;
    } else {
        rc = caseConvertToEndOfLine( r->start.line, r->start.column, mode );
        if( rc != ERR_NO_ERR ) {
            return( rc );
        }
        for( curr = r->start.line + 1; curr < r->end.line; curr++ ) {
            rc = caseConvertToEndOfLine( curr, 0, mode );
            if( rc != ERR_NO_ERR ) {
                return( rc );
            }
        }
        rc = caseConvertOneLine( r->end.line, 0, r->end.column, mode );
        msg = MSG_LINES;
        total = r->end.line - r->start.line + 1;
    }
    EditFlags.Dotable = TRUE;
    DCDisplayAllLines();
    if( mode == 0 ) {
        Message1( "lowercased %l %s", total, msg );
    } else {
        Message1( "uppercased %l %s", total, msg );
    }
    Modified( TRUE );
    return( DO_NOT_CLEAR_MESSAGE_WINDOW );
}

/*
 * DoLowercase - gu operator: lowercase text over a range
 */
vi_rc DoLowercase( range *r )
{
    return( DoCaseConvert( r, 0 ) );
}

/*
 * DoUppercase - gU operator: uppercase text over a range
 */
vi_rc DoUppercase( range *r )
{
    return( DoCaseConvert( r, 1 ) );
}

/*
 * doGOperator - process a g-prefix operator (gu, gU) with a motion.
 *
 * This reads the next event (the motion), handles repeat counts and
 * text objects, normalizes the range, checks for modification permission,
 * and then calls the operator function.  Modeled on doOperator() in
 * editmain.c.
 */
static vi_rc doGOperator( op_rtn opfn, vi_key opchar )
{
    event       *next;
    vi_rc       rc;
    long        count;
    range       range;
    int         next_type;

    rc = ERR_INVALID_OP;

    /* Initialize range to current cursor position */
    range.start = CurrentPos;
    range.end = CurrentPos;
    range.line_based = FALSE;
    range.highlight = FALSE;
    range.fix_range = FALSE;

    count = GetRepeatCount();

    /* Read the motion key */
    LastEvent = GetNextEvent( TRUE );
    next = &EventList[LastEvent];

    /* Consume any repeat count digits between operator and motion */
    if( next != &EventList['0'] ) {
        while( next->b.is_number ) {
            next->rtn.old();
            LastEvent = GetNextEvent( TRUE );
            next = &EventList[LastEvent];
        }
    }
    count *= GetRepeatCount();
    KillRepeatWindow();

    range.fix_range = next->b.fix_range;
    next_type = next->b.type;

    EditFlags.OperatorWantsMove = TRUE;

    if( next_type == EVENT_OP ) {
        /*
         * Doubled operator (e.g. guu, gUU) — apply to current line.
         * Accept the operator's own character as line-mode shortcut.
         */
        if( LastEvent == opchar ) {
            rc = GetLineRange( &range, count, CurrentPos.line );
        }
    } else if( next_type == EVENT_REL_MOVE || next_type == EVENT_ABS_MOVE ) {
        rc = next->rtn.move( &range, count );
    } else {
        /* Handle text objects and special cases */
        if( LastEvent == 'i' || LastEvent == 'a' ) {
            bool    txtobj_inner = ( LastEvent == 'i' );
            vi_key  objchar;
            LastEvent = GetNextEvent( TRUE );
            objchar = LastEvent;
            rc = dispatchTextObject( &range, txtobj_inner, objchar );
        } else if( LastEvent == '0' ) {
            rc = MoveLineBegin( &range, 1 );
        } else if( LastEvent == VI_KEY( ESC ) ) {
            rc = RANGE_REQUEST_CANCELLED;
        }
    }

    EditFlags.OperatorWantsMove = FALSE;

    if( rc == ERR_NO_ERR ) {
        rc = ModificationTest();
        if( rc == ERR_NO_ERR ) {
            NormalizeRange( &range );
            rc = opfn( &range );
        }
    }
    return( rc );
}

/*
 * DoGPrefix - main dispatcher for g-prefix commands.
 *
 * Called when 'g' is pressed in command mode.  Reads the next keystroke
 * to determine which g-command to execute:
 *
 *   gg  - go to first line (or line N if repeat count given)
 *   gd  - go to local declaration of word under cursor
 *   gu  - lowercase operator (takes a motion)
 *   gU  - uppercase operator (takes a motion)
 *   gJ  - join lines without inserting a space
 *   gv  - reselect last selection region
 *   gf  - open filename under cursor
 */
vi_rc DoGPrefix( void )
{
    vi_key      key;
    vi_rc       rc;

    key = GetNextEvent( FALSE );

    switch( key ) {

    case 'g':
        /*
         * gg - go to first line of file, or to line N if a repeat
         * count was given.  Modeled on DoGo() in opmove.c but
         * defaults to line 1 instead of last line.
         */
        {
            linenum     lne;
            long        count;

            if( CurrentLine == NULL ) {
                return( ERR_NO_FILE );
            }
            count = GetRepeatCount();
            KillRepeatWindow();
            CFindLastLine( &lne );
            if( NoRepeatInfo ) {
                /* No count given: go to line 1 */
                lne = 1;
            } else {
                /* Count given: go to that line */
                if( count > lne ) {
                    return( ERR_NO_SUCH_LINE );
                }
                lne = count;
            }
            MemorizeCurrentContext();
            rc = GoToLineNoRelCurs( lne );
            if( rc == ERR_NO_ERR ) {
                GoToColumnOnCurrentLine( FindStartOfCurrentLine() );
            }
            return( rc );
        }

    case 'd':
        /*
         * gd - go to local declaration of word under cursor.
         * Searches backward from the current position for the opening
         * brace '{' at column 1 (the start of the current function),
         * then searches forward from there for the first occurrence
         * of the word under the cursor.
         */
        {
            char        word[MAX_STR];
            linenum     startLine;
            linenum     ln;
            line        *cline;
            fcb         *cfcb;
            char        *p;

            if( CurrentLine == NULL ) {
                return( ERR_NO_FILE );
            }
            rc = GimmeCurrentWord( word, sizeof( word ), FALSE );
            if( rc != ERR_NO_ERR ) {
                return( rc );
            }
            if( word[0] == 0 ) {
                return( ERR_NO_WORD_TO_FIND );
            }

            /*
             * Search backward for '{' at column 0 to find the
             * beginning of the current function/block.
             */
            startLine = 1;
            for( ln = CurrentPos.line - 1; ln >= 1; ln-- ) {
                rc = CGimmeLinePtr( ln, &cfcb, &cline );
                if( rc != ERR_NO_ERR ) {
                    break;
                }
                if( cline->len > 0 && cline->data[0] == '{' ) {
                    startLine = ln;
                    break;
                }
            }

            /*
             * Search forward from startLine for the first occurrence
             * of the word.  We look for the word as a standalone
             * identifier (not a substring of a larger word).
             */
            for( ln = startLine; ln <= CurrentPos.line; ln++ ) {
                rc = CGimmeLinePtr( ln, &cfcb, &cline );
                if( rc != ERR_NO_ERR ) {
                    break;
                }
                p = cline->data;
                while( (p = strstr( p, word )) != NULL ) {
                    int     wlen = strlen( word );
                    int     col_before = (int)( p - cline->data );

                    /*
                     * Check that this is a whole-word match:
                     * character before must not be alphanumeric or '_',
                     * and character after must not be alphanumeric or '_'.
                     */
                    if( ( col_before == 0 || !( isalnum( p[-1] ) || p[-1] == '_' ) ) &&
                        !( isalnum( p[wlen] ) || p[wlen] == '_' ) ) {
                        /* Found the declaration — go there */
                        MemorizeCurrentContext();
                        rc = GoToLineNoRelCurs( ln );
                        if( rc == ERR_NO_ERR ) {
                            GoToColumnOnCurrentLine( col_before + 1 );
                        }
                        return( rc );
                    }
                    p += wlen;
                }
            }
            Message1( "Declaration of '%s' not found", word );
            return( DO_NOT_CLEAR_MESSAGE_WINDOW );
        }

    case 'u':
        /*
         * gu{motion} - lowercase text over motion range.
         * Works as an operator: reads a motion, then lowercases
         * all characters in the resulting range.
         */
        return( doGOperator( DoLowercase, 'u' ) );

    case 'U':
        /*
         * gU{motion} - uppercase text over motion range.
         * Works as an operator: reads a motion, then uppercases
         * all characters in the resulting range.
         */
        return( doGOperator( DoUppercase, 'U' ) );

    case 'J':
        /*
         * gJ - join lines without inserting a space between them.
         * Uses the same GenericJoinCurrentLineToNext() as J but
         * passes FALSE to skip whitespace removal/space insertion.
         */
        {
            int     i, j;

            rc = ModificationTest();
            if( rc != ERR_NO_ERR ) {
                return( rc );
            }
            i = (int) GetRepeatCount();
            KillRepeatWindow();
            StartUndoGroup( UndoStack );
            for( j = 0; j < i; j++ ) {
                rc = GenericJoinCurrentLineToNext( FALSE );
                if( rc != ERR_NO_ERR ) {
                    break;
                }
            }
            EndUndoGroup( UndoStack );
            EditFlags.Dotable = TRUE;
            return( rc );
        }

    case 'v':
        /*
         * gv - reselect last selection region.
         * Restores the previous selection so the user can apply
         * another operator to the same area of text.
         */
        KillRepeatWindow();
        return( ReselectRegion() );

    case 'f':
        /*
         * gf - open the filename under the cursor.
         * Extracts the word (filename) at the cursor position and
         * opens it for editing.
         */
        {
            char    fname[MAX_STR];

            if( CurrentLine == NULL ) {
                return( ERR_NO_FILE );
            }
            KillRepeatWindow();
            rc = GimmeCurrentWord( fname, sizeof( fname ), FALSE );
            if( rc != ERR_NO_ERR ) {
                return( rc );
            }
            if( fname[0] == 0 ) {
                return( ERR_NO_WORD_TO_FIND );
            }
            return( EditFile( fname, FALSE ) );
        }

    case VI_KEY( ESC ):
        /* User cancelled the g-prefix — do nothing */
        return( ERR_NO_ERR );

    default:
        return( ERR_INVALID_KEY );
    }
}
