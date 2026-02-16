This document defines the standard for writing documentation for the Math Solver project.  
Rules: Whenever a new feature is implemented or significant logic changes are made (e.g., new commands, solver functionality, or parsing logic), developers must create or update documentation in the `docs/*` folder following the format specified below.

1. Documentation Format Standard

New feature documentation must not only include a "User Guide" but also an "Implementation Walkthrough" to explain the internal mechanics. The documentation must include the following sections:

1.1 File Structure

- **Title**: Name of the feature or command.
- **Table of Contents**: Links to various sections.
- **Overview**: A high-level explanation of the functionality (diagrams or text-based flowcharts are recommended).
- **Step-by-Step Walkthrough**: A detailed explanation of the process from input to processing to output.
- **Internal Logic**: Explanation of the classes, functions, or design patterns used (e.g., Visitor Pattern, Recursion).
- **Data Structure / AST**: Representation of how data changes at each step (e.g., AST structure).
- **Error Handling / Edge Cases**: Scenarios where the system may fail or its limitations.

2. Example Template (Follow this format)

Adhere to the following writing format using the `solve` command as an example:

# [Feature Name] — Detailed Walkthrough

## Table of Contents
1. [Overview](#1-overview)
2. [Step 1 — [Process Name]](#step-1)
3. [Step 2 — [Process Name]](#step-2)
...

---

## 1. Overview

Describe the main flow of this feature:
Input: "..."
    │
    ├─ Step 1: [Component A] ──→ [Result A]
    ├─ Step 2: [Component B] ──→ [Result B]
    └─ Output

---

## Step 1 — [Step Name, e.g., Parse / Lex]

Show the main code snippet being used:
```cpp
// filename.cpp
auto result = component.process(input);
```

What happens inside?

Explain the logic in detail:

- Token transformation.
- Object creation.
- Algorithms used.

Example data structure at this step:

```
Structure Name
├── Field A: Value
└── Field B: Value
```

---

## Step 2 — [Step Name, e.g., Evaluate / Solve]

Explain how memory or context is handled:

- Are variables modified?
- What patterns are used? (e.g., Visitor)

[!NOTE]
Add notes to highlight important points or exceptions.

---

## Step [N] — Save Results & Display Output

Explain the side effects:

- Save to context.
- Print to the screen.

---

## 3. Checklist Before Submitting the Feature

- [ ] Create a new `.md` file in the `docs/` folder (e.g., `docs/integration_command.md`).
- [ ] Explain all steps from input to output.
- [ ] Include code or pseudo-code examples.
- [ ] Show the structure of the AST or relevant memory states.
- [ ] Add a link to this new documentation file in the main `README.md` of the project.
