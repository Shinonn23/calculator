# Built-in Math Functions

## Overview

Math Solver supports 17 built-in unary functions (sin, cos, sqrt, etc.). They are resolved at **parse time** into a `FunctionCall` AST node and dispatched via a `switch` on `FuncKind` — no string hashing at evaluation time.

### Pipeline integration

```
"sin(pi/2)"
    │
    ▼
Parser::parse_primary()
  identifier "sin" followed by "("  →  func_kind_from_name("sin") → FuncKind::Sin
  parse argument expression: pi/2
  consume ")"
    │
    ▼
FunctionCall("sin", FuncKind::Sin, arg=BinaryOp(pi, 2, Div))
    │
    ├─▶ Evaluator::visit(FunctionCall)     → std::sin(arg_value)
    ├─▶ Expander::visit(FunctionCall)      → expand arg, rebuild FunctionCall
    ├─▶ LinearCollector::visit(FunctionCall)
    │       constant arg → numerically evaluate
    │       variable arg → E0315 non-linear error
    ├─▶ ASTToPolynomial::visit(FunctionCall) → E0310 reject unconditionally
    └─▶ Resolver::resolve_recursive(FunctionCall) → same dispatch as Evaluator
```

---

## Step-by-step: adding a new built-in function

### 1. Add the enum variant

In `src/ast/math/call_expr.hpp`, add to `FuncKind`:

```cpp
enum class FuncKind {
    Sin, Cos, /* ... existing ... */,
    MyFunc   // ← add here
};
```

### 2. Register the name

In the same file, add an entry to `func_kind_from_name()`:

```cpp
{"myfunc", FuncKind::MyFunc},
```

### 3. Implement evaluation in `Evaluator`

In `src/eval/evaluator.cpp`, add a `case` to `visit(const FunctionCall& node)`:

```cpp
case FuncKind::MyFunc:
    result_ = my_computation(x);
    break;
```

For functions with domain restrictions, push an `errors::func_domain` diagnostic and return early (see the `sqrt`/`ln` cases as templates).

### 4. Implement in `LinearCollector`

In `src/algebra/linear/linear_collector.hpp`, add the same `case` inside `visit(const FunctionCall& node)`:

```cpp
case FuncKind::MyFunc:
    val = my_computation(x);
    break;
```

This enables `:solve myfunc(constant) + x = 2` to work.

### 5. Implement in `Resolver`

In `src/runtime/context/resolver.cpp`, add the `case` to the `FunctionCall` branch:

```cpp
case FuncKind::MyFunc:
    return Result<double>::ok(my_computation(x));
```

### 6. No changes needed for `Expander` or `ASTToPolynomial`

- `Expander` recursively expands the argument then rebuilds the `FunctionCall` node — no case needed.
- `ASTToPolynomial` unconditionally rejects all `FunctionCall` nodes — no case needed.

### 7. Add a test

In `tests/ui/math_functions.msl`, add lines exercising the new function and run:

```bash
python tests/run_ui_tests.py --binary build/bin/cmath-solver.exe --tests-dir tests/ui --bless
```

---

## Checklist

- [ ] `FuncKind` variant added in `call_expr.hpp`
- [ ] Name registered in `func_kind_from_name()` lookup table
- [ ] `case` added in `Evaluator::visit(FunctionCall)`
- [ ] `case` added in `LinearCollector::visit(FunctionCall)`
- [ ] `case` added in `Resolver::resolve_recursive` FunctionCall branch
- [ ] Domain errors use `errors::func_domain(message, span, input)` (code `E0002`)
- [ ] `tests/ui/math_functions.msl` extended and blessed
- [ ] `README.md` Built-in Functions table updated

---

## Complete function reference

| Name    | `FuncKind`    | Domain restriction          | C++ implementation  |
| ------- | ------------- | --------------------------- | ------------------- |
| sin     | `Sin`         | none                        | `std::sin`          |
| cos     | `Cos`         | none                        | `std::cos`          |
| tan     | `Tan`         | none                        | `std::tan`          |
| asin    | `Asin`        | `x ∈ [-1, 1]`               | `std::asin`         |
| acos    | `Acos`        | `x ∈ [-1, 1]`               | `std::acos`         |
| atan    | `Atan`        | none                        | `std::atan`         |
| sinh    | `Sinh`        | none                        | `std::sinh`         |
| cosh    | `Cosh`        | none                        | `std::cosh`         |
| tanh    | `Tanh`        | none                        | `std::tanh`         |
| exp     | `Exp`         | none                        | `std::exp`          |
| sqrt    | `Sqrt`        | `x ≥ 0`                     | `std::sqrt`         |
| ln      | `Ln`          | `x > 0`                     | `std::log`          |
| log     | `Log`         | `x > 0`                     | `std::log10`        |
| abs     | `Abs`         | none                        | `std::abs`          |
| floor   | `Floor`       | none                        | `std::floor`        |
| ceil    | `Ceil`        | none                        | `std::ceil`         |
| round   | `Round`       | none                        | `std::round`        |
