# HRX otomatik yayin betigi
# Kullanim (tercih edilen): kokteki YAYINLA.bat'a cift tikla.
# Ne yapar: surum dosyasini uretir -> README rozetlerini gunceller -> kaynagi senkronlar
#           -> GitHub Release (vSURUM) + dosya yukler -> push eder.
# Not: git/robocopy/curl stderr yazabilir; EAP Continue ile native cagrilari guvenli tutuyoruz,
# hata kontrolu her zaman $LASTEXITCODE uzerinden. Dosya ASCII kodlamalidir (PS5.1 guvenligi).

$ErrorActionPreference = "Continue"

$pub = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $pub
$osiris = Join-Path $root "osiris"
$injector = Join-Path $root "osiris-injector"
$repoApi = "https://api.github.com/repos/yusuf1296/hrx"
$repoWeb = "https://github.com/yusuf1296/hrx"
$assetDir = "https://github.com/yusuf1296/hrx/releases/download"

function Write-Utf8NoBom([string]$path, [string]$text)
{
    [System.IO.File]::WriteAllText($path, $text, (New-Object System.Text.UTF8Encoding($false)))
}

function Sync-Tree([string]$source, [string]$dest, [string[]]$excludeDirs, [string[]]$excludeFiles)
{
    $robocopyArgs = @($source, $dest, "/MIR", "/NFL", "/NDL", "/NJH", "/NJS", "/NP", "/XD") + $excludeDirs + @("/XF") + $excludeFiles
    $null = robocopy @robocopyArgs 2>&1
    if ($LASTEXITCODE -ge 8) {
        throw "robocopy basarisiz ($LASTEXITCODE): $source"
    }
}

function Step([int]$n, [int]$total, [string]$msg, [string]$status = "OK", [string]$color = "Green")
{
    $m = $msg
    if ($m.Length -gt 54) { $m = $m.Substring(0, 51) + "..." }
    Write-Host "  [$n/$total] " -NoNewline -ForegroundColor Cyan
    Write-Host $m.PadRight(56) -NoNewline -ForegroundColor Gray
    Write-Host $status -ForegroundColor $color
}

function Detail([string]$msg, [string]$color = "DarkGray")
{
    Write-Host "              $msg" -ForegroundColor $color
}

function Fail([string]$msg)
{
    Write-Host ""
    Write-Host "  [HATA] $msg" -ForegroundColor Red
    exit 1
}

$totalSteps = 7

# --- 1) surum ---
$versionFile = Join-Path $osiris "version.txt"
if (-not (Test-Path $versionFile)) { Fail "$versionFile yok" }
$version = (Get-Content $versionFile -TotalCount 1).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { Fail "gecersiz surum: '$version'" }

Write-Host ""
Write-Host ("=" * 64) -ForegroundColor DarkCyan
Write-Host "  HRX YAYIN  -  surum $version" -ForegroundColor Cyan
Write-Host "  $repoWeb" -ForegroundColor DarkGray
Write-Host ("=" * 64) -ForegroundColor DarkCyan
Write-Host ""
Step 1 $totalSteps "surum dogrulandi: $version"

# --- 2) derleme kontrolu ---
$dllPath = Join-Path $osiris "x64\Release\Osiris.dll"
if (-not (Test-Path $dllPath)) { Fail "Osiris.dll yok. Once Release x64 derle (Osiris.sln)." }
$exePath = Join-Path $injector "HRX.exe"
if (-not (Test-Path $exePath)) {
    $legacyExe = Join-Path $injector "OsirisInjector.exe"
    if (Test-Path $legacyExe) { $exePath = $legacyExe }
}
$exeAssetName = Split-Path -Leaf $exePath
Step 2 $totalSteps "derleme bulundu: Osiris.dll (Release x64)"

# --- 3) surum dosyasi (2. satir = release adresi) + README rozetleri ---
$releaseDllUrl = "$assetDir/v$version/Osiris.dll"
Write-Utf8NoBom (Join-Path $pub "version.txt") "$version`r`n$releaseDllUrl`r`n"
foreach ($readmeName in @("README.md", "README.en.md")) {
    $readmePath = Join-Path $pub $readmeName
    if (-not (Test-Path $readmePath)) { continue }
    $readme = [System.IO.File]::ReadAllText($readmePath)
    $readme = [regex]::Replace($readme, 'version-\d+(\.\d+)+', "version-$version")
    Write-Utf8NoBom $readmePath $readme
}
Step 3 $totalSteps "version.txt + README rozetleri guncellendi"

# --- 4) kaynak senkronu ---
try {
    Sync-Tree $osiris (Join-Path $pub "osiris") @(".git", "x64", ".vs", "build") @("*.user", "*.opendb", "*.suo")
    Sync-Tree $injector (Join-Path $pub "injector") @(".vs") @("*.obj", "OsirisInjector.exe", "HRX.exe")
} catch {
    Fail $_.Exception.Message
}
Step 4 $totalSteps "kaynak senkron: osiris/ + injector/"

