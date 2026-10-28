# Bayesian Network Inference

Exact inference on discrete Bayesian networks with the **variable elimination** algorithm, written from scratch in C++17 with only the standard library. It includes a parser for the `.bif` format, so it works with the standard networks from the [bnlearn repository](https://www.bnlearn.com/bnrepository/).

```console
$ ./variable_elimination asia.bif lung smoke=yes
...
P(lung | smoke=yes):
  yes   0.1000
  no    0.9000
```

## What's inside

- **`BIFParser`**: reads variables, their domains and the conditional probability tables (both `table` rows and per-parent-assignment rows) into a list of factors.
- **`Factor`**: a table of values over a set of variables, stored flat in row-major order.
- **Factor operations**:
  - `restrictFactor`: applies the evidence
  - `multiplyFactors`: pointwise product
  - `sumOut`: marginalises out a variable
  - `normalize`
- **`variableElimination`**: restricts every factor to the evidence. Then, for each variable in the elimination order, it multiplies the factors that mention the variable and sums it out. Finally it multiplies what is left and normalises.

## Build and run

```bash
g++ -std=c++17 -O2 -o variable_elimination variable_elimination.cpp

./variable_elimination [file.bif] [query] [var=value ...]
./variable_elimination                                         # P(lung) on asia.bif
./variable_elimination asia.bif tub asia=yes xray=yes dysp=yes
./variable_elimination alarm.bif HYPOVOLEMIA
```

The variables that are neither the query nor evidence are eliminated in alphabetical order.

## Included networks

| File | Variables | Description |
|---|---|---|
| `asia.bif` | 8 | Lauritzen & Spiegelhalter's "chest clinic" network |
| `alarm.bif` | 37 | ALARM patient-monitoring network |

## Validation

The results match [pgmpy](https://pgmpy.org)'s `VariableElimination` to 4 decimal places:

| Query | Result |
|---|---|
| P(lung = yes) | 0.0550 |
| P(lung = yes \| smoke = yes) | 0.1000 |
| P(tub = yes \| asia = yes, xray = yes, dysp = yes) | 0.3917 |
| P(HYPOVOLEMIA = TRUE) on `alarm` | 0.2000 |

## Possible extensions

- Min-degree or min-fill heuristics for the elimination order (the order is currently alphabetical)
- Index factors with integer variable IDs instead of `unordered_map<string,int>`, which would be much faster on large networks
- Approximate inference (likelihood weighting, Gibbs sampling) for comparison

## License

MIT
