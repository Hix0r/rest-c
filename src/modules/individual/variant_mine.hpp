#pragma once
// ============================================================================
// ВАРИАНТ 5: Калькулятор упущенной выгоды от оттока клиентов
//            (Churn Leakage Estimator).
//
// Входные поля JSON (строгий плоский объект, все поля обязательны):
//   active_users_start    (int)    >= 10
//   churned_users         (int)    0 <= x < active_users_start
//   arpu                  (double) > 0
//   winback_cost_per_user (double) > 0
//
// Формулы:
//   churn_rate_percentage     = churned_users / active_users_start * 100
//   monthly_revenue_leakage   = churned_users * arpu
//   total_recovery_budget     = churned_users * winback_cost_per_user
// ============================================================================

#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include "../../json.hpp"

namespace TaskVariant {

    struct Input {
        long long active_users_start;
        long long churned_users;
        double    arpu;
        double    winback_cost_per_user;

        // Пустая строка = JSON разобран без ошибок. Иначе validate()
        // вернёт это сообщение (-> HTTP 400).
        std::string parse_error;
    };

    struct Output {
        double churn_rate_percentage;
        double monthly_revenue_leakage;
        double total_recovery_budget;
    };

    // ------------------------------------------------------------------------
    // Проверка входных данных: "" если всё ок, иначе текст ошибки.
    // ------------------------------------------------------------------------
    inline std::string validate(const Input& in) {
        if (!in.parse_error.empty())
            return in.parse_error;
        if (in.active_users_start < 10)
            return "active_users_start must be >= 10";
        if (in.churned_users < 0)
            return "churned_users must be >= 0";
        if (in.churned_users >= in.active_users_start)
            return "churned_users must be < active_users_start";
        if (!(in.arpu > 0.0))
            return "arpu must be > 0.0";
        if (!(in.winback_cost_per_user > 0.0))
            return "winback_cost_per_user must be > 0.0";
        return "";
    }

    inline Output calculate(const Input& in) {
        Output out{};
        out.churn_rate_percentage =
            ((double)in.churned_users / (double)in.active_users_start) * 100.0;
        out.monthly_revenue_leakage = (double)in.churned_users * in.arpu;
        out.total_recovery_budget   = (double)in.churned_users * in.winback_cost_per_user;
        return out;
    }

    // ------------------------------------------------------------------------
    // Строгий разбор плоского JSON-объекта из четырёх числовых полей.
    // Ошибкой считаются: не JSON-объект, неизвестное или повторяющееся поле,
    // нечисловое значение, NaN/Inf, отсутствующее поле, дробное значение
    // в int-поле, мусор после закрывающей скобки.
    // ------------------------------------------------------------------------
    inline Input parse(const std::string& body) {
        Input in{};
        static const char* const keys[4] = {
            "active_users_start", "churned_users", "arpu", "winback_cost_per_user"
        };
        double val[4] = {0, 0, 0, 0};
        bool   seen[4] = {false, false, false, false};

        size_t pos = 0;
        auto ws = [&] {
            while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' ||
                                         body[pos] == '\r' || body[pos] == '\n')) ++pos;
        };
        auto fail = [&](const std::string& e) { in.parse_error = e; return in; };

        ws();
        if (pos >= body.size() || body[pos] != '{') return fail("Expected JSON object");
        ++pos; ws();

        if (pos < body.size() && body[pos] != '}') {
            while (true) {
                if (pos >= body.size() || body[pos] != '"') return fail("Invalid JSON: expected field name");
                size_t e = body.find('"', pos + 1);
                if (e == std::string::npos) return fail("Invalid JSON: unterminated field name");
                std::string key = body.substr(pos + 1, e - pos - 1);
                pos = e + 1; ws();
                if (pos >= body.size() || body[pos] != ':') return fail("Invalid JSON: expected ':'");
                ++pos; ws();

                int idx = -1;
                for (int i = 0; i < 4; ++i) if (key == keys[i]) idx = i;
                if (idx < 0)     return fail("Unknown field: " + key);
                if (seen[idx])   return fail("Duplicate field: " + key);

                size_t b = pos;
                while (pos < body.size() && std::string("+-.0123456789eE").find(body[pos]) != std::string::npos) ++pos;
                std::string tok = body.substr(b, pos - b);
                if (tok.empty() || tok[0] == '+') return fail(key + " must be a number");
                char* end = nullptr;
                double v = std::strtod(tok.c_str(), &end);
                if (end != tok.c_str() + tok.size() || !std::isfinite(v))
                    return fail(key + " must be a number");
                val[idx] = v; seen[idx] = true;

                ws();
                if (pos < body.size() && body[pos] == ',') { ++pos; ws(); continue; }
                break;
            }
        }
        if (pos >= body.size() || body[pos] != '}') return fail("Invalid JSON: expected '}'");
        ++pos; ws();
        if (pos != body.size()) return fail("Unexpected content after JSON object");

        for (int i = 0; i < 4; ++i)
            if (!seen[i]) return fail(std::string(keys[i]) + " is required");
        for (int i = 0; i < 2; ++i)
            if (val[i] != std::floor(val[i]) || std::fabs(val[i]) > 9e15)
                return fail(std::string(keys[i]) + " must be an integer");

        in.active_users_start    = (long long)val[0];
        in.churned_users         = (long long)val[1];
        in.arpu                  = val[2];
        in.winback_cost_per_user = val[3];
        return in;
    }

    // Число без экспоненты (JObj::num печатает 1.2e+06 для больших значений).
    inline std::string fmt(double v) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.10g", v);
        std::string s = buf;
        if (s.find('e') != std::string::npos) {
            std::snprintf(buf, sizeof buf, "%.2f", v);
            s = buf;
        }
        return s;
    }

    inline std::string to_json(const Output& out) {
        JObj o;
        o.raw("churn_rate_percentage",   fmt(out.churn_rate_percentage));
        o.raw("monthly_revenue_leakage", fmt(out.monthly_revenue_leakage));
        o.raw("total_recovery_budget",   fmt(out.total_recovery_budget));
        return o.str();
    }

} // namespace TaskVariant
