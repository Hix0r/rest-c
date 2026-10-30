#pragma once
// ============================================================================
// Публичные и административные эндпоинты.
//
//   1. GET /api/v1.1/counters        — счётчики (без авторизации)
//   2. GET /api/v1.1/history         — публичная история (без авторизации)
//   3. GET /api/v1.1/admin/stats     — логи (только admin/owner)
//   4. GET /api/v1.1/admin/run-tests — самотестирование (только admin/owner)
// ============================================================================

#include <string>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <vector>
#include <map>
#include "../lib/httplib.h"
#include "../database.hpp"
#include "../json.hpp"
#include "../crypto.hpp"
#include "auth.hpp"
#include "../modules/individual/variant_mine.hpp"

namespace Public {
    using namespace std;
    using namespace httplib;

    inline void init_routes(Server& svr) {

        // ─── 1. Счётчики ────────────────────────────────────────────────────
        svr.Get("/api/v1.1/counters", [](const Request&, Response& res) {
            auto count = [](const string& q) {
                auto r = db_row(q);
                return r.empty() ? 0LL : stoll(r[0]);
            };

            JObj o;
            o.int_("total_accounts",          count("SELECT COUNT(*) FROM users;"));
            o.int_("successful_calculations", count("SELECT COUNT(*) FROM works;"));
            o.int_("failed_validations",
                   count("SELECT COUNT(*) FROM works WHERE result_json LIKE '%error%';"));
            res.set_content(o.str(), "application/json");
        });

        // ─── 2. Публичная история ───────────────────────────────────────────
        svr.Get("/api/v1.1/history", [](const Request& req, Response& res) {
            int limit = MAX_HISTORY_LIMIT;
            if (req.has_param("limit")) {
                try { limit = min(stoi(req.get_param_value("limit")), limit); }
                catch (...) {}
            }

            JArr arr;
            for (auto& r : db_all(
                "SELECT w.id,u.username,w.datetime,w.input_json,w.result_json "
                "FROM works w JOIN users u ON w.user_id=u.id "
                "ORDER BY w.id DESC LIMIT " + to_string(limit) + ";")) {
                JObj o;
                o.int_("id", stoll(r[0]));
                o.str("username", r[1]);
                o.str("datetime", r[2]);
                o.raw("input", r[3]);
                o.raw("result", r[4]);
                arr.add(o.str());
            }
            res.set_content(arr.str(), "application/json");
        });

        // ─── 3. Логи (только admin/owner) ───────────────────────────────────
        svr.Get("/api/v1.1/admin/stats", [](const Request& req, Response& res) {
            string role, name;
            if (!Auth::auth(req, role, name)) { res.status = 401; return; }
            if (!Auth::admin(role))           { res.status = 403; return; }

            JArr arr;
            for (auto& r : db_all(
                "SELECT id,datetime,status,message FROM logs "
                "ORDER BY id DESC LIMIT " + to_string(MAX_HISTORY_LIMIT) + ";")) {
                JObj o;
                o.int_("id", stoll(r[0]));
                o.str("datetime", r[1]);
                o.str("status", r[2]);
                o.str("message", r[3]);
                arr.add(o.str());
            }
            res.set_content(arr.str(), "application/json");
        });

        // ─── 4. Самотестирование ────────────────────────────────────────────
        svr.Get("/api/v1.1/admin/run-tests", [](const Request& req, Response& res) {
            string role, name;
            if (!Auth::auth(req, role, name)) { res.status = 401; return; }
            if (!Auth::admin(role))           { res.status = 403; return; }

            // Хелперы для проверок модуля варианта 5.
            auto body = [](const string& a, const string& c, const string& r, const string& w) {
                return "{\"active_users_start\":" + a + ",\"churned_users\":" + c +
                       ",\"arpu\":" + r + ",\"winback_cost_per_user\":" + w + "}";
            };
            auto err = [&](const string& b) { return TaskVariant::validate(TaskVariant::parse(b)); };
            auto ok  = [&](const string& b) { return err(b).empty(); };
            auto ref = TaskVariant::calculate(TaskVariant::parse(body("5000", "250", "30.0", "15.0")));
            string h = hash_password("test");

            vector<pair<string,bool>> tests = {
                // --- система (6) ---
                {"DB handle active",                     db != nullptr},
                {"Users table accessible",               !db_row("SELECT 1 FROM users LIMIT 1;").empty()},
                {"Password verify accepts valid",        verify_password("test", h)},
                {"Password verify rejects invalid",      !verify_password("wrong", h)},
                {"Token unique, prefix TK",              make_token() != make_token() && make_token().rfind("TK",0) == 0},
                {"Config: TTL > 0, history limit > 0",   TOKEN_TTL_SECONDS > 0 && MAX_HISTORY_LIMIT > 0},
                // --- вариант 5: корректные данные (4) ---
                {"V5 sample: churn_rate = 5.0",          fabs(ref.churn_rate_percentage - 5.0) < 1e-9},
                {"V5 sample: leakage = 7500.0",          fabs(ref.monthly_revenue_leakage - 7500.0) < 1e-9},
                {"V5 sample: recovery budget = 3750.0",  fabs(ref.total_recovery_budget - 3750.0) < 1e-9},
                {"V5 valid input accepted",              ok(body("5000", "250", "30.0", "15.0"))},
                // --- граничные значения (4) ---
                {"V5 boundary: active_users_start = 10", ok(body("10", "0", "1", "1"))},
                {"V5 boundary: churned = active - 1",    ok(body("10", "9", "1", "1"))},
                {"V5 boundary: churned = 0 -> zeros",    TaskVariant::calculate(TaskVariant::parse(body("10", "0", "5", "5"))).total_recovery_budget == 0.0},
                {"V5 boundary: arpu = 0.01 accepted",    ok(body("10", "1", "0.01", "0.01"))},
                // --- неверные данные (4) ---
                {"V5 invalid: active_users_start = 9",   !err(body("9", "1", "1", "1")).empty()},
                {"V5 invalid: churned = active",         !err(body("100", "100", "1", "1")).empty()},
                {"V5 invalid: churned < 0",              !err(body("100", "-1", "1", "1")).empty()},
                {"V5 invalid: arpu = 0 / winback < 0",   !err(body("100", "5", "0", "1")).empty() && !err(body("100", "5", "1", "-3")).empty()},
                // --- неполные данные (2) ---
                {"V5 incomplete: empty body",            !err("").empty()},
                {"V5 incomplete: missing arpu",          err("{\"active_users_start\":100,\"churned_users\":5,\"winback_cost_per_user\":2}") == "arpu is required"},
            };
            int passed = 0;
            JArr cases;
            for (size_t i = 0; i < tests.size(); ++i) {
                if (tests[i].second) passed++;
                JObj o;
                o.int_("tc_id", (long long)(i + 1));
                o.str("name", tests[i].first);
                o.str("status", tests[i].second ? "PASSED" : "FAILED");
                cases.add(o.str());
            }

            JObj rep;
            rep.str("status", passed == 20 ? "success" : "failed");
            rep.int_("total_tests", 20);
            rep.int_("passed", passed);
            rep.int_("failed", 20 - passed);
            rep.raw("test_cases", cases.str());
            res.set_content(rep.str(), "application/json");
        });
    }
}