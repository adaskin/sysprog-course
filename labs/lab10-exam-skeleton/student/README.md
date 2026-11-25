# student: the client program

`student.c` is the **student agent**: one process, one connection, one exam. You will run many of them against one exam server.

TODOs S1–S3 are in `student.c`; framing and `send_all` are shared with the server in `../common/linebuf.c`.

```bash
bin/student 127.0.0.1 5200 1001 <pin>      # pins are in data/students.csv
sh tools/run_students.sh 20 5200           # 20 students at once
```
