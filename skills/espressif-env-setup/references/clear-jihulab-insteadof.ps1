# Clear jihulab url.*insteadOf only if present, then verify.
# See mirrors.md § clear-jihulab-insteadof: check → clear if any → verify

$ErrorActionPreference = "Continue"
$keys = git config --global --get-regexp 'url\..*jihulab' 2>$null
if (-not $keys) {
    Write-Host "No jihulab url.*insteadOf entries found. Skip clear."
    exit 0
}

$keys | ForEach-Object {
    $key = ($_ -split '\s+', 2)[0]
    git config --global --unset-all $key 2>$null
    Write-Host "Unset: $key"
}
git config --global --unset-all 'url.https://jihulab.com/esp-mirror/.insteadOf' 2>$null

Write-Host "`nRemaining jihulab entries (should be empty):"
git config --global --get-regexp jihulab
if ($LASTEXITCODE -ne 0) {
    Write-Host "(none)"
}
