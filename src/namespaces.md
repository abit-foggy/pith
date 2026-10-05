# Built-in Tools (Namespaces)

Pith comes packed with powerful built-in tools right out of the box. You don't have to install heavy third-party packages or write dozens of lines of boilerplate just to read a file, check the operating system, or connect over a network.

These built-in tools are organized into **namespaces**:
- **`fs`**: Read, write, and manage files
- **`os`**: Check system details and platform kernels
- **`proc`**: Control processes, read CLI arguments, environment variables, and process ID
- **`net`**: Send and receive data over TCP networks
- **`str`**: Search, inspect, and transform ASCII strings

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

### Checking if a File Exists
Use `fs.exists(path)`. It returns `1` if the file or path exists, or `0` if it does not:

```pith
if fs.exists("notes.txt")
    print "notes.txt is present!"
end
```

### Deleting a File
Use `fs.remove(path)` to delete a file. It returns `1` on success, or `0` on error:

```pith
if fs.remove("notes.txt")
    print "File removed."
end
```

### `fs` Reference

| Function | What it does | Returns |
|---|---|---|
| `fs.readFile(path)` | Reads an entire file | File contents as text (or `""` on error) |
| `fs.writeFile(path, content)` | Writes text into a file | `1` on success, `0` on error |
| `fs.exists(path)` | Checks if a file exists | `1` if exists, `0` otherwise |
| `fs.remove(path)` | Deletes a file | `1` on success, `0` on error |

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

---

## 3. Process Tools: `proc`

The `proc` namespace controls the running process: checking command-line arguments, reading environment variables, querying the process ID, and exiting.

### Reading Command-Line Arguments & Environment
You can read arguments passed into your script from the terminal:

```pith
# Check how many arguments were passed:
count = proc.argCount

# Read the first argument (0 is the first argument after your script name):
if count > 0
    first_arg = proc.getArg(0)
    print "First argument: " + first_arg
end

# Read environment variables (e.g. USER, PATH, HOME):
user = proc.getEnv("USER")
print "Current user: " + user

# Get the process ID:
pid = proc.pid
print "Running PID: " + pid
```

### Exiting a Program
Use `proc.exit(code)` to terminate the program immediately with an exit status:

```pith
if count == 0
    print "Error: missing required argument"
    proc.exit(1)
end
```

### Pausing Execution (Sleeping)
Use `proc.sleep(ms)` to pause execution for a given number of milliseconds:

```pith
print "Waiting 250 milliseconds..."
proc.sleep(250)
print "Done!"
```

### `proc` Reference

| Member | What it does | Returns |
|---|---|---|
| `proc.argCount` | Total CLI arguments passed | Integer count |
| `proc.getArg(index)` | Gets argument at 0-based index | String argument |
| `proc.getEnv(name)` | Gets an environment variable | String value (or `""` if unset) |
| `proc.pid` | Process ID of current process | Integer PID |
| `proc.sleep(ms)` | Pauses execution for milliseconds | void |
| `proc.exit(code)` | Terminates program immediately | Exits with given status code |

---

## 4. Network Tools: `net`

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

## 5. String Tools: `str`

The `str` namespace provides byte-oriented string queries, substring tests, and ASCII case folding:

```pith
greeting = "Hello, World!"

# Query byte length
len = str.length(greeting)
print len  # 13

# Substring and prefix/suffix queries
if str.contains(greeting, "World")
    print "Found World!"
end

if str.startsWith(greeting, "Hello")
    print "Starts with Hello"
end

if str.endsWith(greeting, "!")
    print "Ends with exclamation"
end

# Case transformation (ASCII)
shout = str.upper(greeting)
whisper = str.lower(greeting)
print shout    # "HELLO, WORLD!"
print whisper  # "hello, world!"
```

### `str` Reference

| Function | What it does | Returns |
|---|---|---|
| `str.length(s)` | Payload byte length | Integer length |
| `str.contains(s, sub)` | Substring search | `1` if found, `0` otherwise |
| `str.startsWith(s, prefix)` | Prefix match | `1` if true, `0` otherwise |
| `str.endsWith(s, suffix)` | Suffix match | `1` if true, `0` otherwise |
| `str.upper(s)` | Uppercase copy (ASCII) | New string |
| `str.lower(s)` | Lowercase copy (ASCII) | New string |

---

## 6. How Namespaces Work Under the Hood

Pith uses a simple, predictable hierarchy:

1. **`root.fs.*` / `root.os.*` / `root.proc.*` / `root.net.*` / `root.str.*`**: The built-in runtime functions directly. They can never be overridden.
2. **`fs.*` / `os.*` / `proc.*` / `net.*` / `str.*`**: The active tools you use every day. If you import a module that enhances one of these, the enhancement applies here.
3. **`alice.fs.*`**: If you import a custom package from another developer (like Alice), you can call their specific version directly by author name!

All strings, buffers, and data returned by built-in namespaces are automatically managed and cleaned up for you with zero performance overhead.
