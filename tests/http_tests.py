#!/usr/bin/env python3
"""HTTP-проверки варианта 5 и RBAC. Только стандартная библиотека Python.

Запуск: python tests/http_tests.py [http://localhost:8080]
ВАЖНО: сценарий регистрирует/удаляет тестового пользователя, меняет его пароль
и создаёт расчёты. Запускайте на тестовой БД (сервер из пустой папки),
не на рабочей.
"""
import json, math, sys, urllib.request, urllib.error
from pathlib import Path

BASE = sys.argv[1] if len(sys.argv) > 1 else 'http://localhost:8080'
rows = []


def call(method, path, body=None, token=None, expected=200, label=None):
    raw = body if isinstance(body, str) else (json.dumps(body) if body is not None else None)
    headers = {'Content-Type': 'application/json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    req = urllib.request.Request(BASE + path, data=raw.encode() if raw is not None else None,
                                 headers=headers, method=method)
    try:
        res = urllib.request.urlopen(req)
    except urllib.error.HTTPError as e:
        res = e
    text = res.read().decode() or 'null'
    try:
        content = json.loads(text)
    except ValueError:
        content = text
    ok = res.status == expected
    rows.append(dict(name=label or method + ' ' + path, expected=expected, actual=res.status, passed=ok))
    assert ok, (rows[-1], text)
    return content


A = '/api/v1.1'
auth = A + '/auth'
route = A + '/works/calc-individual'

# --- публичные ---
call('GET', '/health')
call('GET', A + '/counters')
call('GET', A + '/history')

# --- вход ---
tokens = [call('POST', auth + '/login', dict(username=x, password=x))['token']
          for x in ['user1', 'admin1', 'owner1']]
u, a, o = tokens
for t in tokens:
    call('GET', auth + '/profile', token=t)
    call('POST', auth + '/refresh', token=t)
call('POST', auth + '/login', dict(username="' OR 1=1 --", password='x'), expected=401)
call('POST', auth + '/login', dict(username='user1', password='wrong'), expected=401)

# --- модуль варианта 5: корректные данные ---
sample = dict(active_users_start=5000, churned_users=250, arpu=30.0, winback_cost_per_user=15.0)
result = call('POST', route, sample, u)
assert math.isclose(result['churn_rate_percentage'], 5.0)
assert math.isclose(result['monthly_revenue_leakage'], 7500.0)
assert math.isclose(result['total_recovery_budget'], 3750.0)
call('POST', route, dict(active_users_start=10, churned_users=0, arpu=1, winback_cost_per_user=1), u,
     label='Boundary: active=10, churned=0')
call('POST', route, dict(active_users_start=10, churned_users=9, arpu=0.01, winback_cost_per_user=0.01), u,
     label='Boundary: churned=active-1, arpu=0.01')

# --- неверные значения ---
cases = [('active_users_start', 9), ('active_users_start', 10.5), ('active_users_start', -5),
         ('churned_users', -1), ('churned_users', 5000), ('churned_users', 6000), ('churned_users', 1.5),
         ('arpu', 0), ('arpu', -1), ('winback_cost_per_user', 0), ('winback_cost_per_user', -3),
         ('arpu', '30')]
for field, val in cases:
    r = call('POST', route, sample | {field: val}, u, 400, 'Invalid %s=%s' % (field, val))
    assert r['error'] != 'Request failed', r

# --- неполный / некорректный JSON ---
for raw in ['', '{}', '[]', 'null', '{"active_users_start":100,}', json.dumps(sample) + 'x',
            json.dumps(sample)[:-1] + ',"arpu":1}', json.dumps(sample)[:-1] + ',"extra":1}',
            json.dumps(sample).replace('30.0', 'NaN'), json.dumps(sample).replace('30.0', '1e999')]:
    r = call('POST', route, raw, u, 400, 'Invalid JSON: ' + raw[:45])
    assert 'error' in r and r['error'] != 'Request failed', r
for field in sample:
    bad = sample.copy()
    del bad[field]
    r = call('POST', route, bad, u, 400, 'Missing ' + field)
    assert r['error'] == field + ' is required', r

# --- авторизация модуля ---
call('POST', route, sample, None, 401)
call('POST', route, sample, 'invalid', 401)
call('POST', route, sample | {'churned_users': 500}, u)
hist = call('GET', A + '/works/history-individual', token=u)
assert hist[0]['input']['churned_users'] == 500
assert call('GET', A + '/works/history-individual', token=a) == []
call('GET', A + '/works/history-individual', expected=401)

# --- демонстрационный модуль ---
call('POST', A + '/works/calc', {'data': [1, 2, 3]}, u)
call('GET', A + '/works/history', token=u)

# --- RBAC: 401 без токена, 403 для user, 200 для admin/owner ---
for path in [auth + '/users', A + '/admin/stats', A + '/admin/run-tests', A + '/admin/run-tests-individual']:
    call('GET', path, expected=401)
    call('GET', path, token=u, expected=403)
    call('GET', path, token=a)
call('DELETE', auth + '/users', dict(username='user1'), expected=401)
call('DELETE', auth + '/users', dict(username='user1'), u, 403)

# --- автотесты сервера ---
for path, fname in [('/admin/run-tests', 'builtin_results.json'),
                    ('/admin/run-tests-individual', 'variant_results.json')]:
    r = call('GET', A + path, token=o)
    assert r['total_tests'] == 20 and r['passed'] == 20 and r['failed'] == 0, r
    Path(__file__).with_name(fname).write_text(json.dumps(r, ensure_ascii=False, indent=2), encoding='utf-8')

# --- логи: дата заполнена ---
logs = call('GET', A + '/admin/stats', token=a)
assert logs and all(x['datetime'] for x in logs[:5]), logs[:5]

# --- пароли и удаление ---
call('PUT', auth + '/users/password', dict(username='owner1', new_password='forbidden'), a, 403)
call('PUT', auth + '/users/password', dict(username='user1', new_password='user1'), a)
call('PUT', auth + '/users/password', dict(username='admin1', new_password='admin1'), o)
call('PUT', auth + '/users/password', dict(username='absent_test_user', new_password='pass123'), o, 404)
call('PUT', auth + '/users/password', dict(new_password='user1'), u)
call('PUT', auth + '/users/password', dict(username='admin1', new_password='x1234'), u, 403)
call('PUT', auth + '/users/password', dict(new_password='1'), u, 400)
call('DELETE', auth + '/users', dict(username='owner1'), o, 400)
call('DELETE', auth + '/users', dict(username='owner1'), a, 400)
call('DELETE', auth + '/users', dict(username='absent_test_user'), a, 404)
call('POST', auth + '/register', dict(username='variant_test_user', password='pass123'))
call('POST', auth + '/register', dict(username='variant_test_user', password='pass123'), expected=409)
call('POST', auth + '/register', dict(username='zz', password='1'), expected=400)
call('PUT', auth + '/users/password', dict(username='variant_test_user', new_password='new123'), a)
call('DELETE', auth + '/users', dict(username='variant_test_user'), a)

# --- служебное ---
call('OPTIONS', route, expected=204)
call('GET', '/no-such-route', expected=404)
call('POST', auth + '/logout', token=u)
call('GET', auth + '/profile', token=u, expected=401)

Path(__file__).with_name('http_results.json').write_text(
    json.dumps(dict(passed=len(rows), failed=0, cases=rows), ensure_ascii=False, indent=2), encoding='utf-8')
print('HTTP integration: %d/%d passed' % (len(rows), len(rows)))
