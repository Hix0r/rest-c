#pragma once
// ============================================================================
// 20 автоматических проверок варианта 5.
// Используются: GET /api/v1.1/admin/run-tests-individual и tests/variant_tests.cpp.
// Не обращаются к БД и сети.
// ============================================================================

#include <string>
#include <vector>
#include <utility>
#include <cmath>
#include "variant_mine.hpp"

namespace VariantTests {

    inline std::vector<std::pair<std::string, bool>> run() {
        using namespace TaskVariant;

        auto body = [](const std::string& a, const std::string& c,
                       const std::string& r, const std::string& w) {
            return "{\"active_users_start\":" + a + ",\"churned_users\":" + c +
                   ",\"arpu\":" + r + ",\"winback_cost_per_user\":" + w + "}";
        };
        auto err   = [](const std::string& b) { return validate(parse(b)); };
        auto valid = [&](const std::string& b) { return err(b).empty(); };
        auto approx = [](double a, double b) { return std::fabs(a - b) < 1e-9; };
        const std::string sample = body("5000", "250", "30.0", "15.0");
        auto ref = calculate(parse(sample));
        auto zero = calculate(parse(body("10", "0", "5", "5")));

        return {
            // --- корректные данные ---
            {"Control example: churn=5, leakage=7500, recovery=3750",
                valid(sample) && approx(ref.churn_rate_percentage, 5.0) &&
                approx(ref.monthly_revenue_leakage, 7500.0) && approx(ref.total_recovery_budget, 3750.0)},
            {"Fractional churn rate: 10 of 30 -> 33.33%",
                approx(calculate(parse(body("30", "10", "1", "1"))).churn_rate_percentage, 100.0 / 3)},
            {"Large values are printed without exponent",
                to_json(calculate(parse(body("100000000", "50000000", "1000", "1")))).find("e+") == std::string::npos},
            // --- граничные значения ---
            {"Boundary: active_users_start = 10", valid(body("10", "0", "1", "1"))},
            {"Boundary: churned_users = active - 1", valid(body("10", "9", "1", "1"))},
            {"Boundary: churned_users = 0 gives zeros",
                zero.churn_rate_percentage == 0.0 && zero.monthly_revenue_leakage == 0.0 && zero.total_recovery_budget == 0.0},
            {"Boundary: arpu and winback = 0.01", valid(body("10", "1", "0.01", "0.01"))},
            // --- неверные данные ---
            {"Invalid: active_users_start = 9", !valid(body("9", "1", "1", "1"))},
            {"Invalid: churned_users = active_users_start", !valid(body("100", "100", "1", "1"))},
            {"Invalid: churned_users < 0", !valid(body("100", "-1", "1", "1"))},
            {"Invalid: arpu = 0", !valid(body("100", "5", "0", "1"))},
            {"Invalid: arpu < 0", !valid(body("100", "5", "-2", "1"))},
            {"Invalid: winback_cost_per_user = 0", !valid(body("100", "5", "1", "0"))},
            {"Invalid: winback_cost_per_user < 0", !valid(body("100", "5", "1", "-3"))},
            // --- неполные и некорректные данные ---
            {"Incomplete: missing arpu",
                err("{\"active_users_start\":100,\"churned_users\":5,\"winback_cost_per_user\":2}") == "arpu is required"},
            {"Incomplete: empty body and empty object", !valid("") && !valid("{}")},
            {"Wrong type: string instead of number, fractional int",
                !valid(body("\"abc\"", "5", "1", "1")) && !valid(body("10.5", "1", "1", "1"))},
            {"Numbers: exponent 1e2 accepted, 1e999 rejected",
                valid(body("1e2", "5", "1", "1")) && !valid(body("100", "5", "1e999", "1"))},
            {"Strict JSON: unknown field rejected",
                !valid("{\"active_users_start\":100,\"churned_users\":5,\"arpu\":1,\"winback_cost_per_user\":1,\"x\":1}")},
            {"Strict JSON: duplicate key and trailing junk rejected",
                !valid("{\"active_users_start\":100,\"active_users_start\":100,\"churned_users\":5,\"arpu\":1,\"winback_cost_per_user\":1}") &&
                !valid(sample + "junk")},
        };
    }
}
