#pragma once
// ============================================================================
// МОДУЛЬ INDIVIDUAL: каркас роута.
// Не содержит математики — только вызовы функций варианта.
// ============================================================================

#include <string>
#include "../../lib/httplib.h"
#include "../../database.hpp"
#include "../../json.hpp"
#include "../../routes/auth.hpp"

// ────────────────────────────────────────────────────────────────────────────
// ПОДКЛЮЧИТЕ СВОЙ ВАРИАНТ ЗДЕСЬ (замените одну строку).
// ────────────────────────────────────────────────────────────────────────────
// #include "variant_roi.hpp"
#include "variant_mine.hpp"
#include "variant_tests.hpp"

namespace Individual {
    using namespace std;
    using namespace httplib;

    inline void init_routes(Server& svr) {

        // ─── GET /admin/run-tests-individual (admin/owner) ──────────────────
        // 20 проверок модуля варианта без обращения к БД.
        svr.Get("/api/v1.1/admin/run-tests-individual", [](const Request& req, Response& res) {
            string role, name;
            if (!Auth::auth(req, role, name)) { res.status = 401; return; }
            if (!Auth::admin(role))           { res.status = 403; return; }

            const auto tests = VariantTests::run();
            int passed = 0;
            JArr cases;
            for (size_t i = 0; i < tests.size(); ++i) {
                if (tests[i].second) ++passed;
                JObj o;
                o.int_("tc_id", (long long)(i + 1));
                o.str("name", tests[i].first);
                o.str("status", tests[i].second ? "PASSED" : "FAILED");
                cases.add(o.str());
            }
            JObj rep;
            rep.str("status", passed == (int)tests.size() ? "success" : "failed");
            rep.int_("total_tests", (long long)tests.size());
            rep.int_("passed", passed);
            rep.int_("failed", (long long)tests.size() - passed);
            rep.raw("test_cases", cases.str());
            res.set_content(rep.str(), "application/json");
        });

        // ─── POST /works/calc-individual ────────────────────────────────────
        svr.Post("/api/v1.1/works/calc-individual", [](const Request& req, Response& res) {
            string role, name;
            int uid = Auth::auth(req, role, name);
            if (!uid) { res.status = 401; return; }

            auto in = TaskVariant::parse(req.body);

            string err = TaskVariant::validate(in);
            if (!err.empty()) {
                JObj o; o.str("error", err);
                res.status = 400;
                res.set_content(o.str(), "application/json");
                return;
            }

            auto out = TaskVariant::calculate(in);
            string result = TaskVariant::to_json(out);

            if (!db_run("INSERT INTO works (user_id,datetime,input_json,result_json) "
                        "VALUES (?,?,?,?);",
                        {to_string(uid), now_iso(), req.body, result})) {
                JObj o; o.str("error", "Could not save result");
                res.status = 500;
                res.set_content(o.str(), "application/json");
                return;
            }
            res.set_content(result, "application/json");
        });

        // ─── GET /works/history-individual ──────────────────────────────────
        svr.Get("/api/v1.1/works/history-individual", [](const Request& req, Response& res) {
            string role, name;
            int uid = Auth::auth(req, role, name);
            if (!uid) { res.status = 401; return; }

            int limit = MAX_HISTORY_LIMIT;
            if (req.has_param("limit")) {
                try { limit = min(stoi(req.get_param_value("limit")), limit); }
                catch (...) {}
            }

            JArr arr;
            for (auto& r : db_all(
                "SELECT id,datetime,input_json,result_json FROM works "
                "WHERE user_id=" + to_string(uid) +
                " ORDER BY id DESC LIMIT " + to_string(limit) + ";")) {
                JObj o;
                o.int_("id", stoll(r[0]));
                o.str("datetime", r[1]);
                o.raw("input", r[2]);
                o.raw("result", r[3]);
                arr.add(o.str());
            }
            res.set_content(arr.str(), "application/json");
        });
    }
}