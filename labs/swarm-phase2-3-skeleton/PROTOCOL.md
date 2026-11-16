# The swarm protocol (Phases 2 and 3)

This is the contract. Your programs may be built any way you like, but they must keep every rule
here: the tests check your programs against it, from the outside. Because every team keeps the same
contract, any team's captain can fly for any team's center, and any viewer can watch any show.

Words used below:

- **center**: the one program everyone connects to. It owns the zones, plans the choreography,
  runs the beat clock, checks every move and publishes what the stage looks like.
- **captain**: flies the drones of one zone of the stage.
- **drone** / **slot**: each captain has `n` slots, `0..n-1`. In Phase 2 a drone lives inside its
  captain (a thread, or however you model it). In Phase 3 every drone is a **performer** process.
- **viewer**, **swarmctl**: watch the show, and steer it.

Nobody starts anybody. Every program is started on its own, by a person or a test script, on any
machine, and finds the others through the center's address.

## 1. Channels

All communication goes through the seven functions of `include/chan.h`. The address picks the
scheme. Each scheme must behave exactly as below.

### 1.1 Line schemes: `local:PATH` and `tcp:HOST:PORT`

- `chan_listen("local:/tmp/x.sock")` creates a Unix-domain stream socket at that path (removing a
  stale file there first). `chan_listen("tcp::7000")` listens on TCP port 7000 on every address;
  `tcp:127.0.0.5:0` listens on one address and any free port.
- `chan_accept(server, ms)` returns the next client, or `NULL` after `ms` milliseconds.
- `chan_open(addr, CHAN_BOTH)` connects. It returns `NULL` with `errno` `ECONNREFUSED` (or
  `ENOENT` for a missing socket file) when nobody listens, and gives up after a few seconds.
- A message is one line. `chan_send(c, "HELLO 0", 7)` writes `HELLO 0\n`. A buffer that already
  holds several lines separated by `\n` is written in one go and arrives as several messages.
- `chan_recv` returns the next line without its `\n`, NUL-terminated. A line that does not fit in
  `cap` is skipped up to its `\n` and reported as `CHAN_TOOBIG`. When the other side has closed and
  no complete line is left, it returns `CHAN_CLOSED`.
- Every line is delivered, in order. Sending to a connection whose other side is gone returns
  `CHAN_CLOSED`; it never kills the program with `SIGPIPE`. Closing a server removes its socket file.

### 1.2 Datagrams: `udp:HOST:PORT`

- A message is one datagram of at most 512 bytes, delivered whole or not at all. It may be lost,
  duplicated or reordered.
- `chan_open("udp:HOST:PORT", CHAN_BOTH)` talks to that one address. If the system reports that
  nobody listens there (on one machine it does, with `ECONNREFUSED`), `chan_recv` returns
  `CHAN_CLOSED`; otherwise silence just times out.
- `chan_listen("udp::0")` receives from anybody. **The reply rule:** `chan_send` on it goes to the
  sender of the last datagram received, and `chan_addr(c, CHAN_REMOTE, ...)` tells who that was.
- With `SWARM_DROP=PCT` in the environment, `chan_send` silently drops that percentage of the
  datagrams it sends (it still returns 0). The tests use it to make the network lossy.

### 1.3 The stage bus: `shm:NAME`

- `chan_open("shm:NAME", CHAN_SEND)` creates the shared memory object `/NAME` (replacing an old
  one). `CHAN_RECV` opens an existing one, or returns `NULL` with `errno` `ENOENT`.
- One sender, any number of receivers. A message is at most `sizeof(Frame)` bytes.
- **The newest message wins.** `chan_send` never waits for anyone. `chan_recv` returns a message
  newer than the last one this receiver got (a new receiver first gets the newest message, if
  there is one), waiting up to `timeout_ms`; a slow receiver skips messages. A receiver never gets
  a torn message (half old, half new). At least 16 receivers can be open at once.
