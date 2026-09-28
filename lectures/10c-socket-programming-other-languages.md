# Socket Programming Examples in Different Languages

## Table of Contents
1. [Python](#python-examples)
2. [Java](#java-examples)
3. [Node.js](#nodejs-examples)
4. [Go](#go-examples)
5. [Rust](#rust-examples)
6. [Comparison Summary](#language-comparison)

---

## Python Examples  
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
    s.bind(('0.0.0.0', 8080))  # Bind to all network interfaces
    s.listen(5)  # Queue up to 5 clients
    
    print("Server running on port 8080...")
    while True:
        conn, addr = s.accept()  # Wait for connection
        print(f"Connected to {addr}")
        # Start a new thread for each client
        threading.Thread(target=handle_client, args=(conn,)).start()
```

---

### TCP Client  
```python
import socket

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect(('localhost', 8080))  # Connect to server
    s.sendall(b'Hello')  # Send bytes
    response = s.recv(1024)  # Wait for reply
    print(f"Server says: {response.decode()}")
```

---

## Java Examples  
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

### Client  
```java
import java.net.*;
import java.io.*;

public class Client {
    public static void main(String[] args) {
        try (Socket s = new Socket("localhost", 8080)) {  // Auto-close
            s.getOutputStream().write("Hello".getBytes());  // Send message
            
            byte[] buffer = new byte[1024];
            int bytesRead = s.getInputStream().read(buffer);
            System.out.println("Server says: " + new String(buffer, 0, bytesRead));
        } catch (IOException e) {
            System.err.println("Client error: " + e.getMessage());
        }
    }
}
```

---

## Node.js Examples  
### Event-Driven Server  
```javascript
const net = require('net');

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
### Client  
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

---

## Go Examples  
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
        conn.Write(buffer[:n])  // Echo back
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

### Client  
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

---

## Rust Examples  
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
                thread::spawn(|| {
                    handle_client(stream).unwrap_or_else(|e| eprintln!("Error: {}", e));
                });
            }
            Err(e) => eprintln!("Connection failed: {}", e),
        }
    }
}
```
---

### Client  
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




## Language Comparison Matrix

| Language | Concurrency Model       | Memory Safety | Performance | Ecosystem | Use Case Suitability              |
|---------|--------------------------|---------------|-------------|-----------|-----------------------------------|
| Python  | Threading/asyncio        | Dynamic       | Moderate    | Rich      | Prototyping, scripting            |
| Java    | Threads                  | Static        | Good        | Mature    | Enterprise applications           |
| Node.js | Event loop (non-blocking)| Dynamic       | Good        | Modern    | Real-time apps, microservices     |
| Go      | Goroutines               | Static        | Excellent   | Growing   | Cloud-native, high-concurrency    |
| Rust    | Threads + async          | Safe by design| Excellent   | Modern    | Systems programming, safety-critical|

---

### **Memory Safety**
Memory safety refers to how a language prevents common bugs like:
- **Buffer overflows** (writing past allocated memory)
- **Use-after-free** (accessing memory after it’s deleted)
- **Null pointer dereferences** (crashing when accessing `null`)

---

## Key Observations

1. **Concurrency Models**:
   - Go's goroutines and Rust's async provide efficient concurrency
   - Python's GIL limits true parallelism
     -  Python: Use `asyncio` for better concurrency. 
   - Java's thread management is explicit but verbose

2. **Safety Features**:
   - Rust enforces memory safety at compile time
   - Go prevents null pointer dereferencing
   - Python/Java handle errors at runtime

---

3. **Development Speed**:
   - Python/Node.js: Fast prototyping
   - Go/Rust: Slightly slower development but better performance

4. **Error Handling**:
   - Rust's `Result` type forces error handling
   - Go uses multiple return values
   - Python/Java use exceptions

5. **Standard Library**:
   - All languages have mature networking libraries
   - Go and Rust have modern API designs

---

[Complete Socket Programming Guide](https://www.csd.uoc.gr/~hy556/material/tutorials/cs556-3rd-tutorial.pdf)  
[Linux Socket API Documentation](https://www.kernel.org/doc/html/latest/networking/tls.html)