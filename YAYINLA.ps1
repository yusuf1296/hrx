# Osiris otomatik yayin betigi
# Kullanim (tercih edilen): kokteki YAYINLA.bat'a cift tikla.
# Ne yapar: surum dosyasini uretir -> README rozetini gunceller -> kaynagi senkronlar
#           -> GitHub Release (vSURUM) + dosya yukler -> push eder.

# Not: git/robocopy/curl stderr yazabilir; EAP Continue ile native cagrilari guvenli tutuyoruz,
# hata kontrolu her zaman $LASTEXITCODE uzerinden.
$ErrorActionPreference = "Continue"

$pub = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $pub
$osiris = Join-Path $root "osiris"
$injector = Join-Path $root "osiris-injector"
$repoApi = "https://api.github.com/repos/yusuf1296/osiris-updater"
$repoWeb = "https://github.com/yusuf1296/osiris-updater"
$assetDir = "https://github.com/yusuf1296/osiris-updater/releases/download"

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

# --- 1) surum ---
$versionFile = Join-Path $osiris "version.txt"
if (-not (Test-Path $versionFile)) { Write-Host "[HATA] $versionFile yok"; exit 1 }
$version = (Get-Content $versionFile -TotalCount 1).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { Write-Host "[HATA] gecersiz surum: '$version'"; exit 1 }

# --- 2) derleme kontrolu ---
$dllPath = Join-Path $osiris "x64\Release\Osiris.dll"
if (-not (Test-Path $dllPath)) { Write-Host "[HATA] Osiris.dll yok. Once Release x64 derle (Osiris.sln)."; exit 1 }
$exePath = Join-Path $injector "OsirisInjector.exe"

# --- 3) surum dosyasi (2. satir = release adresi) ---
$releaseDllUrl = "$assetDir/v$version/Osiris.dll"
Write-Utf8NoBom (Join-Path $pub "version.txt") "$version`r`n$releaseDllUrl`r`n"

# --- 4) README surum rozeti ---
$readmePath = Join-Path $pub "README.md"
$readme = [System.IO.File]::ReadAllText($readmePath)
$readme = [regex]::Replace($readme, 'version-\d+(\.\d+)+', "version-$version")
Write-Utf8NoBom $readmePath $readme

# --- 5) kaynak senkronu ---
Write-Host "- kaynak senkronlaniyor..."
Sync-Tree $osiris (Join-Path $pub "osiris") @(".git", "x64", ".vs", "build") @("*.user", "*.opendb", "*.suo")
Sync-Tree $injector (Join-Path $pub "injector") @(".vs") @("*.obj", "OsirisInjector.exe")

# --- 6) git commit ---
Set-Location $pub
$null = git add -A 2>&1
if ($LASTEXITCODE -ne 0) { Write-Host "[HATA] git add basarisiz"; exit 1 }
$null = git diff --cached --quiet 2>&1
if ($LASTEXITCODE -ne 0) {
    $commitOut = git commit -q -m "publish $version" 2>&1
    if ($LASTEXITCODE -ne 0) { Write-Host "[HATA] commit basarisiz: $commitOut"; exit 1 }
    Write-Host "- commit hazir: publish $version"
} else {
    Write-Host "- degisiklik yok, commit atlandi"
}

# --- 7) GitHub Release + dosya yukleme (push'tan ONCE) ---
$tokenPath = Join-Path $env:USERPROFILE ".osiris_gh_token"
if (-not (Test-Path $tokenPath)) {
    Write-Host "[HATA] token yok: $tokenPath"
    exit 1
}
$token = (Get-Content $tokenPath -Raw).Trim()
$headers = @{ Authorization = "Bearer $token"; "User-Agent" = "osiris-publish"; Accept = "application/vnd.github+json" }
$tag = "v$version"

$release = $null
try {
    $release = Invoke-RestMethod -Uri "$repoApi/releases/tags/$tag" -Headers $headers -TimeoutSec 30
    Write-Host "- release mevcut: $tag"
} catch {
    $notes = "Surum $version yayimlandi. Detaylar: CHANGELOG.md"
    $changelogPath = Join-Path $pub "CHANGELOG.md"
    if (Test-Path $changelogPath) {
        $changelog = [System.IO.File]::ReadAllText($changelogPath)
        $match = [regex]::Match($changelog, "(?ms)^## $([regex]::Escape($version))\b.*?(?=^## |\z)")
        if ($match.Success) { $notes = $match.Value.Trim() }
    }
    $body = @{ tag_name = $tag; target_commitish = "main"; name = "Osiris $version"; body = $notes; draft = $false; prerelease = $false } | ConvertTo-Json
    try {
        $release = Invoke-RestMethod -Uri "$repoApi/releases" -Method Post -Headers $headers -Body $body -TimeoutSec 30
        Write-Host "- release olusturuldu: $tag"
    } catch {
        Write-Host "[HATA] release olusturulamadi: $($_.ErrorDetails.Message)"
        exit 1
    }
}

$uploadBase = ($release.upload_url -replace '\{\?.*\}$', '')
$existingAssets = @($release.assets | ForEach-Object { $_.name })

# DLL zorunlu
if ($existingAssets -contains "Osiris.dll") {
    Write-Host "- Osiris.dll zaten release'te"
} else {
    Write-Host "- Osiris.dll yukleniyor..."
    $code = curl.exe -sS -w "%{http_code}" -o (Join-Path $env:TEMP "osiris_asset_resp.json") -X POST -H "Authorization: Bearer $token" -H "Content-Type: application/octet-stream" --data-binary "@$dllPath" "$($uploadBase)?name=Osiris.dll"
    if ($code -ne "201") {
        $errText = ""
        $errFile = Join-Path $env:TEMP "osiris_asset_resp.json"
        if (Test-Path $errFile) { $errText = Get-Content $errFile -Raw }
        Write-Host "[HATA] DLL yukleme basarisiz (http $code): $errText"
        exit 1
    }
    Write-Host "- Osiris.dll yuklendi"
}

# Injector (opsiyonel - yoksa sadece uyar)
if (Test-Path $exePath) {
    if ($existingAssets -contains "OsirisInjector.exe") {
        Write-Host "- OsirisInjector.exe zaten release'te"
    } else {
        Write-Host "- OsirisInjector.exe yukleniyor..."
        $code = curl.exe -sS -w "%{http_code}" -o (Join-Path $env:TEMP "osiris_asset_resp.json") -X POST -H "Authorization: Bearer $token" -H "Content-Type: application/octet-stream" --data-binary "@$exePath" "$($uploadBase)?name=OsirisInjector.exe"
        if ($code -ne "201") {
            Write-Host "- UYARI: injector yuklenemedi (http $code), sonraki yayinda tekrar denenir"
        } else {
            Write-Host "- OsirisInjector.exe yuklendi"
        }
    }
} else {
    Write-Host "- UYARI: OsirisInjector.exe yok (build.bat ile derle), release'a eklenmedi"
}

# --- 8) push (release hazir olduktan SONRA) ---
$pushOut = git push -q origin main 2>&1
if ($LASTEXITCODE -ne 0) { Write-Host "[HATA] push basarisiz: $pushOut"; exit 1 }

Write-Host ""
Write-Host "YAYIN TAMAM - surum $version"
Write-Host "  version : $repoWeb/blob/main/version.txt"
Write-Host "  release : $repoWeb/releases/tag/$tag"
Write-Host "Kullanicilar OsirisInjector.exe'yi yeniden calistirinca guncelleyecek."
exit 0
