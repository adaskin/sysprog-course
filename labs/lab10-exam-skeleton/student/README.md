# student client

`student.c` implements one student agent: one process, one TCP connection, one exam. Start with the manual single-client test before running many copies.

- TODO S1: `connect_to_server()` creates the client connection.
- TODO S2: `run_exam_session()` completes LOGIN through QUIT.
- `read_protocol_reply()` is given. It uses the line buffer correctly when one `recv()` contains a partial reply or several replies.

```bash
PIN=$(awk -F, '$1==1001 {print $3}' data/students.csv)
bin/student 127.0.0.1 5200 1001 "$PIN"
sh tools/run_students.sh 20 5200   # after the iterative server works
```
