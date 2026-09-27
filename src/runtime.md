# Memory & Performance

One of Pith's best features is that **you never have to manage memory yourself**. 

In some languages (like C), you have to manually allocate and free memory, which is easy to mess up. In other languages (like Python or Java), a heavy "garbage collector" runs in the background, which can cause sudden stuttering and slowdowns.

Pith gives you the best of both worlds: **automatic cleanup with zero stuttering and maximum speed.**

---

## How It Works (Without the Jargon)

When you create a variable or join two strings together, Pith saves it in memory. It keeps a small counter on that data called a *reference count*:

1. **When you create or share data**, Pith notes that it's in use.
2. **When your code finishes with that data** (like exiting an `if` block, a loop, or a function), Pith frees that memory immediately and automatically.

There is no background collector sweeping through memory, no "stop-the-world" freeze, and no sluggish memory leaks. Everything happens right when your code finishes using it.

---

## Why This Matters for You

### 1. Perfect for Games & Audio
In game development and audio processing, even a 10-millisecond pause can ruin the experience. Because Pith cleans up data continuously in real time, your animations and sounds stay buttery smooth.

### 2. Tiny Memory Footprint
Pith programs only use the exact amount of memory they need at any given moment. They don't require hundreds of megabytes of RAM just to start up.

### 3. Native Machine Code
When you run `pith build`, Pith produces a real binary tailored to your processor. It doesn't run inside an emulated virtual machine; it runs directly on the metal for top-tier speed.

---

## Sized Types & Saving Space

If you are working with large sets of numbers (like image pixels, sound waves, or 3D coordinates), you can tell Pith the exact size of your variables:

```pith
# Use u8 (0 to 255) for RGB color channels to use only 1 byte per value:
mut red: u8 = 255
mut green: u8 = 120
mut blue: u8 = 0
```

By choosing the right size, you can make your programs use a fraction of the memory of other scripting languages while running even faster.

---

## Summary

You don't need to be a systems engineer to build high-performance software. With Pith:
- You never write `malloc` or `free`
- You never experience garbage collector pauses
- Your programs start instantly and run at native speed
