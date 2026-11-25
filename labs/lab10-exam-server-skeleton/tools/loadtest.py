#!/usr/bin/env python3
"""Conformance checks + load test for the exam orchestrator (GIVEN; read it as a
small example of an asyncio network client -- 'agents' talking to your server).

  python3 tools/loadtest.py --port 5200 --clients 300 --label epoll [--pid SERVERPID]

1. CHECKS : protocol conformance, partial lines, pipelining, long lines, a silent
            connection that must not block others, abrupt disconnects.
2. LOAD   : N concurrent 'student agents', each does LOGIN, QUESTIONS, SUBMIT,
            RESULT, QUIT and verifies its score against the answer key.
3. HEALTH : with --pid, the server's open fds are counted before/after (leaks)
            and zombie children are looked for.
Exit status 0 = PASS, 1 = FAIL.
"""
import argparse, asyncio, csv, os, random, sys, time

HOST = "127.0.0.1"
FIRST_ID = 1001


def load_data(d="data"):
    key, nq = {}, {}
    for r in csv.DictReader(open(os.path.join(d, "questions.csv"))):
        e, q = int(r["exam_id"]), int(r["q_no"])
        key[(e, q)] = r["answer"]
        nq[e] = max(nq.get(e, 0), q)
    students = {int(r["id"]): r for r in csv.DictReader(open(os.path.join(d, "students.csv")))}
    return key, nq, students


class Agent:
    """One connection to the server."""
    async def open(self, port):
        self.r, self.w = await asyncio.wait_for(asyncio.open_connection(HOST, port), 10)
        return self

    async def read_reply(self, timeout=10):
        """One reply: lines up to and including END, or a single OK/ERR/BYE line."""
        lines = []
        while True:
            raw = await asyncio.wait_for(self.r.readline(), timeout)
            if not raw:
                raise EOFError("server closed the connection")
            line = raw.decode().rstrip("\r\n")
            lines.append(line)
            if line == "END" or line.startswith(("OK", "ERR", "BYE")):
                return lines

    def send(self, text):
        self.w.write(text.encode())

    async def ask(self, text, timeout=10):
        self.send(text)
        await self.w.drain()
        return await self.read_reply(timeout)

    async def close(self):
        self.w.close()
        try:
            await self.w.wait_closed()
        except Exception:
            pass


def expect(cond, msg):
    if not cond:
        raise AssertionError(msg)


async def student_session(port, sid, data, think=0.05, chunked=False, wrong=5):
    key, nq, students = data
    pin, ex = students[sid]["pin"], int(students[sid]["exam_id"])
    a = await Agent().open(port)
    try:
        if chunked:                                   # one byte at a time
            for ch in f"LOGIN {sid} {pin}\n":
                a.send(ch); await a.w.drain(); await asyncio.sleep(0.001)
            r = await a.read_reply()
        else:
            r = await a.ask(f"LOGIN {sid} {pin}\r\n")
        expect(r[0].startswith("OK"), f"login: {r}")
        r = await a.ask("QUESTIONS\n")
        expect(r[-1] == "END" and len(r) == nq[ex] + 1, f"questions: {len(r)} lines")
        await asyncio.sleep(think)
        ans = [key[(ex, q)] for q in range(1, nq[ex] + 1)]
        for q in range(wrong):                        # make `wrong` answers wrong
            ans[q] = "A" if ans[q] != "A" else "B"
        r = await a.ask("SUBMIT " + "".join(ans) + "\n")
        expect(r == ["OK"], f"submit: {r}")
        r = await a.ask("RESULT\n")
        expect(r[0] == f"SCORE {nq[ex] - wrong}/{nq[ex]}", f"result: {r[0]}")
        expect(sum(x.startswith("WRONG") for x in r) == wrong, "wrong-lines")
        r = await a.ask("QUIT\n")
        expect(r == ["BYE"], f"quit: {r}")
    finally:
        await a.close()


