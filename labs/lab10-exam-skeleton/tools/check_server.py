#!/usr/bin/env python3
"""GIVEN protocol checks; this test client is not the student deliverable."""
import argparse
import asyncio
import csv
import os
import sys

HOST = "127.0.0.1"
FIRST_ID = 1001


def load_data(directory="data"):
    key, nq = {}, {}
    with open(os.path.join(directory, "questions.csv")) as f:
        for row in csv.DictReader(f):
            exam, q = int(row["exam_id"]), int(row["q_no"])
            key[(exam, q)] = row["answer"]
            nq[exam] = max(nq.get(exam, 0), q)
    with open(os.path.join(directory, "students.csv")) as f:
        students = {int(row["id"]): row for row in csv.DictReader(f)}
    return key, nq, students


class Agent:
    async def open(self, port):
        self.r, self.w = await asyncio.wait_for(asyncio.open_connection(HOST, port), 10)
        return self

    async def read_reply(self, timeout=10):
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


def expect(condition, message):
    if not condition:
        raise AssertionError(message)


async def student_session(port, sid, data, think=0.05, chunked=False, wrong=5):
    key, nq, students = data
    pin, exam = students[sid]["pin"], int(students[sid]["exam_id"])
    agent = await Agent().open(port)
    try:
        if chunked:
            for char in f"LOGIN {sid} {pin}\n":
                agent.send(char)
                await agent.w.drain()
                await asyncio.sleep(0.001)
            reply = await agent.read_reply()
        else:
            reply = await agent.ask(f"LOGIN {sid} {pin}\r\n")
        expect(reply[0].startswith("OK"), f"login: {reply}")
        reply = await agent.ask("QUESTIONS\n")
        expect(reply[-1] == "END" and len(reply) == nq[exam] + 1,
               f"questions: {len(reply)} lines")
        await asyncio.sleep(think)
        answers = [key[(exam, q)] for q in range(1, nq[exam] + 1)]
        for q in range(wrong):
            answers[q] = "A" if answers[q] != "A" else "B"
        reply = await agent.ask("SUBMIT " + "".join(answers) + "\n")
        expect(reply == ["OK"], f"submit: {reply}")
        reply = await agent.ask("RESULT\n")
        expect(reply[0] == f"SCORE {nq[exam] - wrong}/{nq[exam]}",
               f"result: {reply[0]}")
        expect(sum(x.startswith("WRONG") for x in reply) == wrong, "wrong-lines")
        reply = await agent.ask("QUIT\n")
        expect(reply == ["BYE"], f"quit: {reply}")
    finally:
        await agent.close()


async def run_checks(port, data, skip_idle=False):
    _, nq, students = data
    results = []

    async def check(name, coroutine):
        try:
            await coroutine
            results.append((name, True, ""))
        except Exception as error:
            results.append((name, False, repr(error)[:100]))

    sid = FIRST_ID + 900
    pin, exam = students[sid]["pin"], int(students[sid]["exam_id"])

    async def basic():
        agent = await Agent().open(port)
        expect((await agent.ask("QUESTIONS\n")) == ["ERR NOLOGIN"], "NOLOGIN")
        expect((await agent.ask(f"LOGIN {sid} 0000\n")) == ["ERR BADLOGIN"], "BADLOGIN")
        expect((await agent.ask("LOGIN abc\n")) == ["ERR BADFORMAT"], "BADFORMAT")
        reply = await agent.ask(f"LOGIN {sid} {pin}\n")
        expect(reply[0].startswith("OK"), reply)
        expect((await agent.ask("HELLO\n")) == ["ERR UNKNOWN"], "UNKNOWN")
        expect((await agent.ask("RESULT\n")) == ["ERR NOSUBMISSION"], "NOSUBMISSION")
        expect((await agent.ask("SUBMIT ABC\n")) == ["ERR BADFORMAT"], "short answers")
        await agent.close()

    async def duplicate():
        sid2 = FIRST_ID + 901
        pin2, exam2 = students[sid2]["pin"], int(students[sid2]["exam_id"])
        agent = await Agent().open(port)
        await agent.ask(f"LOGIN {sid2} {pin2}\n")
        answers = "A" * nq[exam2]
        expect((await agent.ask(f"SUBMIT {answers}\n")) == ["OK"], "first submit")
        expect((await agent.ask(f"SUBMIT {answers}\n")) == ["ERR DUPLICATE"], "second submit")
        await agent.close()
        other = await Agent().open(port)
        await other.ask(f"LOGIN {sid2} {pin2}\n")
        expect((await other.ask(f"SUBMIT {answers}\n")) == ["ERR DUPLICATE"],
               "duplicate on a new connection")
        await other.close()

    async def pipelined():
        agent = await Agent().open(port)
        agent.send(f"LOGIN {sid} {pin}\nQUESTIONS\nQUIT\n")
        r1 = await agent.read_reply()
        r2 = await agent.read_reply()
        r3 = await agent.read_reply()
        expect(r1[0].startswith("OK") and r2[-1] == "END" and r3 == ["BYE"],
               f"{r1[:1]} {r2[-1:]} {r3}")
        await agent.close()

    async def chunked():
        await student_session(port, FIRST_ID + 902, data, think=0, chunked=True)

    async def too_long():
        agent = await Agent().open(port)
        agent.send("X" * 3000)
        await agent.w.drain()
        reply = await agent.read_reply()
        expect(reply == ["ERR TOOLONG"], repr(reply))
        await agent.close()

    async def disconnect():
        for _ in range(5):
            agent = await Agent().open(port)
            agent.send("LOGIN 10")
            await agent.w.drain()
            agent.w.transport.abort()
        agent = await Agent().open(port)
        expect((await agent.ask("QUIT\n")) == ["BYE"], "server alive after aborts")
        await agent.close()

    async def silent():
        idle = await Agent().open(port)
        idle.send("LOGI")
        await idle.w.drain()
        await asyncio.wait_for(student_session(port, FIRST_ID + 903, data, think=0), 5)
        await idle.close()

    checks = [
        ("basic errors", basic),
        ("duplicate submit", duplicate),
        ("pipelined requests", pipelined),
        ("one byte at a time", chunked),
        ("too-long line", too_long),
        ("abrupt disconnects", disconnect),
        ("idle client does not block", silent),
    ]
    for name, function in checks:
        if skip_idle and name.startswith("idle"):
            continue
        await check(name, function())
    return results


async def main_async(args):
    results = await run_checks(args.port, load_data(), args.skip_idle)
    for name, ok, why in results:
        print(f"   {'ok  ' if ok else 'FAIL'} {name} {why}")
    return int(any(not ok for _, ok, _ in results))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=5200)
    parser.add_argument("--skip-idle", action="store_true")
    args = parser.parse_args()
    try:
        sys.exit(asyncio.run(main_async(args)))
    except (ConnectionError, OSError, asyncio.TimeoutError) as error:
        print(f"   FAIL cannot talk to the server: {error!r}")
        sys.exit(1)


if __name__ == "__main__":
    main()
