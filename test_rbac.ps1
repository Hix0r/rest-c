# Проверка RBAC и кодов ответов (сервер должен быть запущен: run.bat)
# Запуск: powershell -ExecutionPolicy Bypass -File test_rbac.ps1
Add-Type -AssemblyName System.Net.Http
$base = "http://localhost:8080/api/v1.1"
$script:pass = 0; $script:fail = 0

function Hit($m, $path, $tok, $body) {
    $c = New-Object System.Net.Http.HttpClient
    if ($tok) { $c.DefaultRequestHeaders.Add("Authorization", "Bearer $tok") }
    $req = New-Object System.Net.Http.HttpRequestMessage ([System.Net.Http.HttpMethod]::new($m)), "$base$path"
    if ($body) { $req.Content = New-Object System.Net.Http.StringContent($body, [Text.Encoding]::UTF8, "application/json") }
    $r = $c.SendAsync($req).Result
    [pscustomobject]@{ Code = [int]$r.StatusCode; Text = $r.Content.ReadAsStringAsync().Result }
}
function Check($name, $expected, $m, $path, $tok, $body) {
    $r = Hit $m $path $tok $body
    $okk = ($r.Code -eq $expected)
    if ($okk) { $script:pass++ } else { $script:fail++ }
    "{0} {1,-58} ожидалось {2}, получено {3}" -f ($(if ($okk) { "[OK]  " } else { "[FAIL]" })), $name, $expected, $r.Code
}
function Login($u, $p) {
    (Hit POST /auth/login $null "{`"username`":`"$u`",`"password`":`"$p`"}").Text | ConvertFrom-Json
}

$U = (Login user1 user1).token; $A = (Login admin1 admin1).token; $O = (Login owner1 owner1).token
Hit POST /auth/register $null '{"username":"victim","password":"victim1"}' | Out-Null
Hit POST /auth/register $null '{"username":"victim2","password":"victim2"}' | Out-Null

Check "Вход с неверным паролем"                         401 POST /auth/login $null '{"username":"user1","password":"x"}'
Check "SQL-инъекция в логине"                           401 POST /auth/login $null '{"username":"'' OR 1=1 --","password":"x"}'
Check "Повторная регистрация (дубль)"                   409 POST /auth/register $null '{"username":"victim","password":"victim1"}'
Check "Регистрация с коротким паролем"                  400 POST /auth/register $null '{"username":"zz","password":"1"}'
Check "Профиль без токена"                              401 GET  /auth/profile $null
Check "Профиль с токеном"                               200 GET  /auth/profile $U
Check "Список пользователей без токена"                401 GET  /auth/users $null
Check "Список пользователей: user"                      403 GET  /auth/users $U
Check "Список пользователей: admin"                     200 GET  /auth/users $A
Check "Логи без токена"                                 401 GET  /admin/stats $null
Check "Логи: user"                                      403 GET  /admin/stats $U
Check "Логи: admin"                                     200 GET  /admin/stats $A
Check "Автотесты: user"                                 403 GET  /admin/run-tests $U
Check "Автотесты: admin"                                200 GET  /admin/run-tests $A
Check "Свой пароль: user"                               200 PUT  /auth/users/password $U '{"new_password":"user1"}'
Check "Чужой пароль: user -> victim"                    403 PUT  /auth/users/password $U '{"username":"victim","new_password":"hacked"}'
Check "Пароль: admin -> user (разрешено)"               200 PUT  /auth/users/password $A '{"username":"victim","new_password":"victim1"}'
Check "Пароль: admin -> owner1 (запрещено)"             403 PUT  /auth/users/password $A '{"username":"owner1","new_password":"hacked"}'
Check "Пароль: owner -> admin1 (разрешено)"             200 PUT  /auth/users/password $O '{"username":"admin1","new_password":"admin1"}'
Check "Пароль несуществующего пользователя"             404 PUT  /auth/users/password $O '{"username":"ghost","new_password":"abcd"}'
Check "Удаление: user"                                  403 DELETE /auth/users $U '{"username":"victim"}'
Check "Удаление без токена"                             401 DELETE /auth/users $null '{"username":"victim"}'
Check "Удаление owner1 (запрещено всем)"                400 DELETE /auth/users $O '{"username":"owner1"}'
Check "Удаление несуществующего"                        404 DELETE /auth/users $A '{"username":"ghost"}'
Check "Удаление: admin -> victim (разрешено)"           200 DELETE /auth/users $A '{"username":"victim"}'
Check "Удаление: owner -> user (разрешено)"             200 DELETE /auth/users $O '{"username":"victim2"}'
Check "Расчёт варианта 5 без токена"                    401 POST /works/calc-individual $null '{}'
Check "Расчёт варианта 5: контрольный пример"           200 POST /works/calc-individual $U '{"active_users_start":5000,"churned_users":250,"arpu":30.0,"winback_cost_per_user":15.0}'
Check "Расчёт варианта 5: невалидные данные"            400 POST /works/calc-individual $U '{"active_users_start":5,"churned_users":1,"arpu":1,"winback_cost_per_user":1}'
Check "Расчёт варианта 5: неполные данные"              400 POST /works/calc-individual $U '{"churned_users":1}'
Check "История варианта 5"                              200 GET  /works/history-individual $U
Check "Logout"                                          200 POST /auth/logout $U
Check "Токен после logout недействителен"               401 GET  /auth/profile $U

"`nИтого: прошло $script:pass, провалено $script:fail"
if ($script:fail -gt 0) { exit 1 }
