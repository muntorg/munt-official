Developer Guidelines
===============

Various coding styles have been used during the history of the codebase, and the current result is not very consistent.
However, we're now trying to converge to a single style, which will be specified below as it emerges.

Please attempt to follow the guidelines where possible:
- **Indentation and whitespace rules**
  - Braces on new lines
  - 4 space indentation (no tabs) for every block except namespaces.
  - No space after function names; one space after `if`, `for` and `while`.
  - If an `if` only has a single-statement `then`-clause, it can appear
    on the same line as the `if`, with braces. In every other case,
    braces are required, and the `then` and `else` clauses must appear
    correctly indented on a new line.

- **Symbol naming conventions**.
  - Variable are lowercase with `C`amel`C`ase for word seperation
    - Global variables have a `g_` prefix e.g. `g_`connManager
  - Variables can have a type prefix where the author feels it aids readability but it is not required, except in the case of raw pointers where it is strongly encouraged.
  Where a prefix is used the letter after the type specifier should be capatilised.
    - `p`ConnManager to signify a raw pointer
    - `n`Data to specify an integer
  - Constant names are all uppercase, and use `_` to separate words
  - Class names and function names start with a capital letter and use CamelCase to seperate words
  - Method names start with a lower case letter and use CamelCase to seperate words
  - Method arguments should prefixed with a _ to avoid shadowing class members

- **Other conventions**
  - Try to apply const correctness as much as possible.
  - Always think about and specify size for types. i.e. prefer `int32_t` or `int64_t` to `int`; prefer `uint32_t` or `uint64_t` to `unsigned int`
  - Variables that are intentionally unused should be marked as such using `[[maybe_unused]]` or `(unused)`
  - Use of `auto` is encouraged in cases where it improves readability e.g. when taking an iterator from a map, but should not be abused in cases where is not necessary e.g. `auto count = 0` is not allowed.
  - Use of `structured bindings` is highly recommended for readability e.g. `for (const auto& [uuid, account] : pactiveWallet->mapAccounts)` instead of `for (const auto& accountIter : pactiveWallet->mapAccounts)`

Block style example:
```c++
int g_count = 0;

namespace foo
{

int ComputeSomething()
{
    ++g_count;
}

class AccountManager
{
    std::string name;
public:
    bool doStuff(const std::string& s_, int n_, [[maybe_unused]] int d_)
    {
        // Comment summarising what this section of code does
        for (int i = 0; i < n; ++i)
        {
            int totalSum = 0;
            // When something fails, return early
            if (!Something()) { return false; }
            ...
            if (SomethingElse(i))
            {
                totalSum += ComputeSomething();
            }
            else
            {
                doStuff(name, totalSum);
            }
        }

        // Success return is usually at the end
        return true;
    }
}
}
```

General C++
-------------

- Assertions should not have side-effects

  - *Rationale*: Even though the source code is set to to refuse to compile
    with assertions disabled, having side-effects in assertions is unexpected and
    makes the code harder to understand

- If you use the `.h`, you must link the `.cpp`

  - *Rationale*: Include files define the interface for the code in implementation files. Including one but
      not linking the other is confusing. Please avoid that. Moving functions from
      the `.h` to the `.cpp` should not result in build errors

- Use the RAII (Resource Acquisition Is Initialization) paradigm where possible. For example by using
  `unique_ptr` for allocations in a function.

  - *Rationale*: This avoids memory and resource leaks, and ensures exception safety

C++ data structures
--------------------

- Never use the `std::map []` syntax when reading from a map, but instead use `.find()`

  - *Rationale*: `[]` does an insert (of the default element) if the item doesn't
    exist in the map yet. This has resulted in memory leaks in the past, as well as
    race conditions (expecting read-read behavior). Using `[]` is fine for *writing* to a map

- Do not compare an iterator from one data structure with an iterator of
  another data structure (even if of the same type)

  - *Rationale*: Behavior is undefined. In C++ parlor this means "may reformat
    the universe", in practice this has resulted in at least one hard-to-debug crash bug

- Watch out for out-of-bounds vector access. `&vch[vch.size()]` is illegal,
  including `&vch[0]` for an empty vector. Use `vch.data()` and `vch.data() +
  vch.size()` instead.

- Vector bounds checking is only enabled in debug mode. Do not rely on it

- Make sure that constructors initialize all fields. If this is skipped for a
  good reason (i.e., optimization on the critical path), add an explicit
  comment about this

  - *Rationale*: Ensure determinism by avoiding accidental use of uninitialized
    values. Also, static analyzers balk about this.

- Use explicitly signed or unsigned `char`s, or even better `uint8_t` and
  `int8_t`. Do not use bare `char` unless it is to pass to a third-party API.
  This type can be signed or unsigned depending on the architecture, which can
  lead to interoperability problems or dangerous conditions such as
  out-of-bounds array accesses

- Prefer explicit constructions over implicit ones that rely on 'magical' C++ behavior

  - *Rationale*: Easier to understand what is happening, thus easier to spot mistakes, even for those
  that are not language lawyers

Strings and formatting
------------------------

- Be careful of `LogPrint` versus `LogPrintf`. `LogPrint` takes a `category` argument, `LogPrintf` does not.

  - *Rationale*: Confusion of these can result in runtime exceptions due to
    formatting mismatch, and it is easy to get wrong because of subtly similar naming

- Use `std::string`, avoid C string manipulation functions

  - *Rationale*: C++ string handling is marginally safer, less scope for
    buffer overflows and surprises with `\0` characters. Also some C string manipulations
    tend to act differently depending on platform, or even the user locale

- Use `ParseInt32`, `ParseInt64`, `ParseUInt32`, `ParseUInt64`, `ParseDouble` from `utilstrencodings.h` for number parsing

  - *Rationale*: These functions do overflow checking, and avoid pesky locale issues

- For `strprintf`, `LogPrint`, `LogPrintf` formatting characters don't need size specifiers

  - *Rationale*: Gulden Core uses tinyformat, which is type safe. Leave them out to avoid confusion