async def run_checks(port, data):
    key, nq, students = data
    results = []

    async def check(name, coro):
        try:
            await coro
            results.append((name, True, ""))
        except Exception as e:
            results.append((name, False, repr(e)[:100]))

    sid = FIRST_ID + 900                              # ids reserved for the checks
    pin, ex = students[sid]["pin"], int(students[sid]["exam_id"])

    async def c_basic():
        a = await Agent().open(port)
        expect((await a.ask("QUESTIONS\n")) == ["ERR NOLOGIN"], "NOLOGIN")
        expect((await a.ask(f"LOGIN {sid} 0000\n")) == ["ERR BADLOGIN"], "BADLOGIN")
        expect((await a.ask("LOGIN abc\n")) == ["ERR BADFORMAT"], "BADFORMAT")
        r = await a.ask(f"LOGIN {sid} {pin}\n")
        expect(r[0].startswith("OK"), r)
        expect((await a.ask("HELLO\n")) == ["ERR UNKNOWN"], "UNKNOWN")
        expect((await a.ask("RESULT\n")) == ["ERR NOSUBMISSION"], "NOSUBMISSION")
        expect((await a.ask("SUBMIT ABC\n")) == ["ERR BADFORMAT"], "short answers")
        await a.close()

    async def c_duplicate():
        s2 = FIRST_ID + 901
        p2, e2 = students[s2]["pin"], int(students[s2]["exam_id"])
        a = await Agent().open(port)
        await a.ask(f"LOGIN {s2} {p2}\n")
        ans = "A" * nq[e2]
        expect((await a.ask(f"SUBMIT {ans}\n")) == ["OK"], "first submit")
        expect((await a.ask(f"SUBMIT {ans}\n")) == ["ERR DUPLICATE"], "second submit")
        await a.close()
        b = await Agent().open(port)                  # same student, new connection
        await b.ask(f"LOGIN {s2} {p2}\n")
        expect((await b.ask(f"SUBMIT {ans}\n")) == ["ERR DUPLICATE"], "dup on new connection")
        await b.close()

    async def c_pipelined():                          # two requests in ONE send
        a = await Agent().open(port)
        a.send(f"LOGIN {sid} {pin}\nQUESTIONS\nQUIT\n")
        r1 = await a.read_reply(); r2 = await a.read_reply(); r3 = await a.read_reply()
        expect(r1[0].startswith("OK") and r2[-1] == "END" and r3 == ["BYE"], f"{r1[:1]} {r2[-1:]} {r3}")
        await a.close()

    async def c_chunked():                            # one request in many sends
        await student_session(port, FIRST_ID + 902, data, think=0, chunked=True)

    async def c_toolong():
        a = await Agent().open(port)
        a.send("X" * 3000)
        await a.w.drain()
        r = await a.read_reply()
        expect(r == ["ERR TOOLONG"], f"{r!r}")
        await a.close()

    async def c_disconnect():                         # vanish in the middle of a request
        for _ in range(5):
            a = await Agent().open(port)
            a.send("LOGIN 10")
            await a.w.drain()
            a.w.transport.abort()
        a = await Agent().open(port)                  # server must still answer
        expect((await a.ask("QUIT\n")) == ["BYE"], "server alive after aborts")
        await a.close()

    async def c_silent():                             # an idle client must not block others
        idle = await Agent().open(port)
        idle.send("LOGI")                             # half a request, then silence
        await idle.w.drain()
        await asyncio.wait_for(student_session(port, FIRST_ID + 903, data, think=0), 5)
        await idle.close()

    for name, fn in [("basic errors", c_basic), ("duplicate submit", c_duplicate),
                     ("pipelined requests", c_pipelined), ("one byte at a time", c_chunked),
                     ("too-long line", c_toolong), ("abrupt disconnects", c_disconnect),
                     ("idle client does not block", c_silent)]:
        await check(name, fn())
    return results


async def run_load(port, data, clients, concurrency, think):
    sem = asyncio.Semaphore(concurrency)
    fails = []

    async def one(i):
        async with sem:
            try:
                await asyncio.wait_for(student_session(
                    port, FIRST_ID + i, data, think=think * random.random(),
                    chunked=(i % 7 == 0), wrong=i % 6), 60)
            except Exception as e:
                fails.append((i, repr(e)[:80]))

    t0 = time.time()
    await asyncio.gather(*(one(i) for i in range(clients)))
    return time.time() - t0, fails


def count_fds(pid):
    try:
        return len(os.listdir(f"/proc/{pid}/fd"))
    except OSError:
        return -1


def zombies(pid):
    try:
        out = os.popen(f"ps -o stat= --ppid {pid}").read().split()
        return sum(1 for s in out if s.startswith("Z"))
    except Exception:
        return 0


async def main_async(a):
    data = load_data()
    if a.clients > 890:
        print("note: ids 1901..1904 are reserved for the checks; use --clients <= 890 or edit the reserved ids")
    fd0 = count_fds(a.pid) if a.pid else -1
    print(f"== {a.label}: checks")
    res = await run_checks(a.port, data)
    for name, ok, why in res:
        print(f"   {'ok  ' if ok else 'FAIL'} {name} {why}")
    bad = sum(1 for _, ok, _ in res if not ok)
    print(f"== {a.label}: load ({a.clients} agents, {a.concurrency} at a time)")
    secs, fails = await run_load(a.port, data, a.clients, a.concurrency, a.think)
    print(f"   {a.clients - len(fails)}/{a.clients} sessions ok in {secs:.2f}s")
    for i, why in fails[:3]:
        print(f"   FAIL agent {i}: {why}")
    leak = zomb = 0
    if a.pid:
        await asyncio.sleep(0.5)
        fd1 = count_fds(a.pid)
        leak = max(0, fd1 - fd0)
        zomb = zombies(a.pid)
        print(f"   server fds: {fd0} -> {fd1}   zombie children: {zomb}")
    ok = bad == 0 and not fails and leak == 0 and zomb == 0
    print(f"== {a.label}: {'PASS' if ok else 'FAIL'}   (CSV: {a.label},{a.clients},{secs:.2f})")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=5200)
    ap.add_argument("--clients", type=int, default=300)
    ap.add_argument("--concurrency", type=int, default=300, help="agents connected at once")
    ap.add_argument("--think", type=float, default=0.1, help="max think time (s) between QUESTIONS and SUBMIT")
    ap.add_argument("--pid", type=int, default=0, help="server pid for fd/zombie checks")
    ap.add_argument("--label", default="server")
    a = ap.parse_args()
    try:
        sys.exit(asyncio.run(main_async(a)))
    except (ConnectionError, OSError, asyncio.TimeoutError) as e:
        print(f"== {a.label}: FAIL (cannot talk to the server: {e!r})")
        sys.exit(1)


if __name__ == "__main__":
    main()
