# Project Documentation: Yield Curve Bootstrapping

This project implements the **Bootstrapping technique** to construct a **Zero Coupon Interest Rate Curve (Discount Curve)** using market-quoted Interest Rate Swap (IRS) rates.

---

# I. Notation

| Variable | Description |
|---------|-------------|
| $T$ | Time to maturity (in years). |
| $t_i$ | The $i$-th cash flow date. |
| $DF(T)$ | The Discount Factor for time $T$. |
| $r(T)$ | The Zero Rate (Spot Rate) for time $T$ (continuously compounded). |
| $\tau_i$ | The time fraction of the accrual period $[t_{i-1}, t_i]$. |
| $S$ | The fixed Par Swap Rate quoted by the market. |
| $N$ | The Notional Principal (typically $1.0$ for valuation). |
| $PV$ | Present Value. |

**Relationship between Zero Rate and Discount Factor:**

$$
DF(T) = e^{-r(T)\, T}
$$

---

# II. Core Assumptions

- **Instrument Set:**  
  The curve is built using short-term Zero Coupon Bonds (ZCBs) followed by Interest Rate Swaps.

- **Payment Frequency:**  
  All swap fixed legs have semi-annual payments ($\tau = 0.5$).

- **Day Count Convention:**  
  Time is measured in exact years (Actual/Actual or similar).

- **Initial ZCB:**  
  The shortest maturity (0.5Y) comes from a money-market deposit treated as a ZCB.

- **Interpolation:**  
  Linear interpolation applied to Zero Rates $r(T)$ between calibrated pillars.

- **No Spread:**  
  Floating leg has no credit spread.

---

# III. Present Value Formulas

## A. Fixed Leg Present Value

The fixed leg is a stream of constant payments:

$$
PV_{\text{Fixed}} = N S \sum_{i=1}^n \left( \tau_i \, DF(t_i) \right)
$$

Where:
- $n$ = number of periods  
- $\tau_i$ = accrual fraction  

---

## B. Floating Leg Present Value

Using the replication argument:

$$
PV_{\text{Floating}} = N(1 - DF(T_n))
$$

### Derivation (Telescoping Sum)

Floating coupon:

$$
PV(\text{Coupon}_i) = N(DF(t_{i-1}) - DF(t_i))
$$

Summing:

$$
PV_{\text{Floating}}
= \sum_{i=1}^{n} N(DF(t_{i-1}) - DF(t_i))
= N(1 - DF(t_n))
$$

---

# IV. The Bootstrapping Process (Calibration)

Bootstrapping solves for each unknown discount factor recursively.

Since swaps are quoted at **zero NPV**:

$$
PV_{\text{Fixed}} = PV_{\text{Floating}}
$$

Separating the known and unknown terms:

$$
N S \left[
\sum_{i=1}^{n-1} (\tau_i DF(t_i)) + \tau_n DF(T_n)
\right]
= N(1 - DF(T_n))
$$

Solving for the unknown **final discount factor**:

$$
\boxed{
DF(T_n) =
\frac{
1 - S \sum_{\text{known}} (\tau_i DF(t_i))
}{
1 + S\,\tau_n
}}
$$

This is repeated for each maturity.

---
