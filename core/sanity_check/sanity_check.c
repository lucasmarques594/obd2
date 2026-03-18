#include "sanity_check.h"
#include <math.h>

static const char* const result_strings[] = {
    [SANITY_RESULT_OK] = "OK",
    [SANITY_RESULT_OUT_OF_RANGE] = "Out of Range",
    [SANITY_RESULT_SENSOR_STUCK] = "Sensor Stuck",
    [SANITY_RESULT_INVALID_DATA] = "Invalid Data",
    [SANITY_RESULT_RATE_OF_CHANGE] = "Rate of Change Exceeded"
};

/*
 * max_rate_per_sec: maximum allowed delta per second.
 * Example: RPM max 8000/s means the engine can't jump more than 8000 RPM in 1 second.
 * At 100ms sample rate, that's 800 RPM per sample — much more realistic than the old
 * absolute threshold of 2000 which was sample-rate-dependent.
 */
static const SanityRule_t sanity_rules[] = {
    {0x04, 0.0f, 100.0f, 200.0f, true, true, true},         /* Engine load %/s */
    {0x05, -40.0f, 215.0f, 20.0f, true, true, true},         /* Coolant temp C/s */
    {0x06, -100.0f, 99.2f, 80.0f, false, true, false},       /* STFT %/s */
    {0x07, -100.0f, 99.2f, 40.0f, false, true, false},       /* LTFT %/s */
    {0x0B, 0.0f, 255.0f, 200.0f, true, true, true},          /* MAP kPa/s */
    {0x0C, 0.0f, 16383.75f, 8000.0f, true, true, true},      /* RPM /s */
    {0x0D, 0.0f, 255.0f, 100.0f, true, true, true},          /* Speed km/h/s */
    {0x0E, -64.0f, 63.5f, 80.0f, false, true, true},         /* Timing advance deg/s */
    {0x0F, -40.0f, 215.0f, 10.0f, true, true, true},         /* IAT C/s */
    {0x10, 0.0f, 655.35f, 400.0f, true, true, true},         /* MAF g/s/s */
    {0x11, 0.0f, 100.0f, 200.0f, true, true, true},          /* Throttle %/s */
    {0x2F, 0.0f, 100.0f, 10.0f, true, true, true},           /* Fuel level %/s */
    {0x33, 70.0f, 110.0f, 5.0f, true, true, true},           /* Baro kPa/s */
    {0x42, 0.0f, 65.535f, 20.0f, true, true, true},          /* Ctrl voltage V/s */
    {0x46, -40.0f, 215.0f, 5.0f, true, true, true},          /* Ambient temp C/s */
    {0x5C, -40.0f, 210.0f, 10.0f, true, true, true},         /* Oil temp C/s */
    {0x5E, 0.0f, 3276.75f, 200.0f, true, true, true},        /* Fuel rate L/h/s */
    {0xFF, 0.0f, 0.0f, 0.0f, false, false, false}
};

#define SANITY_RULES_COUNT (sizeof(sanity_rules) / sizeof(sanity_rules[0]) - 1)

static const SanityRule_t* find_rule(u8 pid)
{
    for (u32 i = 0U; i < SANITY_RULES_COUNT; i++) {
        if (sanity_rules[i].pid == pid) {
            return &sanity_rules[i];
        }
    }
    return NULL_PTR;
}

static SanityHistory_t* find_or_create_history(SanityCheck_t* sc, u8 pid)
{
    for (u8 i = 0U; i < sc->history_count; i++) {
        if (sc->history[i].pid == pid) {
            return &sc->history[i];
        }
    }

    if (sc->history_count < 64U) {
        SanityHistory_t* hist = &sc->history[sc->history_count];
        hist->pid = pid;
        hist->history_idx = 0U;
        hist->history_count = 0U;
        hist->stuck_count = 0U;
        hist->last_check_ms = 0U;
        hist->valid = true;
        sc->history_count++;
        return hist;
    }

    return NULL_PTR;
}

static void add_to_history(SanityHistory_t* hist, float value)
{
    hist->history[hist->history_idx] = value;
    hist->history_idx = (hist->history_idx + 1U) % SANITY_HISTORY_SIZE;

    if (hist->history_count < SANITY_HISTORY_SIZE) {
        hist->history_count++;
    }
}