- When the sender closes the bus, the object is removed, and each receiver gets `CHAN_CLOSED` once
  it has received the last message.
- How you lay out the shared memory and how you synchronize it (a sequence counter, a semaphore,
  a lock) is your design. Only your own programs use it.

### 1.4 The recording: `file:PATH`

- `CHAN_SEND` creates (or truncates) the file. Each message is stored as its length, a 4-byte
  little-endian unsigned integer, followed by its bytes. Nothing else is in the file.
- `CHAN_RECV` opens an existing file and returns its messages in order from the start, and
  `CHAN_CLOSED` at the end. A message cut short (the recorder crashed while writing it) counts as
  the end.
- `read`/`write` or `mmap`: your choice.

### 1.5 Every scheme

- `chan_addr` writes the numeric host and returns the port (0 for `local:`, `shm:` and `file:`).
- With `SWARM_BIND=ADDR` in the environment, every `tcp:` and `udp:` socket binds to `ADDR`:
  connections leave from it, and a server given no host listens on it. The tests give each program
  its own `127.0.0.x` this way, as if each ran on a machine of its own.
- `chan_close` frees everything and removes what its side created. After a clean run nothing of
  yours is left in `/dev/shm` or in `/tmp`.

## 2. Input files

**Score** (`data/scores/*.score`): one setting per line, `#` starts a comment.

```
stage 64 36                                     # width and height in cells (at most 255)
tempo 120                                       # beats per minute (0: as fast as possible)
image data/images/heart64x36.pgm                # the picture of the "mosaic" section
section intro   0 15 dawn   calm                # name, first beat, last beat, theme, mood
section finale 16 39 mosaic triumphant          # sections are contiguous, at most 16
```

**Image**: a plain PGM (`P2`): the line `P2`, then width, height and the maximum value, then one
number per pixel, row by row. A pixel is 0 (empty) or a color 1..15. Lines starting with `#` are
comments.

## 3. Messages

Every message is one line of fields separated by single spaces, at most 255 characters. Numbers are
decimal integers. A program ignores a message whose name it does not know, rejects (logs and
drops) a known message with the wrong fields, and never crashes on either.

### 3.1 The first line to the center

Every program reaches the center at its one address (`local:PATH` in Phase 2, `tcp:HOST:PORT` in
Phase 3) and starts by saying who it is:

| First line | Sent by | Then |
|---|---|---|
| `HELLO <udp_port>` | a captain. `udp_port` is its radio port (Phase 3), or 0 when its drones live inside it (Phase 2). | Section 3.2. If every zone already has a captain, it is a **standby**: the center keeps it until a zone needs it. |
| `JOIN` | a performer (Phase 3) | Section 3.4 |
| `ORPHANED <f> <leader> <last_beat>` | a performer whose captain went silent (Phase 3) | Section 3.4 |
| `WATCH` | a viewer | Section 3.6 |
| `OPERATE` | swarmctl | Section 3.5 |

The center closes a connection whose first line is none of these.

### 3.2 Center and captain

**Center to captain**

| Message | Meaning |
|---|---|
| `ASSIGN <leader> <x0> <y0> <x1> <y1> <n>` | You fly zone `leader`: the cells from (x0, y0) to (x1, y1), corners included, with `n` slots. Zones never overlap. |
| `PLACE <f> <x> <y> <color>` | Where the drone of slot `f` is now. Exactly `n` of these follow every `ASSIGN`. |
| `THEME <section> <start> <end> <theme> <mood>` | One section of the score. All sections follow the `PLACE` lines, in order. |
| `PLAN <req> <k>` | The answer to `PLAN_REQ <req>`: `k` keyframe lines follow, each `<beat> <f> <x> <y> <color>`. |
| `BUSY <req> <retry_ms>` | The center cannot plan now: ask again after `retry_ms`. |
| `BEAT <b>` | Fly beat `b` now. |
| `ADOPT <f>` | Phase 3: a performer is coming to slot `f` (Section 3.4). |
| `CRASH` | Exit at once, as if the machine had died: no goodbye, no `LAND`. Used to test failures. |
| `SHUTDOWN` | The show is over: land your drones (Section 5) and exit. |

