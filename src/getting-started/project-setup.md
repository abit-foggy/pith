# Tutorial: Organizing a Project

When your program grows beyond a single file, Pith makes it easy to organize your code into a clean, professional project with dependencies and automated tasks.

In this tutorial, you'll learn how a Pith project is structured, how to split code into multiple files, and how to use `pith.toml`.

---

## Step 1: Project Structure

Here is what a complete Pith project typically looks like:

```text
my_game/
├── pith.toml        # Project settings and tasks
├── main.pi          # Your main starting file
├── helpers.pi       # Extra helper functions
└── dist/            # Where built apps go (optional)
```

You don't need complicated build systems or endless configuration files—just a folder, your `.pi` scripts, and an optional `pith.toml`.

---

## Step 2: Creating `pith.toml`

The `pith.toml` file stores basic information about your project and custom commands you want to run.

Create a file named `pith.toml` in your project folder:

```toml
[project]
name = "my_game"
version = "0.1.0"

[build]
target = "native"

[tasks.start]
run = "pith run main.pi"

[tasks.test]
all = "pith run tests/test_game.pi"
```

---

## Step 3: Splitting Code Across Multiple Files

As your app grows, you can divide your code into separate files to keep everything tidy.

For example, create `helpers.pi` with a helper function:

```pith
# helpers.pi
fn show_banner(title)
    print "===================="
    print "   " + title
    print "===================="
end
```

Then create your main program in `main.pi`:

```pith
# main.pi
show_banner("MY AWESOME GAME")
print "Ready to play!"
```

To run both files together, simply pass them to `pith run`:

```sh
pith run main.pi helpers.pi
```

Output:
```text
====================
   MY AWESOME GAME
====================
Ready to play!
```

To build a standalone executable from all files together:

```sh
pith build main.pi helpers.pi -o my_game
./my_game
```

Pith automatically combines them into a single, lightning-fast native executable.

---

## Step 4: Running Custom Shortcut Tasks

Remember the `[tasks]` we added to `pith.toml`? You can run them anytime as shortcuts:

```sh
# Runs "pith run main.pi" automatically
pith start

# Runs your test suite
pith test all
```

This saves you from typing long terminal commands over and over.

---

## Step 5: Adding Packages with `pith pkg`

If you want to use a library written by another developer, install it with one command:

```sh
pith pkg add os-utils 1.0.0
```

Pith downloads the package and saves it locally in `.pith/pkgs/`. You can immediately use it in your code!

---

## Step 6: Built-in Time Machine (`--embed-source`)

Have you ever lost the source code to an executable you built months ago? Pith has a built-in superpower:

```sh
pith build main.pi helpers.pi --embed-source -o my_game
```

When you add `--embed-source`, Pith safely packs your project files directly inside the executable. Anyone with the binary can recover the original source code:

```sh
pith decompile ./my_game
```

Your files are cleanly restored to `./restored_workspace/`. It's like having source code version recovery baked right into your binary!

---

## Next Steps

Now that your project is organized:

- **[Built-in Tools](../namespaces.md)** — Read and write files (`fs`), inspect the computer (`os`), and connect to sockets (`net`)
- **[Interactive REPL](../cli/repl.md)** — Try out Pith commands live in your terminal
