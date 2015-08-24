# 2d rewriter - a cellular automata simulator

## Key features
- Declarative input language for rules and initial patterns definition.
- Ability to emulate Conway's *Life Game* via proper rules specification.
- Ability to demonstrate self replicating loops.
- Patterns are tried in 4 orientations. 
- Cell directions are defined against the pattern orientation.
- Total number of rules can be substantially decreased by using *sets* and defining
patterns using variables.
- Required run time environment is a minimal X Window system installation on an 
POSIX-compatible system (\*BSD/Linux/Mac OS X/Cygwin/...).

## How to build

- The standard way. Run *xmkmf -a* to generate *Makefile* and then run *make*.
- A less standard way. Run: *gcc `pkg-config --cflags --libs x11` -O3 -o2d-rewriter \*.c*
- The hard way. Run something like: *gcc -Ix11inc -Lx11lib -lX11 -O3 -o2d-rewriter \*.c*
  where *x11inc* is a path to X11 include files and *x11lib* is a path to X11 libraries.
