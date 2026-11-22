# 🌍 Socket Programming Examples in Different Languages

*Same sockets, same TCP/UDP, different APIs: every example below wraps the C calls from 10b (`socket`, `bind`, `listen`, `accept`, `connect`, `recv`/`send`, `close`).*

## 📚 Table of Contents
1. [C (reference)](#c-reference)
2. [Python](#python-examples)
3. [Java](#java-examples)
4. [Node.js](#nodejs-examples)
5. [Go](#go-examples)
6. [Rust](#rust-examples)
7. [UDP in other languages](#udp-examples)
8. [Comparison Summary](#language-comparison)

---

## 🗺️ Map: language API → C calls

| Step | C | Python | Java | Node.js | Go | Rust |
|------|---|--------|------|---------|----|------|
| create + bind + listen | `socket`+`bind`+`listen` | `socket`+`bind`+`listen` | `new ServerSocket(p)` | `net.createServer().listen` | `net.Listen` | `TcpListener::bind` |
| accept | `accept` | `accept` | `ss.accept()` | `'connection'` callback | `l.Accept()` | `listener.accept()` / `incoming()` |
| connect | `socket`+`connect` | `connect` | `new Socket(h,p)` | `net.connect` | `net.Dial` | `TcpStream::connect` |
| receive / send | `recv` / `send` | `recv` / `sendall` | `read` / `write` | `'data'` / `write` | `Read` / `Write` | `read` / `write_all` |
| close | `close` | `close` / `with` | `close` / try-with-resources | `end` / `destroy` | `Close` / `defer` | `drop` (automatic) |

---

## 🅲 C (reference)
```c
int sfd = socket(AF_INET, SOCK_STREAM, 0);
bind(sfd, (struct sockaddr *)&addr, sizeof(addr));
listen(sfd, 10);
int cfd = accept(sfd, NULL, NULL);
ssize_t n = recv(cfd, buf, sizeof(buf), 0);   // may return less than a "message"!
send(cfd, buf, n, 0);
close(cfd);
```
👉 Full examples in [10b](10b-socket-programming.md). ⚠️ Every `recv(…, 1024)` below has the same **byte-stream caveat** as in C: it may return half a message or two messages. The echo servers work because they do not care about message boundaries; real protocols need framing (10b).

---

## 🐍 Python Examples  
### TCP Server (Multithreaded)  
```python
import socket
import threading

def handle_client(conn):
    try:
        while True:
            data = conn.recv(1024)  # Read data from client
            if not data: 
                break  # Exit loop if client disconnects
            conn.sendall(data)  # Echo back
    except Exception as e:
        print(f"Error: {e}")
    finally:
        conn.close()  # Always close the connection
```
---

```python
# Set up server
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # quick restart
    s.bind(('0.0.0.0', 8080))  # Bind to all network interfaces
    s.listen(5)  # Backlog: pending connections waiting for accept()
    
    print("Server running on port 8080...")
    while True:
        conn, addr = s.accept()  # Wait for connection
        print(f"Connected to {addr}")
        # Start a new thread for each client
        threading.Thread(target=handle_client, args=(conn,), daemon=True).start()
```

---

### 🐍 TCP Client  
```python
import socket

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect(('localhost', 8080))  # Connect to server
    s.sendall(b'Hello')  # Send bytes
    response = s.recv(1024)  # Wait for reply
    print(f"Server says: {response.decode()}")
```

---

### 🐍 Python `asyncio` (event loop, no threads)
```python
import asyncio

async def handle(reader, writer):
    while data := await reader.read(1024):   # b'' = peer closed
        writer.write(data)
        await writer.drain()
    writer.close()
    await writer.wait_closed()

async def main():
    server = await asyncio.start_server(handle, '0.0.0.0', 8080)
    async with server:
        await server.serve_forever()

asyncio.run(main())
```
`asyncio` uses `epoll` under the hood: one thread, thousands of connections.

---

## ☕ Java Examples  
### Multithreaded Server  
```java
import java.net.*;
import java.io.*;

public class Server {
    public static void main(String[] args) {
        try (ServerSocket ss = new ServerSocket(8080)) {  // Auto-close
            System.out.println("Server running on port 8080...");
            
            while (true) {
                Socket client = ss.accept();  // Block until connection
                new Thread(() -> {
                    try (
                        client;  // closed when the handler ends
                        InputStream is = client.getInputStream();
                        OutputStream os = client.getOutputStream()
                    ) {
                        byte[] buffer = new byte[1024];
                        int bytesRead;
                        while ((bytesRead = is.read(buffer)) != -1) {
                            os.write(buffer, 0, bytesRead);  // Echo back
                        }
                    } catch (IOException e) {
                        System.err.println("Client error: " + e.getMessage());
                    }
                }).start();
            }
        } catch (IOException e) {
            System.err.println("Server failed: " + e.getMessage());
        }
    }
}
```

---

### ☕ Virtual threads (Java 21+)
```java
// same server, but thread-per-connection is now cheap (millions of virtual threads)
Thread.ofVirtual().start(() -> handle(client));
// or: ExecutorService ex = Executors.newVirtualThreadPerTaskExecutor();
```
Blocking code, event-loop scalability: the JVM parks a virtual thread when it blocks in `read`.

---

### ☕ Client  
```java
import java.net.*;
import java.io.*;

public class Client {
    public static void main(String[] args) {
        try (Socket s = new Socket("localhost", 8080)) {  // Auto-close
            s.getOutputStream().write("Hello".getBytes());  // Send message
            
            byte[] buffer = new byte[1024];
            int bytesRead = s.getInputStream().read(buffer);
            if (bytesRead > 0)  // -1 means the server closed the connection
                System.out.println("Server says: " + new String(buffer, 0, bytesRead));
        } catch (IOException e) {
            System.err.println("Client error: " + e.getMessage());
        }
    }
}
```

---

## 🟢 Node.js Examples  
### Event-Driven Server  
```javascript
const net = require('net');   // ES modules: import net from 'node:net';

const server = net.createServer((socket) => {
    console.log('Client connected');
    
    socket.on('data', (data) => {
        console.log(`Received: ${data}`);
        socket.write(data);  // Echo back
    });
    
    socket.on('end', () => {
        console.log('Client disconnected');
    });
    
    socket.on('error', (err) => {
        console.error(`Client error: ${err.message}`);
    });
});

server.listen(8080, () => {
    console.log('Server running on port 8080');
});
```

---
### 🟢 Client  
```javascript
const net = require('net');

const client = net.connect({ port: 8080 }, () => {
    console.log('Connected to server!');
    client.write('Hello Server!');
});

client.on('data', (data) => {
    console.log(`Server says: ${data}`);
    client.end();  // Close after response
});

client.on('error', (err) => {
    console.error(`Connection error: ${err.message}`);
});
```
Node runs **one thread + an event loop** (libuv, which uses `epoll`): callbacks must never block.

---

## 🐹 Go Examples  
### Goroutine-Based Server  
```go
package main

import (
    "fmt"
    "net"
)

func handleConnection(conn net.Conn) {
    defer conn.Close()  // Ensure connection closes
    
    buffer := make([]byte, 1024)
    for {
        n, err := conn.Read(buffer)
        if err != nil {
            fmt.Println("Client disconnected:", err)
            return
        }
        fmt.Printf("Received: %s\n", buffer[:n])
        if _, err := conn.Write(buffer[:n]); err != nil {  // Echo back
            return
        }
    }
}

func main() {
    listener, err := net.Listen("tcp", ":8080")
    if err != nil {
        panic("Failed to start server: " + err.Error())
    }
    defer listener.Close()
    
    fmt.Println("Server running on port 8080...")
    for {
        conn, err := listener.Accept()
        if err != nil {
            fmt.Println("Accept error:", err)
            continue
        }
        go handleConnection(conn)  // Handle in a goroutine
    }
}
```

---

### 🐹 Client  
```go
package main

import (
    "fmt"
    "net"
)

func main() {
    conn, err := net.Dial("tcp", "localhost:8080")
    if err != nil {
        panic("Connection failed: " + err.Error())
    }
    defer conn.Close()
    
    _, err = conn.Write([]byte("Hello"))
    if err != nil {
        panic("Send failed: " + err.Error())
    }
    
    buffer := make([]byte, 1024)
    n, err := conn.Read(buffer)
    if err != nil {
        panic("Read failed: " + err.Error())
    }
    fmt.Printf("Server says: %s\n", buffer[:n])
}
```
Goroutines look like blocking code but the Go runtime multiplexes them on `epoll`.

---

## 🦀 Rust Examples  
### Threaded Server  
```rust
use std::net::{TcpListener, TcpStream};
use std::io::{Read, Write};
use std::thread;

fn handle_client(mut stream: TcpStream) -> Result<(), std::io::Error> {
    let mut buffer = [0; 1024];
    loop {
        let bytes_read = stream.read(&mut buffer)?;  // Propagate error
        if bytes_read == 0 { break; }  // Connection closed
        
        stream.write_all(&buffer[..bytes_read])?;  // Echo back
        println!("Received: {}", String::from_utf8_lossy(&buffer[..bytes_read]));
    }
    Ok(())
}

fn main() {
    let listener = TcpListener::bind("0.0.0.0:8080").expect("Failed to bind");
    println!("Server running on port 8080...");
    
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                thread::spawn(move || {   // `move`: the thread owns the stream
                    handle_client(stream).unwrap_or_else(|e| eprintln!("Error: {}", e));
                });
            }
            Err(e) => eprintln!("Connection failed: {}", e),
        }
    }
}
```
---

### 🦀 Client  
```rust
use std::net::TcpStream;
use std::io::{Read, Write};

fn main() -> Result<(), std::io::Error> {
    let mut stream = TcpStream::connect("localhost:8080")?;
    stream.write_all(b"Hello")?;
    
    let mut buffer = [0; 1024];
    let bytes_read = stream.read(&mut buffer)?;
    println!("Server says: {}", String::from_utf8_lossy(&buffer[..bytes_read]));
    
    Ok(())
}
```

---

### 🦀 Async Server with `tokio`
```rust
// Cargo.toml: tokio = { version = "1", features = ["full"] }
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("0.0.0.0:8080").await?;
    loop {
        let (mut sock, addr) = listener.accept().await?;
        tokio::spawn(async move {                 // lightweight task, not an OS thread
            let mut buf = [0u8; 1024];
            loop {
                match sock.read(&mut buf).await {
                    Ok(0) | Err(_) => break,      // closed or failed
                    Ok(n) => if sock.write_all(&buf[..n]).await.is_err() { break; },
                }
            }
            println!("{addr} done");
        });
    }
}
```
The same pattern as `epoll` in C (10b), written as sequential-looking code. The standard library alone gives you blocking sockets + threads; async needs a runtime crate such as `tokio`.

---

## 📨 UDP Examples
*One datagram = one `sendto` = one `recvfrom`. No `listen`/`accept`/`connect`.*

```python
# 🐍 Python: UDP echo server + client
import socket
srv = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
srv.bind(('0.0.0.0', 9000))
while True:
    data, peer = srv.recvfrom(1024)    # whole datagram + sender address
    srv.sendto(data, peer)

# client (in another process)
c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
c.settimeout(2)                         # UDP can lose packets: never wait forever
c.sendto(b'ping', ('127.0.0.1', 9000))
try:
    print(c.recvfrom(1024)[0])
except socket.timeout:
    print('lost?')
```

---

```rust
// 🦀 Rust: UDP echo server + client
use std::net::UdpSocket;
use std::time::Duration;

// server
let sock = UdpSocket::bind("0.0.0.0:9000")?;
let mut buf = [0u8; 1024];
loop {
    let (n, peer) = sock.recv_from(&mut buf)?;
    sock.send_to(&buf[..n], peer)?;
}

// client
let c = UdpSocket::bind("0.0.0.0:0")?;                // port 0 = kernel picks one
c.set_read_timeout(Some(Duration::from_secs(2)))?;
c.send_to(b"ping", "127.0.0.1:9000")?;
let (n, _) = c.recv_from(&mut buf)?;                  // Err(TimedOut) if the datagram was lost
```

---

```javascript
// 🟢 Node.js: UDP echo server + client
const dgram = require('dgram');

const server = dgram.createSocket('udp4');
server.on('message', (msg, rinfo) => server.send(msg, rinfo.port, rinfo.address));
server.bind(9000);

const client = dgram.createSocket('udp4');
client.on('message', (msg) => { console.log(`Reply: ${msg}`); client.close(); });
client.send('ping', 9000, '127.0.0.1');
```

---

```go
// 🐹 Go: UDP echo server
package main

import "net"

func main() {
    pc, _ := net.ListenPacket("udp", ":9000")
    defer pc.Close()
    buf := make([]byte, 1024)
    for {
        n, peer, err := pc.ReadFrom(buf)
        if err != nil { continue }
        pc.WriteTo(buf[:n], peer)
    }
}
```

---

## 📊 Language Comparison Matrix

| Language | Concurrency Model       | Memory Safety | Typing | Performance | Ecosystem | Use Case Suitability              |
|---------|--------------------------|---------------|--------|-------------|-----------|-----------------------------------|
| 🅲 C       | Threads / `fork` / `poll` / `epoll` | ❌ None (manual) | Static | Excellent | Huge, old | OS, embedded, servers, this course |
| 🐍 Python  | Threads (GIL) / `asyncio` | ✅ Runtime (GC) | Dynamic | Moderate    | Rich      | Prototyping, scripting, AI glue   |
| ☕ Java    | Threads / virtual threads | ✅ Runtime (GC) | Static  | Good        | Mature    | Enterprise applications           |
| 🟢 Node.js | Event loop (non-blocking)| ✅ Runtime (GC) | Dynamic | Good        | Huge (npm)| Real-time apps, microservices     |
| 🐹 Go      | Goroutines               | ✅ Runtime (GC) | Static  | Excellent   | Strong (cloud) | Cloud-native, high-concurrency |
| 🦀 Rust    | Threads + `async`/`await` | ✅ Compile time (ownership) | Static | Excellent | Modern, growing | Systems programming, safety-critical |

---

### 🛡️ **Memory Safety**
Memory safety refers to how a language prevents common bugs like:
- **Buffer overflows** (writing past allocated memory)
- **Use-after-free** (accessing memory after it’s deleted)
- **Data races** (two threads, one variable, no synchronization)

C leaves all of these to the programmer; GC languages remove most at runtime; Rust rejects them at **compile time** (unless you write `unsafe`).

*(Null: Java, Python, JS, Go all fail at runtime on `null`/`nil`: Java `NullPointerException`, Go `panic: nil pointer dereference`. Rust has no null: `Option<T>` forces you to handle the empty case.)*

---

## 🔭 Key Observations

1. **Concurrency Models**:
   - Go's goroutines and Rust's `async` (e.g. `tokio`) provide efficient concurrency
   - Python's GIL limits parallel **CPU-bound** threads (threads are still fine for I/O-bound servers; free-threaded builds exist since 3.13)
     -  Python: Use `asyncio` for better concurrency. 
   - Java: threads are explicit; virtual threads (21+) make thread-per-connection cheap

2. **Safety Features**:
   - Rust enforces memory safety **and data-race freedom** at compile time
   - Go has a garbage collector, bounds checks, but `nil` still panics
   - Python/Java handle errors at runtime

---

3. **Development Speed**:
   - Python/Node.js: Fast prototyping
   - Go/Rust: Slightly slower development but better performance

4. **Error Handling**:
   - Rust's `Result` type forces error handling
   - Go uses multiple return values
   - Python/Java use exceptions
   - C: return codes + `errno` (easy to forget to check!)

5. **Standard Library**:
   - All languages have mature networking libraries
   - Go and Rust have modern API designs
   - Under every one of them: the same kernel sockets, `epoll`, TCP/UDP (use `strace -e trace=network` to see it)

---

🧭 **Takeaway:** learn the C API once (10b): every language's networking library is a wrapper around it, and the concepts (byte stream, framing, partial reads, timeouts, concurrency model) transfer unchanged.

---

📖 [Complete Socket Programming Guide](https://www.csd.uoc.gr/~hy556/material/tutorials/cs556-3rd-tutorial.pdf)  
📖 [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)  
📖 `man 7 socket`, `man 7 tcp`, `man 7 udp`  
📖 [Tokio tutorial](https://tokio.rs/tokio/tutorial)
