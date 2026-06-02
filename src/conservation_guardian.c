#include "conservation_guardian.h"
#include <math.h>
#include <string.h>

/* ── Utility ──────────────────────────────────────────────────── */

double cg_min(double a, double b) { return a < b ? a : b; }
double cg_max(double a, double b) { return a > b ? a : b; }

/* ── Budget ───────────────────────────────────────────────────── */

Budget budget_create(double max_value, double warning_threshold, double critical_threshold) {
    Budget b;
    b.max_value = max_value;
    b.current_value = 0.0;
    b.warning_threshold = warning_threshold;
    b.critical_threshold = critical_threshold;
    return b;
}

double budget_remaining(const Budget *b) {
    double rem = b->max_value - b->current_value;
    return rem < 0.0 ? 0.0 : rem;
}

double budget_usage(const Budget *b) {
    if (b->max_value <= 0.0) return 1.0;
    double u = b->current_value / b->max_value;
    return u > 1.0 ? 1.0 : u;
}

bool budget_in_warning(const Budget *b) {
    return budget_usage(b) >= b->warning_threshold;
}

bool budget_in_critical(const Budget *b) {
    return budget_usage(b) >= b->critical_threshold;
}

/* ── ResourceType ─────────────────────────────────────────────── */

static const char *resource_type_names[] = {
    "CPU", "Memory", "Disk", "Bandwidth", "Time"
};

const char *resource_type_name(ResourceType t) {
    if (t >= 0 && t < RESOURCE_TYPE_COUNT) return resource_type_names[t];
    return "Unknown";
}

/* ── Phase ────────────────────────────────────────────────────── */

static const char *phase_names[] = {
    "Normal", "Warning", "Critical", "Emergency"
};

const char *phase_name(Phase p) {
    if (p >= 0 && p < 4) return phase_names[p];
    return "Unknown";
}

/* ── AlertLevel ───────────────────────────────────────────────── */

static const char *alert_level_names[] = {
    "Info", "Warning", "Critical", "Emergency"
};

const char *alert_level_name(AlertLevel a) {
    if (a >= 0 && a < 4) return alert_level_names[a];
    return "Unknown";
}

/* ── ResourceProfile ──────────────────────────────────────────── */

void profile_init(ResourceProfile *p, ResourceType type, double max_value,
                  double warning_threshold, double critical_threshold) {
    memset(p, 0, sizeof(*p));
    p->type = type;
    p->budget = budget_create(max_value, warning_threshold, critical_threshold);
    p->history_count = 0;
    p->trend = TREND_STABLE;
    p->ema = 0.0;
}

void profile_record(ResourceProfile *p, double value) {
    /* ring buffer */
    if (p->history_count < CG_HISTORY_SIZE) {
        p->usage_history[p->history_count++] = value;
    } else {
        memmove(p->usage_history, p->usage_history + 1,
                (CG_HISTORY_SIZE - 1) * sizeof(double));
        p->usage_history[CG_HISTORY_SIZE - 1] = value;
    }

    /* update EMA with alpha = 0.3 */
    double alpha = 0.3;
    p->ema = (p->history_count == 1) ? value : alpha * value + (1.0 - alpha) * p->ema;

    /* update budget current value */
    p->budget.current_value = value;
    p->trend = profile_compute_trend(p);
}

Trend profile_compute_trend(const ResourceProfile *p) {
    if (p->history_count < 2) return TREND_STABLE;

    /* linear regression on usage history */
    size_t n = p->history_count;
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    for (size_t i = 0; i < n; i++) {
        double x = (double)i;
        double y = p->usage_history[i];
        sum_x  += x;
        sum_y  += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    double denom = (double)n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-12) return TREND_STABLE;
    double slope = ((double)n * sum_xy - sum_x * sum_y) / denom;

    double threshold = 0.01;
    if (slope > threshold)  return TREND_INCREASING;
    if (slope < -threshold) return TREND_DECREASING;
    return TREND_STABLE;
}