static float get_previous_value(const SanityHistory_t* hist)
{
    if (hist->history_count == 0U) {
        return 0.0f;
    }

    u8 prev_idx;
    if (hist->history_idx == 0U) {
        prev_idx = SANITY_HISTORY_SIZE - 1U;
    } else {
        prev_idx = hist->history_idx - 1U;
    }

    return hist->history[prev_idx];
}

Result_t SanityCheck_Init(SanityCheck_t* sc, const SanityCheckConfig_t* config)
{
    if (sc == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    sc->history_count = 0U;
    sc->total_checks = 0U;
    sc->total_failures = 0U;

    for (u8 i = 0U; i < 64U; i++) {
        sc->history[i].valid = false;
    }

    sc->error_handler = config->error_handler;
    sc->fail_callback = config->fail_callback;
    sc->callback_context = config->callback_context;
    sc->get_timestamp_ms = config->get_timestamp_ms;
    sc->initialized = true;

    return RESULT_OK;
}

SanityResult_t SanityCheck_ValidateRange(u8 pid, float value)
{
    const SanityRule_t* rule = find_rule(pid);

    if (rule == NULL_PTR) {
        return SANITY_RESULT_OK;
    }

    if (rule->check_range == false) {
        return SANITY_RESULT_OK;
    }

    if ((value < rule->min_value) || (value > rule->max_value)) {
        return SANITY_RESULT_OUT_OF_RANGE;
    }

    return SANITY_RESULT_OK;
}

SanityResult_t SanityCheck_ValidateStuck(SanityCheck_t* sc, u8 pid, float value)
{
    if (sc == NULL_PTR) {
        return SANITY_RESULT_OK;
    }

    if (sc->initialized == false) {
        return SANITY_RESULT_OK;
    }

    const SanityRule_t* rule = find_rule(pid);

    if (rule == NULL_PTR) {
        return SANITY_RESULT_OK;
    }

    if (rule->check_stuck == false) {
        return SANITY_RESULT_OK;
    }

    SanityHistory_t* hist = find_or_create_history(sc, pid);

    if (hist == NULL_PTR) {
        return SANITY_RESULT_OK;
    }

    if (hist->history_count > 0U) {
        float prev = get_previous_value(hist);
        float diff = value - prev;

        if (diff < 0.0f) {
            diff = -diff;
        }

        if (diff < 0.001f) {
            hist->stuck_count++;

            if (hist->stuck_count >= SANITY_STUCK_THRESHOLD) {
                return SANITY_RESULT_SENSOR_STUCK;
            }
        } else {
            hist->stuck_count = 0U;
        }
    }

    return SANITY_RESULT_OK;
}

SanityResult_t SanityCheck_ValidateRateOfChange(SanityCheck_t* sc, u8 pid, float value)
{
    if (sc == NULL_PTR) {
        return SANITY_RESULT_OK;
    }

    if (sc->initialized == false) {
        return SANITY_RESULT_OK;
    }

    const SanityRule_t* rule = find_rule(pid);

    if (rule == NULL_PTR) {
        return SANITY_RESULT_OK;
    }

    if (rule->check_rate == false) {
        return SANITY_RESULT_OK;
    }

    SanityHistory_t* hist = find_or_create_history(sc, pid);

    if (hist == NULL_PTR) {
        return SANITY_RESULT_OK;
    }

    if (hist->history_count > 0U) {
        float prev = get_previous_value(hist);
        float diff = value - prev;

        if (diff < 0.0f) {
            diff = -diff;
        }

        float max_allowed = rule->max_rate_per_sec;

        if (sc->get_timestamp_ms != NULL_PTR && hist->last_check_ms > 0U) {
            u32 now = sc->get_timestamp_ms();
            u32 delta_ms;

            if (now >= hist->last_check_ms) {
                delta_ms = now - hist->last_check_ms;
            } else {
                delta_ms = (0xFFFFFFFFU - hist->last_check_ms) + now + 1U;
            }

            if (delta_ms > 0U) {
                float delta_sec = (float)delta_ms / 1000.0f;
                max_allowed = rule->max_rate_per_sec * delta_sec;

                if (max_allowed < 0.001f) {
                    max_allowed = 0.001f;
                }
            }
        }

        if (diff > max_allowed) {
            return SANITY_RESULT_RATE_OF_CHANGE;
        }
    }

    return SANITY_RESULT_OK;
}

SanityResult_t SanityCheck_ValidatePid(SanityCheck_t* sc, u8 pid, const PidValue_t* value)
{
    if (sc == NULL_PTR) {
        return SANITY_RESULT_OK;
    }

    if (value == NULL_PTR) {
        return SANITY_RESULT_INVALID_DATA;
    }

    if (sc->initialized == false) {
        return SANITY_RESULT_OK;
    }

    if (value->valid == false) {
        return SANITY_RESULT_INVALID_DATA;
    }

    sc->total_checks++;

    SanityResult_t result;

    result = SanityCheck_ValidateRange(pid, value->eng_value);
    if (result != SANITY_RESULT_OK) {
        sc->total_failures++;
        if (sc->fail_callback != NULL_PTR) {
            sc->fail_callback(pid, result, value->eng_value, sc->callback_context);
        }
        if (sc->error_handler != NULL_PTR) {
            ERROR_REPORT(sc->error_handler, ERR_SANITY_OUT_OF_RANGE, ERR_SEV_WARNING);
        }
        return result;
    }

    result = SanityCheck_ValidateStuck(sc, pid, value->eng_value);
    if (result != SANITY_RESULT_OK) {
        sc->total_failures++;
        if (sc->fail_callback != NULL_PTR) {
            sc->fail_callback(pid, result, value->eng_value, sc->callback_context);
        }
        if (sc->error_handler != NULL_PTR) {
            ERROR_REPORT(sc->error_handler, ERR_SANITY_SENSOR_STUCK, ERR_SEV_WARNING);
        }
        return result;
    }

    result = SanityCheck_ValidateRateOfChange(sc, pid, value->eng_value);
    if (result != SANITY_RESULT_OK) {
        sc->total_failures++;
        if (sc->fail_callback != NULL_PTR) {
            sc->fail_callback(pid, result, value->eng_value, sc->callback_context);
        }
        return result;
    }

    SanityHistory_t* hist = find_or_create_history(sc, pid);
    if (hist != NULL_PTR) {
        add_to_history(hist, value->eng_value);

        if (sc->get_timestamp_ms != NULL_PTR) {
            hist->last_check_ms = sc->get_timestamp_ms();
        }
    }

    return SANITY_RESULT_OK;
}

Result_t SanityCheck_ClearHistory(SanityCheck_t* sc, u8 pid)
{
    if (sc == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    if (sc->initialized == false) {
        return RESULT_NOT_READY;
    }

    for (u8 i = 0U; i < sc->history_count; i++) {
        if (sc->history[i].pid == pid) {
            sc->history[i].history_idx = 0U;
            sc->history[i].history_count = 0U;
            sc->history[i].stuck_count = 0U;
            return RESULT_OK;
        }
    }

    return RESULT_OK;
}

Result_t SanityCheck_ClearAllHistory(SanityCheck_t* sc)
{
    if (sc == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    if (sc->initialized == false) {
        return RESULT_NOT_READY;
    }

    for (u8 i = 0U; i < sc->history_count; i++) {
        sc->history[i].history_idx = 0U;
        sc->history[i].history_count = 0U;
        sc->history[i].stuck_count = 0U;
    }

    return RESULT_OK;
}

const SanityRule_t* SanityCheck_GetRule(u8 pid)
{
    return find_rule(pid);
}

bool SanityCheck_IsConfigured(u8 pid)
{
    return (find_rule(pid) != NULL_PTR);
}

const char* SanityCheck_GetResultString(SanityResult_t result)
{
    if (result >= SANITY_RESULT_MAX) {
        return "Unknown";
    }

    return result_strings[result];
}

u32 SanityCheck_GetTotalChecks(const SanityCheck_t* sc)
{
    if (sc == NULL_PTR) {
        return 0U;
    }

    if (sc->initialized == false) {
        return 0U;
    }

    return sc->total_checks;
}

u32 SanityCheck_GetTotalFailures(const SanityCheck_t* sc)
{
    if (sc == NULL_PTR) {
        return 0U;
    }

    if (sc->initialized == false) {
        return 0U;
    }

    return sc->total_failures;
}
