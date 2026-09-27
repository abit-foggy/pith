# Built-in Tools (Namespaces)

Pith comes packed with powerful built-in tools right out of the box. You don't have to install heavy third-party packages or write dozens of lines of boilerplate just to read a file, check the operating system, or connect over a network.

These built-in tools are organized into **namespaces**:
- **`fs`**: Read, write, and manage files
- **`os`**: Check system details, read environment variables, and get command-line arguments
- **`net`**: Send and receive data over TCP networks

---

## 1. Filesystem Tools: `fs`

Reading and writing files in Pith takes just one line of code:

### Writing to a File
Use `fs.writeFile(path, content)`. It returns `1` if the write succeeded, or `0` if there was an error:

```pith
success = fs.writeFile("notes.txt", "Pith makes coding fun!")

if success
    print "File saved successfully!"
else
    print "Could not write to file."
end
```

### Reading from a File
Use `fs.readFile(path)`. It returns the text inside the file as a string (or an empty string `""` if the file could not be read):

```pith
content = fs.readFile("notes.txt")
print "File contents:"
print content
```

### `fs` Reference

| Function | What it does | Returns |
|---|---|---|
| `fs.readFile(path)` | Reads an entire file | File contents as text (or `""` on error) |
| `fs.writeFile(path, content)` | Writes text into a file | `1` on success, `0` on error |

---

## 2. Operating System Tools: `os`

The `os` namespace lets your program ask questions about the computer it's running on:

```pith
# Check the platform
if os.isLinux
    print "Running on Linux!"
elseif os.isMacOS
    print "Running on macOS!"
elseif os.isNT
    print "Running on Windows!"
end

# Get the exact kernel name
print "Kernel: " + os.identifyKernel
```

### Reading Command-Line Arguments & Environment
You can read arguments passed into your script from the terminal:

```pith
# Check how many arguments were passed:
count = os.argCount

# Read the first argument (0 is the first argument after your script name):
if count > 0
    first_arg = os.getArg(0)
    print "First argument: " + first_arg
end

# Read environment variables (e.g. USER, PATH, HOME):
user = os.getEnv("USER")
print "Current user: " + user
```

### `os` Reference

| Member | What it does | Returns |
|---|---|---|
| `os.identifyKernel` | Operating system kernel | `"linux"`, `"darwin"`, `"nt"`, `"freebsd"` |
| `os.identifyKernelVersion` | Kernel release version string | e.g. `"6.5.0-generic"` |
| `os.isLinux` | Checks if running on Linux | `1` if true, `0` otherwise |
| `os.isMacOS` | Checks if running on Apple macOS | `1` if true, `0` otherwise |
| `os.isNT` | Checks if running on Windows | `1` if true, `0` otherwise |
| `os.isDarwin` | Checks if kernel is Darwin | `1` if true, `0` otherwise |
| `os.isFreeBSD` | Checks if running on FreeBSD | `1` if true, `0` otherwise |
| `os.getEnv(name)` | Gets an environment variable | String value (or `""` if unset) |
| `os.argCount` | Total CLI arguments passed | Integer count |
| `os.getArg(index)` | Gets argument at 0-based index | String argument |
| `os.exit(code)` | Terminates program immediately | Exits with given status code |

---

## 3. Network Tools: `net`

Need to talk to a web server or create a TCP connection? Pith includes dead-simple networking:

```pith
# Connect to a web server on port 80:
fd = net.connect("example.com", 80)

if fd >= 0
    # Send an HTTP request
    net.send(fd, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")

    # Read the response (up to 4096 bytes)
    response = net.recv(fd, 4096)
    print response

    # Close the connection when done
    net.close(fd)
end
```

### `net` Reference

| Function | What it does | Returns |
|---|---|---|
| `net.connect(host, port)` | Connects to a TCP host and port | Socket ID (or `-1` on error) |
| `net.send(fd, message)` | Sends text over the socket | Bytes sent (or `-1` on error) |
| `net.recv(fd, maxBytes)` | Reads text from the socket | Received text (or `""` on EOF/error) |
| `net.close(fd)` | Closes the connection | Nothing |
| `net.socket(domain, type, proto)` | Creates a raw socket | Socket ID (or `-1` on error) |

---

## 4. How Namespaces Work Under the Hood

Pith uses a simple, predictable hierarchy:

1. **`root.fs.*` / `root.os.*` / `root.net.*`**: The built-in runtime functions directly. They can never be overridden.
2. **`fs.*` / `os.*` / `net.*`**: The active tools you use every day. If you import a module that enhances one of these, the enhancement applies here.
3. **`alice.fs.*`**: If you import a custom package from another developer (like Alice), you can call their specific version directly by author name!

All strings, buffers, and data returned by built-in namespaces are automatically managed and cleaned up for you with zero performance overhead.