**Captain to center**

| Message | Meaning |
|---|---|
| `PLAN_REQ <req> <section>` | Plan this section for my zone. `req` is any number you choose; the answer carries it back. |
| `POS <b> <f> <x> <y> <color>` | After beat `b`, the drone of slot `f` is at (x, y) in this color. |
| `ACK <b>` | All `POS` lines of beat `b` are sent. |
| `HEARTBEAT` | I am alive. Sent every `HEARTBEAT_MS` (200 ms). |
| `LOST <f>` | Phase 3: the performer of slot `f` stopped answering. |

**The rules of flying.** A keyframe is a *target*. On `BEAT <b>`, a captain moves each of its drones
toward its keyframe for beat `b` (a drone with no keyframe for `b` keeps its last target). A move
may be at most `MAX_SPEED` (2) cells in x and in y, must stay inside the zone, and must never put
two drones in one cell. When no safe move exists, the drone stays where it is. A plan may be wrong
(the center's planner may be faulty): the captain checks every move itself and never trusts the
plan. Then it reports one `POS` for every drone that is flying and ends the beat with `ACK`.

A `POS` must be the truth: in Phase 3 a captain reports a drone's new cell only after its performer
confirmed it (Section 3.3).

**Plans.** The center's planner is yours to design. A plan gives keyframes for beats `start..end`
of the section, inside the zone, colors 0..15. Formation themes (`dawn`, `waves`, `hold`, ...) are
your choice of shapes; a theme you do not know means "hold the current place". The theme `mosaic`
means: move the drones onto the non-empty pixels of the image inside the zone, in the pixel's color.
Planning may be slow (it stands for a model that thinks), so the center serves `PLAN_REQ` from a
queue and answers `BUSY` when the queue is full.

### 3.3 The radio: captain and performers (Phase 3, `udp:`)

| Message | Direction | Meaning |
|---|---|---|
| `NEXT <f> <b>` | performer to captain | I am ready for beat `b`. This also confirms that I reached my cell for every beat before `b`. |
| `GOTO <f> <b> <x> <y> <color>` | captain to performer | Be at (x, y) in this color on beat `b`. |
| `WAIT <f> <b>` | captain to performer | I am alive; I have nothing for beat `b` yet. |
| `LAND <f>` | captain to performer | The show is over for you: exit. |

The performer always asks and the captain always answers the sender (the reply rule of 1.2):

- A performer starts with `NEXT <f> 0`. On `GOTO` for a beat `g` at or after the one it asked
  for, it flies there and asks `NEXT <f> g+1` at once. On `WAIT`, or with no answer, it asks again
  after `RADIO_RETRY_MS` (50 ms). Duplicates and stale answers (a beat before the one it asked for,
  another slot) are ignored. On `LAND` it exits.
- A captain answers `NEXT <f> <b>` from slot `f`'s performer with its newest `GOTO` for that slot
  if that is for beat `b` or later, and with `WAIT` otherwise. Only the address that took the slot
  may speak for it; anybody else gets `LAND`, unless the slot was adopted (Section 3.4).
- A captain that hears nothing from slot `f` for `PERFORMER_LOST_MS` (3 s) while it has a move for
  it gives the slot up and tells the center `LOST <f>`. A performer that hears nothing from its
  captain for `ORPHAN_MS` (1.5 s), or gets `CHAN_CLOSED`, is an orphan (Section 3.4).

### 3.4 Joining and rescue (Phase 3)

- On `JOIN`, the center gives the performer a free slot of a captain that has a radio: it answers
  `REHOME <leader> <f> <host> <udp_port>` (that captain's radio address: the host the captain's
  connection comes from, and the port from its `HELLO`) and sends that captain `ADOPT <f>`. If no
  slot is free, the connection stays open and the performer is a **spare**: it gets its `REHOME`
  when a slot frees up (`LOST`), or `LAND -1` when the show ends.
- On `ORPHANED <f> <leader> <last_beat>`, the center answers `REHOME` to the captain that now flies
  zone `leader` (it may be a standby that took over, or the same captain if it is still alive) and
  sends that captain `ADOPT <f>`, or answers `LAND <f>` if the zone has no captain with a radio.
- `ADOPT <f>` tells the captain that the next performer to say `NEXT <f> ...` from a new address
  takes slot `f`, even if another performer had it (that one gets `LAND` when it speaks again).
  The newcomer flies the slot's drone from where that drone is: until the captain has a move for
  it, it gets `WAIT`.
- After the answer, the center closes the connection. A performer that gets `LAND` exits.

### 3.5 Operator commands

After `OPERATE`, each line is a command and gets one answer line, `OK ...` or `ERR <why>`:

| Command | Effect |
|---|---|
| `PAUSE`, `RESUME` | Stop and restart the beat clock. |
| `TEMPO <bpm>` | Change the tempo, 0..600 (0: as fast as possible). |
| `KILL <leader>` | Send that zone's captain `CRASH`. |
| `STATUS` | `OK beat=<b> last_beat=<b> captains=<alive> standby=<k> paused=<0/1> tempo=<bpm>` |
| `STOP` | End the show now, cleanly, as if it had reached its last beat. |

### 3.6 Viewers

After `WATCH`, the center sends the frame of every beat as text: one line
`FRAME <beat> <last_beat> <w> <h> <n_captains> <n_followers> <tempo> <paused> <finished> <collisions> <speed_violations>`
followed by `n_captains * n_followers` lines `D <x> <y> <color> <state>`, one per drone in order,
as one send. After the frame with `finished` 1, the center closes the connection.

## 4. Timing and failures

| Constant (`swarm.h`) | Value | Rule |
|---|---|---|
| `HEARTBEAT_MS` | 200 | A captain sends `HEARTBEAT` this often. |
| `CAPTAIN_DEAD_MS` | 1000 | The center declares a captain dead after this long without a line from it, or at once when its connection closes. |
| `RADIO_RETRY_MS` | 50 | A waiting performer repeats `NEXT` this often. |
| `ORPHAN_MS` | 1500 | A performer with no answer from its captain for this long is an orphan. |
| `PERFORMER_LOST_MS` | 3000 | A captain with no word from a performer for this long reports it `LOST`. |

When a captain dies, its drones hover where they were: the frames show them `DRONE_LOST` at their
last cells. If a standby captain is waiting, the center gives it the dead captain's zone at once
(`ASSIGN` with the same `leader`, `PLACE` lines with the drones' last cells, then the sections), and
the show goes on. Without a standby the zone stays frozen. The center closes a dead captain's
connection: a captain that was only frozen (`SIGSTOP`) finds it closed when it continues, and exits.

The center keeps going whatever happens to captains, performers, viewers or operators. A program
whose center disappears exits.

## 5. The show

1. When it starts, the center opens its address, its bus and its recording at once, before any
   captain arrives. It has `--captains N` zones. Before the show, a captain that says `HELLO` gets
   the first free zone (`ASSIGN`, `PLACE`, `THEME`s) at once, and later ones are standbys. After
   the show has started, a captain that joins takes a zone whose captain died, if there is one, and
   is a standby otherwise. Performers may join at any time.
2. The first beat starts when all `N` zones have a captain and every slot of every radio captain has
   a performer, or when `--wait` seconds have passed with at least one captain. Zones with no
   captain stay empty for the whole show.
3. For each beat `b` from 0 to the score's last beat, the center sends `BEAT <b>` to every live
   captain and waits for their `ACK <b>`, at most one beat period (60000 / tempo ms; with tempo 0,
   at most `CAPTAIN_DEAD_MS`). A captain that misses it is **late**: its drones keep their last
   cells in this frame, and its `POS` lines for an old beat are ignored.
4. The center then **audits** the beat. A *speed violation* is a drone that moved more than
   `MAX_SPEED` cells per beat since the last cell the center knew for it, or left the stage. A
   *collision* is a cell holding two or more drones (`DRONE_OK` or `DRONE_LOST`); each such cell
   counts once per beat. Both are added to the frame's totals.
5. It builds the `Frame` of the beat and publishes it: to the bus (`--bus`), to the recording
   (`--record`), and to every `WATCH` viewer.
6. After the last beat, or `STOP`, or `SIGINT`/`SIGTERM`: the last frame has `finished` 1; the center
   sends `SHUTDOWN` to every captain and standby, `LAND` to every spare, closes everything (removing
   its socket file and its bus), prints the `RESULT` line (Section 7) and exits.

A captain that gets `SHUTDOWN` lands its performers and exits. Because it can only answer (the reply
rule), it keeps its radio open for half a second and answers every `NEXT` with `LAND`: a waiting
performer asks every 50 ms, so each one hears it, even with lost datagrams. A performer that still
misses it finds the captain gone, asks the center, finds the center gone too, and exits.

## 6. Frames

### 6.1 The binary frame

The `Frame` struct of `include/swarm.h`, as one message of `sizeof(Frame)` bytes, on `shm:` and
`file:`. Drone `i` is slot `i % n_followers` of zone `i / n_followers`.

`check` lets a reader detect a torn copy: it is the 32-bit FNV-1a hash of all `sizeof(Frame)`
bytes of the frame, computed with `check` set to 0. FNV-1a: start with `h = 2166136261`; for each
byte, `h = (h ^ byte) * 16777619` (modulo 2^32).

### 6.2 The text frame

Section 3.6.

### 6.3 The recording

A `file:` channel (Section 1.4) holding the frames of a show, one per beat, in order. A frame may
repeat its beat (for example while paused). `tests/framecheck` audits any recording.

## 7. Command lines

| Program | Command line |
|---|---|
| center | `center --listen ADDR --score FILE [--captains N] [--followers M] [--bus shm:NAME] [--record file:PATH] [--tempo BPM] [--wait SEC]` |
| captain | `captain --center ADDR [--radio udp:HOST:PORT]` (with `--radio`, its drones are performers) |
| performer | `performer --center ADDR` |
| viewer | `viewer --from ADDR [--text \| --gui \| --check]` (ADDR: `shm:`, `file:`, `local:` or `tcp:`) |
| swarmctl | `swarmctl --center ADDR <command>`, for example `swarmctl --center local:/tmp/s.sock tempo 240` |

Defaults: `--captains 2`, `--followers 8`, `--tempo` from the score, `--wait 10`. You may add options.

At the end of the show the center prints one line to standard output and exits 0 if there was no
collision and no speed violation (1 otherwise):

```
RESULT beats=<b> drones=<d> collisions=<c> speed_violations=<s> late=<l> dead=<k> takeovers=<t> lost=<x> joined=<j> rehomed=<r> landed=<n>
```

`beats`: beats flown. `drones`: `n_captains * n_followers`. `late`: captain-beats with no `ACK` in
time. `dead`: captains that died. `takeovers`: zones given to a standby. `lost`: `LOST` messages.
`joined`: performers given a slot after `JOIN`. `rehomed`: `ORPHANED` answered with `REHOME`.
`landed`: `JOIN` or `ORPHANED` answered with `LAND`. You may add keys after these.

`viewer --check` reads as fast as it can (a replay is not paced), prints
`CHECK frames=<n> torn=<t> last_beat=<b> finished=<0|1>` and exits 0 when no frame was torn and it
saw the last one. `swarmctl` prints the answer line and exits 0 on `OK`, 1 on
`ERR`, 2 when it cannot reach the center.
