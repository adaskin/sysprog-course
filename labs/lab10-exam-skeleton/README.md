# Lab 10 starter: Exam Orchestrator (one server, many students)

Handout: [`../10lab-exam-server.md`](../10lab-exam-server.md)

```text
lab10-exam-skeleton/
  common/        linebuf.c/h  framing + send_all, used by BOTH programs   (TODO 1-2)
  exam-server/   the SERVER program (one process)                          (TODO 3-9)
  student/       the CLIENT program (one student; you start many)          (TODO S1-S3)
  tools/         data generator, run_students.sh, check_server.py, test_server.sh
```

```bash
make data                          # mock data: 1000 students, 3 exams x 20 questions (same for everyone)
make                               # builds bin/examsrv and bin/student (the TODO stubs compile)
bin/examsrv iterative 5200 &       # the server
sh tools/run_students.sh 20 5200   # 20 student programs at the same time
make test MODE=thread              # server checks + 300 students, one model
make test-all                      # every model (unfinished ones are reported as FAIL)
make reset                         # delete results/ (a student can submit only once)
```

Do not commit `bin/`, `data/` or `results/`.
