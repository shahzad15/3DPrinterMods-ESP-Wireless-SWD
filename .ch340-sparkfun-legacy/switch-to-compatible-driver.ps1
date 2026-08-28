$ErrorActionPreference = 'Stop'

$deviceId = 'USB\VID_1A86&PID_7523\5&18A0A658&0&7'
$legacyInf = Join-Path $PSScriptRoot 'extracted\CH341SER.INF'
$logPath = Join-Path $PSScriptRoot 'switch-driver.log'
$deviceDisabled = $false

Start-Transcript -Path $logPath -Force
try {
    if (-not (Test-Path -LiteralPath $legacyInf)) {
        throw "Compatible driver INF is missing: $legacyInf"
    }

    $legacyText = Get-Content -LiteralPath $legacyInf -Raw
    if ($legacyText -notmatch 'DriverVer\s*=\s*08/08/2014,\s*3\.4\.2014\.08' -or
        $legacyText -notmatch 'USB\\VID_1A86&PID_7523') {
        throw 'The selected INF is not the expected CH340 3.4.2014.08 package.'
    }

    $active = Get-CimInstance Win32_PnPSignedDriver |
        Where-Object { $_.DeviceID -eq $deviceId } |
        Select-Object -First 1
    if (-not $active) {
        throw "CH340 device was not found at expected instance ID $deviceId."
    }
    $incompatibleVersions = @('3.9.2024.9', '4.0.2026.2')
    if ($active.DriverVersion -notin $incompatibleVersions) {
        throw "Active driver is $($active.DriverVersion), not a recognized incompatible CH340 version. No changes made."
    }

    $activeInf = $active.InfName
    if ($activeInf -notmatch '^oem\d+\.inf$') {
        throw "Unexpected active INF name: $activeInf"
    }

    Write-Host 'Staging Microsoft-signed CH340 driver 3.4.2014.08...'
    pnputil /add-driver $legacyInf
    if ($LASTEXITCODE -ne 0) {
        throw "Could not stage the compatible driver (exit code $LASTEXITCODE)."
    }

    $storedDrivers = (pnputil /enum-drivers /class Ports) -join "`n"
    if ($storedDrivers -notmatch 'Driver Version:\s+08/08/2014\s+3\.4\.2014\.8') {
        throw 'Compatible driver was not found in the driver store after staging.'
    }

    Write-Host "Temporarily disabling only $deviceId..."
    pnputil /disable-device $deviceId
    if ($LASTEXITCODE -ne 0) {
        throw "Could not disable the CH340 device (exit code $LASTEXITCODE)."
    }
    $deviceDisabled = $true

    Write-Host "Removing incompatible active package $activeInf..."
    pnputil /delete-driver $activeInf /uninstall /force
    if ($LASTEXITCODE -ne 0) {
        throw "Could not remove $activeInf (exit code $LASTEXITCODE)."
    }

    Write-Host 'Re-enabling the CH340 device...'
    pnputil /enable-device $deviceId
    if ($LASTEXITCODE -ne 0) {
        throw "Could not re-enable the CH340 device (exit code $LASTEXITCODE)."
    }
    $deviceDisabled = $false

    pnputil /scan-devices
    Start-Sleep -Seconds 2

    $result = Get-CimInstance Win32_PnPSignedDriver |
        Where-Object { $_.DeviceID -eq $deviceId } |
        Select-Object -First 1
    $binding = (pnputil /enum-devices /instanceid $deviceId /drivers) -join "`n"
    Write-Host $binding
    if ($binding -notmatch '(?s)Driver Version:\s+08/08/2014\s+3\.4\.2014\.8.*?Driver Status:\s+Best Ranked / Installed') {
        throw 'Windows did not bind the expected compatible driver.'
    }

    Write-Host 'Driver switch completed successfully.'
}
finally {
    if ($deviceDisabled) {
        Write-Host 'Restoring the CH340 device after an error...'
        pnputil /enable-device $deviceId
    }
    Stop-Transcript
}
