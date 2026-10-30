# Демонстрация API (сервер должен быть запущен: run.bat)
$u = "http://localhost:8080"
$login = Invoke-RestMethod "$u/api/v1.1/auth/login" -Method Post -ContentType application/json -Body '{"username":"user1","password":"user1"}'
$h = @{ Authorization = "Bearer $($login.token)" }

function Call($title, $body) {
    Write-Host "`n== $title" -ForegroundColor Cyan
    Write-Host $body
    try {
        $r = Invoke-WebRequest "$u/api/v1.1/works/calc-individual" -Method Post -Headers $h -ContentType application/json -Body $body -UseBasicParsing
        Write-Host "HTTP $($r.StatusCode): $($r.Content)" -ForegroundColor Green
    } catch {
        $resp = $_.Exception.Response
        $text = $_.ErrorDetails.Message
        Write-Host "HTTP $([int]$resp.StatusCode): $text" -ForegroundColor Yellow
    }
}

Call "Контрольный пример (ожидается 5 / 7500 / 3750)" '{"active_users_start":5000,"churned_users":250,"arpu":30.0,"winback_cost_per_user":15.0}'
Call "Граница: churned_users = 0" '{"active_users_start":10,"churned_users":0,"arpu":1,"winback_cost_per_user":1}'
Call "Ошибка: active_users_start < 10" '{"active_users_start":9,"churned_users":1,"arpu":1,"winback_cost_per_user":1}'
Call "Ошибка: churned_users >= active" '{"active_users_start":100,"churned_users":100,"arpu":1,"winback_cost_per_user":1}'
Call "Ошибка: arpu = 0" '{"active_users_start":100,"churned_users":5,"arpu":0,"winback_cost_per_user":1}'
Call "Ошибка: неполные данные" '{"active_users_start":100,"churned_users":5}'

Write-Host "`n== Без токена (ожидается 401)" -ForegroundColor Cyan
try { Invoke-WebRequest "$u/api/v1.1/works/calc-individual" -Method Post -Body '{}' -UseBasicParsing } catch { Write-Host "HTTP $([int]$_.Exception.Response.StatusCode)" -ForegroundColor Green }
