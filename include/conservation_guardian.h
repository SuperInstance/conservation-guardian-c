#ifndef CONSERVATION_GUARDIAN_H
#define CONSERVATION_GUARDIAN_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Budget ──────────────────────────────────────────────────── */

typedef struct {
    double max_value;
    double current_value;
    double warning_threshold;   /* fraction 0.0–1.0 */
    double critical_threshold;  /* fraction 0.0–1.0 */
} Budget;

Budget budget_create(double max_value, double warning_threshold, double critical_threshold);
double budget_remaining(const Budget *b);
double budget_usage(const Budget *b);
bool budget_in_warning(const Budget *b);
bool budget_in_critical(const Budget *b);

/* ── ResourceType ────────────────────────────────────────────── */

typedef enum {
    RESOURCE_CPU = 0,
    RESOURCE_MEMORY,
    RESOURCE_DISK,
    RESOURCE_BANDWIDTH,
    RESOURCE_TIME,
    RESOURCE_TYPE_COUNT
} ResourceType;

const char *resource_type_name(ResourceType t);

/* ── Phase ───────────────────────────────────────────────────── */

typedef enum {
    PHASE_NORMAL = 0,
    PHASE_WARNING,
    PHASE_CRITICAL,
    PHASE_EMERGENCY
} Phase;

const char *phase_name(Phase p);

/* ── AlertLevel ──────────────────────────────────────────────── */

typedef enum {
    ALERT_INFO = 0,
    ALERT_WARNING,
    ALERT_CRITICAL,
    ALERT_EMERGENCY
} AlertLevel;

const char *alert_level_name(AlertLevel a);

/* ── ResourceProfile ─────────────────────────────────────────── */

#define CG_HISTORY_SIZE 64

typedef enum {
    TREND_STABLE = 0,
    TREND_INCREASING,
    TREND_DECREASING
} Trend;

typedef struct {
    ResourceType type;
    Budget budget;
    double usage_history[CG_HISTORY_SIZE];
    size_t history_count;
    Trend trend;
    double ema;                 /* exponential moving average */
} ResourceProfile;

void profile_init(ResourceProfile *p, ResourceType type, double max_value,
                  double warning_threshold, double critical_threshold);
void profile_record(ResourceProfile *p, double value);
Trend profile_compute_trend(const ResourceProfile *p);
double profile_predict_exhaustion(const ResourceProfile *p);

/* ── Report ──────────────────────────────────────────────────── */

#define CG_MAX_VIOLATIONS   8
#define CG_MAX_PREDICTIONS  8
#define CG_MAX_RECOMMENDATIONS 8

typedef struct {
    Phase phase;
    ResourceType violations[CG_MAX_VIOLATIONS];
    size_t violation_count;
    double predictions[CG_MAX_PREDICTIONS];  /* predicted steps to exhaustion */
    size_t prediction_count;
    const char *recommendations[CG_MAX_RECOMMENDATIONS];
    size_t recommendation_count;
} Report;

void report_init(Report *r);
void report_add_violation(Report *r, ResourceType t);
void report_add_prediction(Report *r, double steps);
void report_add_recommendation(Report *r, const char *rec);

/* ── DetectionConfig ─────────────────────────────────────────── */

typedef struct {
    double ema_alpha;           /* smoothing factor 0–1 */
    double trend_slope_threshold;
    double emergency_threshold; /* fraction 0–1 */
} DetectionConfig;

DetectionConfig detection_config_default(void);

/* ── Guardian ────────────────────────────────────────────────── */

#define CG_MAX_PROFILES 8

typedef struct {
    ResourceProfile profiles[CG_MAX_PROFILES];
    size_t profile_count;
    DetectionConfig config;
} Guardian;

void guardian_init(Guardian *g, DetectionConfig config);
void guardian_add_profile(Guardian *g, ResourceType type, double max_value,
                          double warning_threshold, double critical_threshold);
void guardian_record(Guardian *g, ResourceType type, double value);
Report guardian_detect(Guardian *g);
Phase guardian_phase_for(const Guardian *g, const ResourceProfile *p);

/* ── Utility ─────────────────────────────────────────────────── */

double cg_min(double a, double b);
double cg_max(double a, double b);

#ifdef __cplusplus
}
#endif

#endif /* CONSERVATION_GUARDIAN_H */
