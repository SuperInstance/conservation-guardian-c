#include "conservation_guardian.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  TEST %-50s ", #name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("[PASS]\n"); \
} while(0)

#define FAIL(msg) do { \
    printf("[FAIL] %s\n", msg); \
} while(0)

#define ASSERT_DBL_EQ(a, b, eps) do { \
    if (fabs((a) - (b)) > (eps)) { \
        FAIL(#a " != " #b); \
        return; \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        FAIL(#cond " is false"); \
        return; \
    } \
} while(0)

#define ASSERT_FALSE(cond) do { \
    if ((cond)) { \
        FAIL(#cond " is true"); \
        return; \
    } \
} while(0)

/* ── Budget Tests ────────────────────────────────────────────── */

static void test_budget_create(void) {
    TEST(test_budget_create);
    Budget b = budget_create(100.0, 0.7, 0.9);
    ASSERT_DBL_EQ(b.max_value, 100.0, 1e-9);
    ASSERT_DBL_EQ(b.current_value, 0.0, 1e-9);
    ASSERT_DBL_EQ(b.warning_threshold, 0.7, 1e-9);
    ASSERT_DBL_EQ(b.critical_threshold, 0.9, 1e-9);
    PASS();
}

static void test_budget_remaining(void) {
    TEST(test_budget_remaining);
    Budget b = budget_create(100.0, 0.7, 0.9);
    b.current_value = 65.0;
    ASSERT_DBL_EQ(budget_remaining(&b), 35.0, 1e-9);
    PASS();
}

static void test_budget_remaining_zero(void) {
    TEST(test_budget_remaining_zero);
    Budget b = budget_create(100.0, 0.7, 0.9);
    b.current_value = 150.0;
    ASSERT_DBL_EQ(budget_remaining(&b), 0.0, 1e-9);
    PASS();
}

static void test_budget_usage(void) {
    TEST(test_budget_usage);
    Budget b = budget_create(200.0, 0.7, 0.9);
    b.current_value = 100.0;
    ASSERT_DBL_EQ(budget_usage(&b), 0.5, 1e-9);
    PASS();
}

static void test_budget_usage_capped(void) {
    TEST(test_budget_usage_capped);
    Budget b = budget_create(50.0, 0.7, 0.9);
    b.current_value = 100.0;
    ASSERT_DBL_EQ(budget_usage(&b), 1.0, 1e-9);
    PASS();
}

static void test_budget_warning(void) {
    TEST(test_budget_warning);
    Budget b = budget_create(100.0, 0.7, 0.9);
    b.current_value = 70.0;
    ASSERT_TRUE(budget_in_warning(&b));
    PASS();
}

static void test_budget_not_warning(void) {
    TEST(test_budget_not_warning);
    Budget b = budget_create(100.0, 0.7, 0.9);
    b.current_value = 50.0;
    ASSERT_FALSE(budget_in_warning(&b));
    PASS();
}

static void test_budget_critical(void) {
    TEST(test_budget_critical);
    Budget b = budget_create(100.0, 0.7, 0.9);
    b.current_value = 92.0;
    ASSERT_TRUE(budget_in_critical(&b));
    PASS();
}

/* ── ResourceType Tests ──────────────────────────────────────── */

static void test_resource_type_names(void) {
    TEST(test_resource_type_names);
    ASSERT_TRUE(strcmp(resource_type_name(RESOURCE_CPU), "CPU") == 0);
    ASSERT_TRUE(strcmp(resource_type_name(RESOURCE_MEMORY), "Memory") == 0);
    ASSERT_TRUE(strcmp(resource_type_name(RESOURCE_DISK), "Disk") == 0);
    ASSERT_TRUE(strcmp(resource_type_name(RESOURCE_BANDWIDTH), "Bandwidth") == 0);
    ASSERT_TRUE(strcmp(resource_type_name(RESOURCE_TIME), "Time") == 0);
    PASS();
}

static void test_phase_names(void) {
    TEST(test_phase_names);
    ASSERT_TRUE(strcmp(phase_name(PHASE_NORMAL), "Normal") == 0);
    ASSERT_TRUE(strcmp(phase_name(PHASE_WARNING), "Warning") == 0);
    ASSERT_TRUE(strcmp(phase_name(PHASE_CRITICAL), "Critical") == 0);
    ASSERT_TRUE(strcmp(phase_name(PHASE_EMERGENCY), "Emergency") == 0);
    PASS();
}

static void test_alert_level_names(void) {
    TEST(test_alert_level_names);
    ASSERT_TRUE(strcmp(alert_level_name(ALERT_INFO), "Info") == 0);
    ASSERT_TRUE(strcmp(alert_level_name(ALERT_WARNING), "Warning") == 0);
    ASSERT_TRUE(strcmp(alert_level_name(ALERT_CRITICAL), "Critical") == 0);
    ASSERT_TRUE(strcmp(alert_level_name(ALERT_EMERGENCY), "Emergency") == 0);
    PASS();
}

/* ── Profile Tests ───────────────────────────────────────────── */

static void test_profile_init(void) {
    TEST(test_profile_init);
    ResourceProfile p;
    profile_init(&p, RESOURCE_CPU, 100.0, 0.7, 0.9);
    ASSERT_TRUE(p.type == RESOURCE_CPU);
    ASSERT_DBL_EQ(p.budget.max_value, 100.0, 1e-9);
    ASSERT_TRUE(p.history_count == 0);
    ASSERT_TRUE(p.trend == TREND_STABLE);
    PASS();
}

static void test_profile_record_updates_budget(void) {
    TEST(test_profile_record_updates_budget);
    ResourceProfile p;
    profile_init(&p, RESOURCE_MEMORY, 1000.0, 0.7, 0.9);
    profile_record(&p, 500.0);
    ASSERT_DBL_EQ(p.budget.current_value, 500.0, 1e-9);
    ASSERT_TRUE(p.history_count == 1);
    PASS();
}

static void test_profile_ema_first(void) {
    TEST(test_profile_ema_first);
    ResourceProfile p;
    profile_init(&p, RESOURCE_CPU, 100.0, 0.7, 0.9);
    profile_record(&p, 50.0);
    ASSERT_DBL_EQ(p.ema, 50.0, 1e-9);
    PASS();
}

static void test_profile_ema_subsequent(void) {
    TEST(test_profile_ema_subsequent);
    ResourceProfile p;
    profile_init(&p, RESOURCE_CPU, 100.0, 0.7, 0.9);
    profile_record(&p, 50.0);
    profile_record(&p, 60.0);
    /* ema = 0.3*60 + 0.7*50 = 18 + 35 = 53 */
    ASSERT_DBL_EQ(p.ema, 53.0, 1e-9);
    PASS();
}

static void test_profile_stable_trend(void) {
    TEST(test_profile_stable_trend);
    ResourceProfile p;
    profile_init(&p, RESOURCE_CPU, 100.0, 0.7, 0.9);
    for (int i = 0; i < 10; i++) profile_record(&p, 50.0);
    ASSERT_TRUE(p.trend == TREND_STABLE);
    PASS();
}

static void test_profile_increasing_trend(void) {
    TEST(test_profile_increasing_trend);
    ResourceProfile p;
    profile_init(&p, RESOURCE_CPU, 100.0, 0.7, 0.9);
    for (int i = 0; i < 10; i++) profile_record(&p, (double)(10 + i * 5));
    ASSERT_TRUE(p.trend == TREND_INCREASING);
    PASS();
}

static void test_profile_decreasing_trend(void) {
    TEST(test_profile_decreasing_trend);
    ResourceProfile p;
    profile_init(&p, RESOURCE_CPU, 100.0, 0.7, 0.9);
    for (int i = 0; i < 10; i++) profile_record(&p, (double)(90 - i * 5));
    ASSERT_TRUE(p.trend == TREND_DECREASING);
    PASS();
}

static void test_profile_ring_buffer(void) {
    TEST(test_profile_ring_buffer);
    ResourceProfile p;
    profile_init(&p, RESOURCE_CPU, 100.0, 0.7, 0.9);
    for (int i = 0; i < CG_HISTORY_SIZE + 10; i++) profile_record(&p, (double)i);
    ASSERT_TRUE(p.history_count == CG_HISTORY_SIZE);
    /* oldest should be 10 (first 10 rolled off) */
    ASSERT_DBL_EQ(p.usage_history[0], 10.0, 1e-9);
    PASS();
}

static void test_predict_exhaustion_increasing(void) {
    TEST(test_predict_exhaustion_increasing);
    ResourceProfile p;
    profile_init(&p, RESOURCE_CPU, 100.0, 0.7, 0.9);
    /* linear: 10, 20, 30, 40, 50 → slope=10, intercept=0 */
    for (int i = 1; i <= 5; i++) profile_record(&p, (double)(i * 10));
    double steps = profile_predict_exhaustion(&p);
    ASSERT_TRUE(steps >= 0.0);
    /* next value 60, then 70, 80, 90, 100 → ~5 more steps from last point */
    ASSERT_TRUE(steps < 10.0);
    PASS();
}

static void test_predict_exhaustion_stable(void) {
    TEST(test_predict_exhaustion_stable);
    ResourceProfile p;
    profile_init(&p, RESOURCE_CPU, 100.0, 0.7, 0.9);
    for (int i = 0; i < 10; i++) profile_record(&p, 50.0);
    double steps = profile_predict_exhaustion(&p);
    ASSERT_TRUE(steps < 0.0); /* stable → -1 */
    PASS();
}

static void test_predict_exhaustion_single_point(void) {
    TEST(test_predict_exhaustion_single_point);
    ResourceProfile p;
    profile_init(&p, RESOURCE_CPU, 100.0, 0.7, 0.9);
    profile_record(&p, 50.0);
    double steps = profile_predict_exhaustion(&p);
    ASSERT_TRUE(steps < 0.0);
    PASS();
}

/* ── Report Tests ────────────────────────────────────────────── */

static void test_report_init(void) {
    TEST(test_report_init);
    Report r;
    report_init(&r);
    ASSERT_TRUE(r.phase == PHASE_NORMAL);
    ASSERT_TRUE(r.violation_count == 0);
    ASSERT_TRUE(r.prediction_count == 0);
    ASSERT_TRUE(r.recommendation_count == 0);
    PASS();
}

static void test_report_add_violation(void) {
    TEST(test_report_add_violation);
    Report r;
    report_init(&r);
    report_add_violation(&r, RESOURCE_CPU);
    report_add_violation(&r, RESOURCE_MEMORY);
    ASSERT_TRUE(r.violation_count == 2);
    ASSERT_TRUE(r.violations[0] == RESOURCE_CPU);
    ASSERT_TRUE(r.violations[1] == RESOURCE_MEMORY);
    PASS();
}

static void test_report_overflow(void) {
    TEST(test_report_overflow);
    Report r;
    report_init(&r);
    for (int i = 0; i < CG_MAX_VIOLATIONS + 5; i++) {
        report_add_violation(&r, RESOURCE_CPU);
    }
    ASSERT_TRUE(r.violation_count == CG_MAX_VIOLATIONS);
    PASS();
}

/* ── Guardian Tests ──────────────────────────────────────────── */

static void test_guardian_init(void) {
    TEST(test_guardian_init);
    Guardian g;
    guardian_init(&g, detection_config_default());
    ASSERT_TRUE(g.profile_count == 0);
    PASS();
}

static void test_guardian_add_profiles(void) {
    TEST(test_guardian_add_profiles);
    Guardian g;
    guardian_init(&g, detection_config_default());
    guardian_add_profile(&g, RESOURCE_CPU, 100.0, 0.7, 0.9);
    guardian_add_profile(&g, RESOURCE_MEMORY, 1024.0, 0.8, 0.95);
    ASSERT_TRUE(g.profile_count == 2);
    ASSERT_TRUE(g.profiles[0].type == RESOURCE_CPU);
    ASSERT_TRUE(g.profiles[1].type == RESOURCE_MEMORY);
    PASS();
}

static void test_guardian_detect_normal(void) {
    TEST(test_guardian_detect_normal);
    Guardian g;
    guardian_init(&g, detection_config_default());
    guardian_add_profile(&g, RESOURCE_CPU, 100.0, 0.7, 0.9);
    profile_record(&g.profiles[0], 50.0);
    Report r = guardian_detect(&g);
    ASSERT_TRUE(r.phase == PHASE_NORMAL);
    ASSERT_TRUE(r.violation_count == 0);
    PASS();
}

static void test_guardian_detect_warning(void) {
    TEST(test_guardian_detect_warning);
    Guardian g;
    guardian_init(&g, detection_config_default());
    guardian_add_profile(&g, RESOURCE_CPU, 100.0, 0.7, 0.9);
    profile_record(&g.profiles[0], 75.0);
    Report r = guardian_detect(&g);
    ASSERT_TRUE(r.phase == PHASE_WARNING);
    ASSERT_TRUE(r.violation_count == 1);
    PASS();
}

static void test_guardian_detect_critical(void) {
    TEST(test_guardian_detect_critical);
    Guardian g;
    guardian_init(&g, detection_config_default());
    guardian_add_profile(&g, RESOURCE_MEMORY, 100.0, 0.7, 0.9);
    profile_record(&g.profiles[0], 91.0);
    Report r = guardian_detect(&g);
    ASSERT_TRUE(r.phase == PHASE_CRITICAL);
    PASS();
}

static void test_guardian_detect_emergency(void) {
    TEST(test_guardian_detect_emergency);
    Guardian g;
    guardian_init(&g, detection_config_default());
    guardian_add_profile(&g, RESOURCE_DISK, 100.0, 0.7, 0.9);
    profile_record(&g.profiles[0], 96.0);
    Report r = guardian_detect(&g);
    ASSERT_TRUE(r.phase == PHASE_EMERGENCY);
    PASS();
}

static void test_guardian_record(void) {
    TEST(test_guardian_record);
    Guardian g;
    guardian_init(&g, detection_config_default());
    guardian_add_profile(&g, RESOURCE_CPU, 100.0, 0.7, 0.9);
    guardian_record(&g, RESOURCE_CPU, 80.0);
    ASSERT_DBL_EQ(g.profiles[0].budget.current_value, 80.0, 1e-9);
    PASS();
}

static void test_guardian_multi_resource(void) {
    TEST(test_guardian_multi_resource);
    Guardian g;
    guardian_init(&g, detection_config_default());
    guardian_add_profile(&g, RESOURCE_CPU, 100.0, 0.7, 0.9);
    guardian_add_profile(&g, RESOURCE_MEMORY, 1024.0, 0.8, 0.95);
    guardian_record(&g, RESOURCE_CPU, 30.0);
    guardian_record(&g, RESOURCE_MEMORY, 980.0);  /* ~95.7% → emergency */
    Report r = guardian_detect(&g);
    ASSERT_TRUE(r.phase == PHASE_EMERGENCY);
    ASSERT_TRUE(r.violation_count >= 1);
    PASS();
}

/* ── DetectionConfig Tests ───────────────────────────────────── */

static void test_detection_config_defaults(void) {
    TEST(test_detection_config_defaults);
    DetectionConfig c = detection_config_default();
    ASSERT_DBL_EQ(c.ema_alpha, 0.3, 1e-9);
    ASSERT_DBL_EQ(c.trend_slope_threshold, 0.01, 1e-9);
    ASSERT_DBL_EQ(c.emergency_threshold, 0.95, 1e-9);
    PASS();
}

/* ── Utility Tests ───────────────────────────────────────────── */

static void test_cg_min(void) {
    TEST(test_cg_min);
    ASSERT_DBL_EQ(cg_min(3.0, 5.0), 3.0, 1e-9);
    ASSERT_DBL_EQ(cg_min(5.0, 3.0), 3.0, 1e-9);
    PASS();
}

static void test_cg_max(void) {
    TEST(test_cg_max);
    ASSERT_DBL_EQ(cg_max(3.0, 5.0), 5.0, 1e-9);
    ASSERT_DBL_EQ(cg_max(5.0, 3.0), 5.0, 1e-9);
    PASS();
}

/* ── Main ────────────────────────────────────────────────────── */

int main(void) {
    printf("\n═══ Conservation Guardian Test Suite ═══\n\n");

    test_budget_create();
    test_budget_remaining();
    test_budget_remaining_zero();
    test_budget_usage();
    test_budget_usage_capped();
    test_budget_warning();
    test_budget_not_warning();
    test_budget_critical();
    test_resource_type_names();
    test_phase_names();
    test_alert_level_names();
    test_profile_init();
    test_profile_record_updates_budget();
    test_profile_ema_first();
    test_profile_ema_subsequent();
    test_profile_stable_trend();
    test_profile_increasing_trend();
    test_profile_decreasing_trend();
    test_profile_ring_buffer();
    test_predict_exhaustion_increasing();
    test_predict_exhaustion_stable();
    test_predict_exhaustion_single_point();
    test_report_init();
    test_report_add_violation();
    test_report_overflow();
    test_guardian_init();
    test_guardian_add_profiles();
    test_guardian_detect_normal();
    test_guardian_detect_warning();
    test_guardian_detect_critical();
    test_guardian_detect_emergency();
    test_guardian_record();
    test_guardian_multi_resource();
    test_detection_config_defaults();
    test_cg_min();
    test_cg_max();

    printf("\n═══ Results: %d/%d passed ═══\n\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}