# --- 5) git commit ---
Set-Location $pub
$null = git add -A 2>&1
if ($LASTEXITCODE -ne 0) { Fail "git add basarisiz" }
$null = git diff --cached --quiet 2>&1
if ($LASTEXITCODE -ne 0) {
    $commitOut = git commit -q -m "publish $version" 2>&1
    if ($LASTEXITCODE -ne 0) { Fail "commit basarisiz: $commitOut" }
    Step 5 $totalSteps "commit: publish $version"
} else {
    Step 5 $totalSteps "commit: degisiklik yok" "atlandi" "DarkYellow"
}

# --- 6) GitHub Release + dosya yukleme (push'tan ONCE) ---
$tokenPath = Join-Path $env:USERPROFILE ".osiris_gh_token"
if (-not (Test-Path $tokenPath)) { Fail "token yok: $tokenPath" }
$token = (Get-Content $tokenPath -Raw).Trim()
$headers = @{ Authorization = "Bearer $token"; "User-Agent" = "hrx-publish"; Accept = "application/vnd.github+json" }
$tag = "v$version"

$release = $null
try {
    $release = Invoke-RestMethod -Uri "$repoApi/releases/tags/$tag" -Headers $headers -TimeoutSec 30
} catch {
    $notes = "Surum $version yayimlandi. Detaylar: CHANGELOG.md"
    $changelogPath = Join-Path $pub "CHANGELOG.md"
    if (Test-Path $changelogPath) {
        $changelog = [System.IO.File]::ReadAllText($changelogPath)
        $match = [regex]::Match($changelog, "(?ms)^## \[?$([regex]::Escape($version))\]?(\s|$).*?(?=^## |\z)")
        if ($match.Success) { $notes = $match.Value.Trim() }
    }
    $body = @{ tag_name = $tag; target_commitish = "main"; name = "HRX $version"; body = $notes; draft = $false; prerelease = $false } | ConvertTo-Json
    try {
        $release = Invoke-RestMethod -Uri "$repoApi/releases" -Method Post -Headers $headers -Body $body -TimeoutSec 30
        Detail "release olusturuldu: $tag" "DarkGreen"
    } catch {
        Fail "release olusturulamadi: $($_.ErrorDetails.Message)"
    }
}

$uploadBase = ($release.upload_url -replace '\{\?.*\}$', '')
$existingAssets = @($release.assets | ForEach-Object { $_.name })
$exeWarn = $false

# DLL zorunlu
if ($existingAssets -contains "Osiris.dll") {
    Detail "Osiris.dll zaten release'te"
} else {
    $code = curl.exe -sS -w "%{http_code}" -o (Join-Path $env:TEMP "osiris_asset_resp.json") -X POST -H "Authorization: Bearer $token" -H "Content-Type: application/octet-stream" --data-binary "@$dllPath" "$($uploadBase)?name=Osiris.dll"
    if ($code -ne "201") {
        $errText = ""
        $errFile = Join-Path $env:TEMP "osiris_asset_resp.json"
        if (Test-Path $errFile) { $errText = Get-Content $errFile -Raw }
        Fail "DLL yukleme basarisiz (http $code): $errText"
    }
    Detail "Osiris.dll yuklendi" "DarkGreen"
}

# Injector (opsiyonel - yoksa sadece uyar)
if (Test-Path $exePath) {
    if ($existingAssets -contains $exeAssetName) {
        Detail "$exeAssetName zaten release'te"
    } else {
        $code = curl.exe -sS -w "%{http_code}" -o (Join-Path $env:TEMP "osiris_asset_resp.json") -X POST -H "Authorization: Bearer $token" -H "Content-Type: application/octet-stream" --data-binary "@$exePath" "$($uploadBase)?name=$exeAssetName"
        if ($code -ne "201") {
            Detail "UYARI: injector yuklenemedi (http $code), sonraki yayinda tekrar denenir" "Yellow"
            $exeWarn = $true
        } else {
            Detail "$exeAssetName yuklendi" "DarkGreen"
        }
    }
} else {
    Detail "UYARI: HRX.exe yok (build.bat ile derle), release'a eklenmedi" "Yellow"
    $exeWarn = $true
}

if ($exeWarn) {
    Step 6 $totalSteps "release ${tag}: dll hazir" "UYARI" "Yellow"
} else {
    Step 6 $totalSteps "release ${tag}: dll + exe hazir"
}

# --- 7) push (release hazir olduktan SONRA) ---
$pushOut = git push -q origin main 2>&1
if ($LASTEXITCODE -ne 0) { Fail "push basarisiz: $pushOut" }
Step 7 $totalSteps "push origin main"

Write-Host ""
Write-Host ("-" * 64) -ForegroundColor DarkCyan
Write-Host "  YAYIN TAMAM - surum $version" -ForegroundColor Green
Write-Host "  version : $repoWeb/blob/main/version.txt" -ForegroundColor Gray
Write-Host "  release : $repoWeb/releases/tag/$tag" -ForegroundColor Gray
Write-Host "  Kullanicilar HRX.exe'yi yeniden calistirinca guncelleyecek." -ForegroundColor DarkGray
Write-Host ("-" * 64) -ForegroundColor DarkCyan
exit 0
