/* proto.h -- GIVEN. Constants shared by the exam server and the student client.
 *
 * The protocol (text, one request per line, "\n" or "\r\n"):
 *   LOGIN <id> <pin>   -> OK <name> <exam_id> <num_questions> | ERR BADLOGIN | ERR BADFORMAT
 *   QUESTIONS          -> Q <n>|text|A|B|C|D  (one line per question), then END
 *   SUBMIT <answers>   -> OK | ERR BADFORMAT | ERR DUPLICATE   (one letter A-D per question)
 *   RESULT             -> SCORE <s>/<n>, WRONG <q> <yours> <correct> ..., END | ERR NOSUBMISSION
 *   QUIT               -> BYE (the server then closes)
 * A reply is complete when you have read a line that is "END" or starts with
 * "OK", "ERR" or "BYE". Full description: labs/10lab-exam-server.md */
#ifndef PROTO_H
#define PROTO_H

#define MAX_LINE   512      /* longest line, including the '\n' */
#define OUT_MAX    16384    /* largest reply the server produces for one request */
#define MAX_Q      32       /* most questions in one exam */
#define FIRST_ID   1001     /* student ids are FIRST_ID .. FIRST_ID+999 */

#endif
