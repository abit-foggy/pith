# Tutorial: Your First Script

Welcome to Pith! In this tutorial, you'll write your very first program, learn the basic building blocks, and turn your script into a real standalone application that runs at native speed.

It takes less than 5 minutes, so let's jump right in!

---

## Step 1: Hello, World!

First, create a new file named `hello.pi` in your favorite text editor.

Add this single line of code to it:

```pith
# hello.pi
print "Hello from Pith!"
```

### Running Your Script

Open your terminal in the same folder and type:

```sh
pith run hello.pi
```

You will see:
```text
Hello from Pith!
```

Congratulations! You just ran your first Pith program. Notice how instantaneous it was? `pith run` compiles and executes your code directly into memory in milliseconds.

---

## Step 2: Adding a Variable

Let's make our program a little more personal. Instead of hardcoding the message, let's store a name in a variable:

```pith
# hello.pi
name = "Alex"
print "Hello, " + name + "!"
```

Run it again:
```sh
pith run hello.pi
```

Output:
```text
Hello, Alex!
```

### Clean Syntax
Look closely at what we wrote:
- No semicolons `;` at the end of lines
- No parentheses required around `print`
- Just clean, natural code

---

## Step 3: Making Decisions with `if`

Now let's teach our script how to make choices. We'll check if the player's score is high enough to win:

```pith
# hello.pi
player = "Alex"
score = 100

print "Welcome, " + player + "!"

if score >= 100
    print "You win the game!"
else
    print "Keep playing!"
end
```

Run it:
```sh
pith run hello.pi
```

Output:
```text
Welcome, Alex!
You win the game!
```

In Pith, code blocks don't need curly braces `{}` or indentation rules. You open an `if` block, write your code, and close it with a single `end`.

---

## Step 4: Making Numbers Change with `while` and `mut`

Now let's add a loop that counts down from 3 before starting:

```pith
# hello.pi
mut count = 3

while count > 0
    print "Starting in: " + count
    count = count - 1
end

print "Go!"
```

Notice the word `mut` in front of `count`? By default, Pith prevents variables from changing so you don't accidentally introduce bugs. Adding `mut` (short for *mutable*) tells Pith: *"I want this variable to change as the program runs!"*

Run the script:
```sh
pith run hello.pi
```

Output:
```text
Starting in: 3
Starting in: 2
Starting in: 1
Go!
```

---

## Step 5: Build a Standalone Executable

Here is where Pith really shines. With other beginner-friendly languages, you need a heavy runtime, virtual machine, or interpreter installed on every computer that runs your script.

With Pith, you can compile your script into a **single, standalone binary file** with one command:

```sh
pith build hello.pi
```

Pith builds a standalone executable named `hello`. Now run it directly from your terminal:

```sh
./hello
```

You can take this `hello` file and run it on another computer without needing to install Pith at all! It runs directly on the processor with maximum speed and minimum memory usage.

---

## What's Next?

You now know how to write scripts, store variables, make decisions, repeat actions, and build standalone apps.

Here are great places to explore next:

- **[Variables & Mutability](../language/variables.md)**: Learn how variables and `mut` work in depth
- **[Functions](../language/functions.md)**: Break your code into reusable actions
- **[Working with Files & Network](../namespaces.md)**: Read files, save data, and connect to servers
- **[Setting Up a Project](project-setup.md)**: Organize larger projects with multiple files
