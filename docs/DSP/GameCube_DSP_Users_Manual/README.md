# Building the GameCube DSP User's Manual

1. Install a LaTeX environment and implementation (for example, MiKTeX).
2. Once installed, install any of the packages within `GameCube_DSP_Users_Manual.tex` that aren't installed
   by default in the environment via the implementation's package manager.
   - These are searchable at the top of the TeX file via the `\usepackage` statements.
3. Once all relevant packages are installed, run `pdflatex` (or any other output processor for LaTeX files) using `GameCube_DSP_Users_Manual.tex` as the input. Run the processor over the TeX file twice -- once to generate the main textual content, and the other time to build and integrate any indexes (such as the table of contents).
4. If all goes well, it should build fine with no errors.