double profile_predict_exhaustion(const ResourceProfile *p) {
    if (p->history_count < 2) return -1.0; /* unknown */

    size_t n = p->history_count;
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    for (size_t i = 0; i < n; i++) {
        double x = (double)i;
        double y = p->usage_history[i];
        sum_x  += x;
        sum_y  += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    double denom = (double)n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-12) return -1.0;

    double slope = ((double)n * sum_xy - sum_x * sum_y) / denom;
    double intercept = (sum_y - slope * sum_x) / (double)n;

    if (slope <= 1e-12) return -1.0; /* never exhausts */

    double steps = (p->budget.max_value - intercept) / slope - (double)(n - 1);
    return steps > 0.0 ? steps : 0.0;
}

/* ── Report ───────────────────────────────────────────────────── */

void report_init(Report *r) {
    memset(r, 0, sizeof(*r));
}

void report_add_violation(Report *r, ResourceType t) {
    if (r->violation_count < CG_MAX_VIOLATIONS) {
        r->violations[r->violation_count++] = t;
    }
}

void report_add_prediction(Report *r, double steps) {
    if (r->prediction_count < CG_MAX_PREDICTIONS) {
        r->predictions[r->prediction_count++] = steps;
    }
}

void report_add_recommendation(Report *r, const char *rec) {
    if (r->recommendation_count < CG_MAX_RECOMMENDATIONS) {
        r->recommendations[r->recommendation_count++] = rec;
    }
}

/* ── DetectionConfig ──────────────────────────────────────────── */

DetectionConfig detection_config_default(void) {
    DetectionConfig c;
    c.ema_alpha = 0.3;
    c.trend_slope_threshold = 0.01;
    c.emergency_threshold = 0.95;
    return c;
}

/* ── Guardian ─────────────────────────────────────────────────── */

void guardian_init(Guardian *g, DetectionConfig config) {
    memset(g, 0, sizeof(*g));
    g->config = config;
}

void guardian_add_profile(Guardian *g, ResourceType type, double max_value,
                          double warning_threshold, double critical_threshold) {
    if (g->profile_count >= CG_MAX_PROFILES) return;
    profile_init(&g->profiles[g->profile_count], type, max_value,
                 warning_threshold, critical_threshold);
    g->profile_count++;
}

void guardian_record(Guardian *g, ResourceType type, double value) {
    for (size_t i = 0; i < g->profile_count; i++) {
        if (g->profiles[i].type == type) {
            profile_record(&g->profiles[i], value);
            return;
        }
    }
}

Phase guardian_phase_for(const Guardian *g, const ResourceProfile *p) {
    (void)g;
    double usage = budget_usage(&p->budget);
    if (usage >= g->config.emergency_threshold) return PHASE_EMERGENCY;
    if (usage >= p->budget.critical_threshold)  return PHASE_CRITICAL;
    if (usage >= p->budget.warning_threshold)   return PHASE_WARNING;
    return PHASE_NORMAL;
}

Report guardian_detect(Guardian *g) {
    Report r;
    report_init(&r);
    r.phase = PHASE_NORMAL;

    for (size_t i = 0; i < g->profile_count; i++) {
        ResourceProfile *p = &g->profiles[i];
        Phase p_phase = guardian_phase_for(g, p);
        if (p_phase > r.phase) r.phase = p_phase;

        if (p_phase >= PHASE_WARNING) {
            report_add_violation(&r, p->type);
        }

        double steps = profile_predict_exhaustion(p);
        if (steps >= 0.0 && steps < 100.0) {
            report_add_prediction(&r, steps);
        }
    }

    /* recommendations based on phase */
    switch (r.phase) {
        case PHASE_EMERGENCY:
            report_add_recommendation(&r, "Immediately reduce resource consumption");
            report_add_recommendation(&r, "Shed non-critical workloads");
            /* fallthrough */
        case PHASE_CRITICAL:
            report_add_recommendation(&r, "Throttle resource-intensive operations");
            report_add_recommendation(&r, "Enable conservation mode");
            /* fallthrough */
        case PHASE_WARNING:
            report_add_recommendation(&r, "Monitor resource usage closely");
            break;
        case PHASE_NORMAL:
            report_add_recommendation(&r, "Resources within normal limits");
            break;
    }

    return r;
}
