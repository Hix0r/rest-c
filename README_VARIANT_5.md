# Вариант 5 — Churn Leakage Estimator

Автор: Дроздов Артём Игоревич, группа 2, РАНХиГС.
Исходный проект: https://github.com/mal-corp/rest-c

## Сборка и запуск

Нужны C++17, SQLite и потоки.

**Windows, без CMake** (достаточно `g++`, например w64devkit):

```powershell
g++ -std=c++17 -O1 main.cpp -o rest_server.exe -lws2_32 -pthread
.\rest_server.exe
```

В репозитории есть `build.bat` (сборка) и `run.bat` (запуск). По README исходного проекта сборка возможна и через CMake:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/rest_server
```

Сервер слушает порт 8080 и создаёт `data/rest.db` относительно текущей папки.
Веб-коллекция раздаётся самим сервером: откройте http://localhost:8080/ (сервер нужно запускать из корня
проекта, где лежит папка `frontend`). Файл `frontend/index.html` можно открыть и напрямую в браузере.

## Контракт

`POST /api/v1.1/works/calc-individual`, заголовок `Authorization: Bearer <TOKEN>`.

```json
{"active_users_start":5000,"churned_users":250,"arpu":30.0,"winback_cost_per_user":15.0}
```

```json
{"churn_rate_percentage":5,"monthly_revenue_leakage":7500,"total_recovery_budget":3750}
```

- `active_users_start` — целое, `>= 10`; `churned_users` — целое, `0 <= x < active_users_start`.
- `arpu` и `winback_cost_per_user` — числа `> 0`.
- Все четыре поля обязательны; неизвестные и повторяющиеся поля, нечисловые значения, мусор после JSON — ошибка.
- Ошибки входных данных: HTTP 400 и `{"error":"..."}`; без токена — 401; сбой записи в БД — 500.

## Демонстрация (порядок для защиты)

1. `run.bat`, открыть http://localhost:8080/health.
2. Веб-коллекция: «Вход в систему» (user1/user1) → «Расчёт по своему варианту» → Отправить: 5 / 7500 / 3750.
3. Изменить `active_users_start` на 5: HTTP 400, `active_users_start must be >= 10`.
4. «История по своему варианту»: успешные расчёты сохранены.
5. Войти как admin1/admin1 → «20 тестов варианта 5» и «Запустить встроенные тесты»: по 20 из 20.
6. Под user1 те же кнопки дают 403; без токена — 401.

## Тестирование

| Что | Как | Результат |
|-----|-----|-----------|
| 20 модульных проверок варианта | `test.bat` или `ctest` | 20/20 |
| Те же 20 через API | `GET /api/v1.1/admin/run-tests-individual` (admin/owner) | 20/20 |
| Встроенные 20 системных + вариант | `GET /api/v1.1/admin/run-tests` | 20/20 |
| RBAC и коды ответов | `powershell -File test_rbac.ps1` | 33/33 |
| HTTP-проверки | `python tests/http_tests.py` | 87/87 |

`tests/http_tests.py` регистрирует и удаляет тестового пользователя, меняет пароли и создаёт расчёты:
запускайте сервер из пустой папки (тестовая БД), не на рабочей. Протоколы последнего запуска лежат в
`tests/http_results.json`, `tests/builtin_results.json`, `tests/variant_results.json`.

## Изменения относительно исходного проекта

- `variant_mine.hpp` — модуль варианта (`parse`, `validate`, `calculate`, `to_json`), только стандартная библиотека.
- `variant_tests.hpp` — 20 проверок варианта.
- `task.hpp` — подключение варианта, эндпоинт `/admin/run-tests-individual`, ответ 500 при сбое записи в БД.
- `public.hpp` — встроенные автотесты дополнены проверками варианта; различие 401/403 для `/admin/stats`.
- `auth.hpp` — 401/403 для списка и удаления пользователей; admin меняет пароль и удаляет только группу user,
  owner — любых (кроме owner1); 404 при смене пароля несуществующего пользователя; очистка сессий удалённых пользователей.
- `database.hpp` — `write_log` записывает дату (раньше в `/admin/stats` она была пустой).
- `main.cpp` — глобальный обработчик ошибок не затирает тело `{"error": ...}`.
- `frontend/index.html` — пример запроса варианта 5 и кнопка «20 тестов варианта 5».

Отчёты — в папке `reports/`, скриншоты и блок-схемы — в `docs/`.
