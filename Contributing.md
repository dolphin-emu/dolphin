# Dolphin Coding Style & Licensing

If you make any contributions to Dolphin after December 1st, 2014, you are agreeing that any code you have contributed will be licensed under the GNU GPL version 2 (or any later version).

## Coding Style
---

- [Introduction] (#introduction)
- [Styling and formatting] (#styling-and-formatting)
  - [General] (#general)
  - [Naming] (#naming)
  - [Conditionals] (#conditionals)
  - [Classes and Structs] (#classes-and-structs)
- [Code specific] (#code-specific)
  - [General] (#general-1)
  - [Headers] (#headers)
  - [Loops] (#loops)
  - [Functions] (#functions)
  - [Classes and Structs] (#classes-and-structs-1)


## Introduction
---

This guide is for developers who wish to contribute to the Dolphin codebase. It will detail how to properly style and format code to fit this project. This guide also offers suggestions on specific functions and other varia that may be used in code.

Following this guide and formatting your code as detailed will likely get your pull request merged much faster than if you don't (assuming the written code has no mistakes in itself).

## Styling and formatting
---

### General
- Try to limit lines of code to a maximum of 100 characters.
    - Note that this does not mean you should try and use all 100 characters every time you have the chance. Typically with well formatted code, you normally shouldn't hit a line count of anything over 80 or 90 characters.
- The indentation style we use is tabs for initial indentation and then, if vertical alignment is needed, spaces are to be used.
- The opening brace for namespaces, classes, functions, enums, structs, unions, conditionals, and loops go on the next line.
  - With array initializer lists and lambda expressions it is OK to keep the brace on the same line.
- References and pointers have the ampersand or asterisk against the type name, not the variable name. Example: `int* var`, not `int *var`.
- Don't use multi-line comments (`/* Comment text */`), use single-line comments (`// Comment text`) instead.
- Don't collapse single line conditional or loop bodies onto the same line as its header. Put it on the next line.
  - Yes:

    ```c++
    if (condition)
        return 0;

    while (var != 0)
        var--;
    ```
  - No:

    ```c++
    if (condition) return 0;

    while (var != 0) var--;
    ```

### Naming
- All class, enum, function, and struct names should be in upper CamelCase. If the name contains an abbreviation uppercase it.
  - `class SomeClassName`
  - `enum IPCCommandType`
- All compile time constants should be fully uppercased. With constants that have more than one word in them, use an underscore to separate them.
  - `const int PI = 3.14159;`
  - `const int MAX_PATH = 260;`
- All variables should be lowercase with underscores separating the individual words in the name.
  - `int this_variable_name;`
- Please do not use [Hungarian notation](http://en.wikipedia.org/wiki/Hungarian_notation) prefixes with variables. The only exceptions to this are the variable prefixes below.
  - Global variables – `g_`
  - Class variables – `m_`
  - Static variables – `s_`

### Conditionals
- Do not leave `else` or `else if` conditions dangling unless the `if` condition lacks braces.
  - Yes:

    ```c++
    if (condition)
    {
        // code
    }
    else
    {
        // code
    }
    ```
  - Acceptable:

    ```c++
    if (condition)
        // code line
    else
        // code line
    ```
  - No:

    ```c++
    if (condition)
    {
        // code
    }
    else
        // code line
    ```


### Classes and Structs
- If making a [POD](http://en.wikipedia.org/wiki/Plain_Old_Data_Structures) type, use a `struct` for this. Use a `class` otherwise.
- Class layout should be in the order, `public`, `protected`, and then `private`.
  - If one or more of these sections are not needed, then simply don't include them.
- For each of the above specified access levels, the contents of each should follow this given order: constructor, destructor, operator overloads, functions, then variables.
- When defining the variables, define `static` variables before the non-static ones.

```c++
class ExampleClass : public SomeParent
{
public:
    ExampleClass(int x, int y);

    int GetX() const;
    int GetY() const;

protected:
    virtual void SomeProtectedFunction() = 0;
    static float s_some_variable;

private:
    int m_x;
    int m_y;
};
```

## Code Specific
---

### General
- Using C++11 features is OK and recommended.
- Use the [nullptr](http://en.cppreference.com/w/cpp/language/nullptr) type over the macro `NULL`.
- If a [range-based for loop](http://en.cppreference.com/w/cpp/language/range-for) can be used instead of container iterators, use it.
- Obviously, try not to use `goto` unless you have a *really* good reason for it.
- If a compiler warning is found, please try and fix it.
- Try to avoid using raw pointers (pointers allocated with `new`) as much as possible. There are cases where using a raw pointer is unavoidable, and in these situations it is OK to use them. An example of this is functions from a C library that require them. In cases where it is avoidable, the STL usually has a means to solve this (`vector`, `unique_ptr`, etc).
- Do not use the `auto` keyword everywhere. While it's nice that the type can be determined by the compiler, it cannot be resolved at 'readtime' by the developer as easily. Use auto only in cases where it is obvious what the type being assigned is (note: 'obvious' means not having to open other files or reading the header file). Some situations where it is appropriate to use `auto` is when iterating over a `std::map` container in a foreach loop, or to shorten the length of container iterator variable declarations.
- Do not use `using namespace [x];` in headers. Try not to use it at all if you can.
- The preferred form of the increment and decrement operator in for-loops is prefix-form (e.g. `++var`).

### Headers
- If a header is not necessary in a certain source file, remove them.
- If you find duplicate includes of a certain header, remove it.
- When declaring includes in a source file, make sure they follow the given pattern:
  - Standard library headers
  - System-specific headers (these should also likely be in an `#ifdef` block unless the source file itself is system-specific).
  - Dolphin source file headers
- Each of the above header sections should also be in alphabetical order
- Project source file headers should be included in a way that is relative to the `[Dolphin Root]/Source/Core` directory.
- This project uses `#pragma once` as header guards.

### Loops
- If an infinite loop is required, do not use `for (;;)`, use `while (true)`.
- Empty-bodied loops should use braces after their header, not a semicolon.
  - Yes: `while (condition) {}`
  - No: `while (condition);`

### Functions
- If a function parameter is a pointer or reference and its value or data isn't intended to be changed, please mark that parameter as `const`.
- Functions that specifically modify their parameters should have the respective parameter(s) marked as a pointer so that the variables being modified are syntaxically obvious.
  - What not to do:

    ```c++
    template<class T>
    inline void Clamp(T& val, const T& min, const T& max)
    {
        if (val < min)
            val = min;
        else if (val > max)
            val = max;
    }
    ```

    Example call: `Clamp(var, 1000, 5000);`

  - What to do:

    ```c++
    template<class T>
    inline void Clamp(T* val, const T& min, const T& max)
    {
        if (*val < min)
            *val = min;
        else if (*val > max)
            *val = max;
    }
    ```

    Example call: `Clamp(&var, 1000, 5000);`

- Class member functions that you do not want to be overridden in inheriting classes should be marked with the `final` specifier.

  ```c++
  class ClassName : ParentClass
  {
  public:
      void Update() final;
  };
  ```

- Overridden member functions that can also be inherited should be marked with the `override` specifier to make it easier to see which functions belong to the parent class.

  ```c++
  class ClassName : ParentClass
  {
  public:
      void Update() override;
  };
  ```

### Classes and Structs
- Classes and structs that are not intended to be extended through inheritance should be marked with the `final` specifier.

  ```c++
  class ClassName final : ParentClass
  {
      // Class definitions
  };
  ```
