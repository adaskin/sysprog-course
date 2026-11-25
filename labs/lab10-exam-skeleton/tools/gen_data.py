#!/usr/bin/env python3
"""Generate the mock data: data/students.csv and data/questions.csv.
Same --seed gives the same files, so every student has identical data.

students.csv : id,name,pin,exam_id
questions.csv: exam_id,q_no,text,A,B,C,D,answer
"""
import argparse, os, random

TOPICS = [  # (question, correct, [wrong1, wrong2, wrong3])
    ("Which call creates a new process?", "fork()", ["exec()", "wait()", "pipe()"]),
    ("Which call waits for a child to finish?", "waitpid()", ["fork()", "dup2()", "kill()"]),
    ("Which call creates a thread?", "pthread_create()", ["fork()", "clone2()", "spawn()"]),
    ("What does recv() return when the peer closed the connection?", "0", ["-1", "1", "EOF"]),
    ("Which protocol is a reliable byte stream?", "TCP", ["UDP", "IP", "ICMP"]),
    ("Which call marks a socket as passive (accepting)?", "listen()", ["bind()", "connect()", "socket()"]),
    ("Which function converts a port to network byte order?", "htons()", ["ntohs()", "htonl()", "atoi()"]),
    ("Which call waits on many fds and rebuilds its set each time?", "select()", ["epoll_wait()", "accept()", "read()"]),
    ("Which Linux call scales to thousands of fds without scanning them all?", "epoll_wait()", ["select()", "poll()", "sleep()"]),
    ("Which signal is sent when writing to a closed socket?", "SIGPIPE", ["SIGCHLD", "SIGINT", "SIGSEGV"]),
    ("What does a mutex protect?", "shared data", ["the stack", "the code segment", "file names"]),
    ("Which flag makes a socket call return EAGAIN instead of blocking?", "O_NONBLOCK", ["O_APPEND", "O_EXCL", "SO_REUSEADDR"]),
]

def make_question(rng):
    if rng.random() < 0.5:
        t, good, bad = rng.choice(TOPICS)
    else:
        a, b = rng.randint(3, 19), rng.randint(3, 19)
        t, good = f"What is {a} x {b}?", str(a * b)
        bad = list({str(a * b + d) for d in (-10, -3, -1, 1, 2, 5, 10)})
        rng.shuffle(bad); bad = bad[:3]
    opts = [good] + bad
    rng.shuffle(opts)
    return t, opts, "ABCD"[opts.index(good)]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=322)
    ap.add_argument("--students", type=int, default=1000)
    ap.add_argument("--exams", type=int, default=3)
    ap.add_argument("--questions", type=int, default=20)
    ap.add_argument("--out", default="data")
    a = ap.parse_args()
    rng = random.Random(a.seed)
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "students.csv"), "w") as f:
        f.write("id,name,pin,exam_id\n")
        for i in range(a.students):
            f.write(f"{1001 + i},Agent{i:04d},{rng.randint(1000, 9999)},{i % a.exams}\n")
    with open(os.path.join(a.out, "questions.csv"), "w") as f:
        f.write("exam_id,q_no,text,A,B,C,D,answer\n")
        for e in range(a.exams):
            for q in range(1, a.questions + 1):
                t, o, ans = make_question(rng)
                f.write(f"{e},{q},{t},{','.join(o)},{ans}\n")
    print(f"wrote {a.students} students, {a.exams} exams x {a.questions} questions to {a.out}/")

if __name__ == "__main__":
    main()
