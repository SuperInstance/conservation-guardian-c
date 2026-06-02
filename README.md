# conservation-guardian-c

A C11 library implementing the **Budget → Profile → Detect → Report** pattern for resource conservation monitoring.

## Overview

Conservation Guardian tracks resource budgets (CPU, memory, disk, bandwidth, time) and detects when usage approaches or exceeds defined thresholds. It uses exponential moving averages and linear regression to predict resource exhaustion.

## Architecture

```
Budget ──► ResourceProfile ──► Guardian ──► Report
              │                  │
              ├─ EMA tracking    ├─ Phase detection
              ├─ Trend analysis  ├─ Violation collection
              └─ Prediction      └─ Recommendations
```

### Core Types

| Type | Purpose |
|------|---------|
| `Budget` | Max/current values with warning/critical thresholds |
| `ResourceProfile` | Per-resource tracking with history, EMA, and trend |
| `Guardian` | Multi-resource monitor with detection engine |
| `Report` | Phase, violations, predictions, recommendations |

### Phases

1. **Normal** — usage below warning threshold
2. **Warning** — usage between warning and critical thresholds
3. **Critical** — usage between critical and emergency thresholds
4. **Emergency** — usage above emergency threshold (default 95%)

## Building

```bash
make
```

## Running Tests

```bash
make test
```

## Usage

```c
#include "conservation_guardian.h"

Guardian g;
guardian_init(&g, detection_config_default());
guardian_add_profile(&g, RESOURCE_CPU, 100.0, 0.7, 0.9);
guardian_add_profile(&g, RESOURCE_MEMORY, 1024.0, 0.8, 0.95);

guardian_record(&g, RESOURCE_CPU, 85.0);
guardian_record(&g, RESOURCE_MEMORY, 600.0);

Report r = guardian_detect(&g);
printf("Phase: %s\n", phase_name(r.phase));
```

## Requirements

- C11 compiler (gcc, clang)
- libm (math library)
- No other dependencies
