/* proto.h -- GIVEN. Constants shared by the exam server and the student client.
 *
 * The protocol (text, one request per line, "\n" or "\r\n"):
 *   LOGIN <id> <pin>   -> OK <name> <exam_id> <num_questions> | ERR ...
 *   QUESTIONS          -> Q ... lines, then END
 *   SUBMIT <answers>   -> OK | ERR BADFORMAT | ERR DUPLICATE
 *   RESULT             -> SCORE ..., WRONG ... lines, then END
 *   QUIT               -> BYE (the server then closes)
 * A reply is complete at END or at a line starting with OK, ERR, or BYE. */
#ifndef PROTO_H
#define PROTO_H

#define MAX_LINE     512    /* longest line, including the '\n' */
#define OUT_MAX      16384  /* largest reply the server produces for one request */
#define MAX_Q        32     /* most questions in one exam */
#define FIRST_ID     1001   /* first valid student id */
#define MAX_STUDENTS 1000   /* valid ids: FIRST_ID .. FIRST_ID+MAX_STUDENTS-1 */

#endif
