Generate Rust-style `///` inline documentation for every public function, method, class, and struct in the module: $ARGUMENTS

---

## Your role

You are an expert C++17 developer and technical writer for the **Math Solver** project. Your job is to write accurate, terse `///` doc comments modelled on Rust's documentation style. Every comment must be derived from reading the actual source — never invent behaviour.

---

## Rust-style doc comment format (applied to C++)

Use `///` line comments placed **immediately above** each declaration. Mirror the Rustdoc section headings exactly:

```cpp
/// Brief one-sentence summary ending with a period.
///
/// Optional longer paragraph with additional context.
///
/// # Arguments
///
/// * `param_name` — Description of what the parameter represents and any
///   constraints (e.g. must be non-null, valid range).
///
/// # Returns
///
/// Description of the return value, including what it means when empty /
/// null / zero. For `Result<T>` returns, describe the `T` on success.
///
/// # Errors
///
/// Bullet list of every `Diagnostic` / error code this function can produce,
/// with the condition that triggers each one. Only include this section if
/// the function returns `Result<T>` or pushes to a `DiagnosticSink`.
///
/// # Panics
///
/// Describe any `assert` / `abort` conditions. Omit the section if there
/// are none.
///
/// # Examples
///
/// ```cpp
/// // Minimal, self-contained usage snippet.
/// ```
ReturnType function_name(ParamType param_name);
```

Rules:
- **Brief line**: one sentence, imperative mood ("Parse the token stream…", "Resolve a variable…"). No leading "This function".
- **Sections**: include only the sections that apply. Never add an empty section.
- **`# Errors`**: mandatory for any function returning `Result<T>` or accepting `DiagnosticSink&`. List error codes as `E0xxx` when visible in `src/diagnostics/kinds/`.
- **`# Examples`**: include a short, compilable snippet for non-trivial public API. Omit for private helpers unless the logic is subtle.
- Module-level file header uses `//!` (inner doc comment style):

```cpp
//! # Module — `src/<path>/filename.hpp`
//!
//! One-paragraph summary of what this file provides and where it fits in
//! the pipeline (e.g. "Part of the algebra layer; converts AST nodes to
//! polynomial form for expansion and factoring.").
```

---

## Instructions

### Step 1 — Locate all files in the module

The module name provided is `$ARGUMENTS`. Map it to source paths:

- If it matches a directory under `src/` (e.g. `algebra`, `eval`, `parser/math`), document **every** `.hpp` and `.cpp` file in that subtree.
- If it matches a single file path, document that file only.
- If ambiguous, glob for `src/**/$ARGUMENTS*` and document every match.

List the files you found before proceeding.

### Step 2 — Read every file in full

Read each file completely before writing a single comment. Identify:

1. **Classes / structs** — purpose, key fields, invariants.
2. **Public methods** — signature, what they do, what they return, what errors they produce.
3. **Free functions** — same as above.
4. **Private / internal helpers** — document if non-trivial; skip trivial getters.
5. **`using` / `typedef` aliases** — add a one-line `///` if the alias name is not self-explanatory.

### Step 3 — Write the documented source

Output the **complete file** with `///` comments inserted. Do not omit any existing code. Preserve all `#include` directives, namespaces, and formatting. Insert the `//!` module header at the top (after the include guard / `#pragma once` line if present).

For each item, follow the section order: brief → long description → `# Arguments` → `# Returns` → `# Errors` → `# Panics` → `# Examples`.

### Step 4 — Write the changes back

Use the Edit tool to write each documented file back to disk, replacing only the comment blocks (do not alter logic). If a file already has `///` comments, improve or replace them — never leave duplicates.

### Step 5 — Verify

Re-read each modified file. Confirm:
- Every public function has a brief line.
- Every `Result<T>`-returning function has an `# Errors` section.
- No section lists behaviour not present in the source.
- No line numbers appear in comments (they go stale).

---

## What NOT to do

- Do not invent error codes, parameter names, or behaviour not in the source.
- Do not use `@param` / `@return` Doxygen tags — use Rust-style `# Arguments` / `# Returns` sections instead.
- Do not add `///` to `#include` lines, preprocessor macros, or closing braces.
- Do not alter any logic, formatting, or non-comment code.
- Do not use Thai or any language other than English.
- Do not document private implementation details that are obvious from the code.
