# SignBridge — własny certyfikat do podpisywania kodu (do czasu zakupu certyfikatu z publicznego CA).
# Uruchom RAZ na komputerze budującym, jako administrator:
#   powershell -ExecutionPolicy Bypass -File tools\make-signing-cert.ps1
# Wersje zmienione (fork) — podaj własną nazwę wydawcy (nazwy „Craftprojects” nie wolno używać, zob. README):
#   powershell -ExecutionPolicy Bypass -File tools\make-signing-cert.ps1 -Publisher "Moja Firma" -FileName "MojaFirma-CodeSigning"
# Tworzy certyfikat wydawcy -Publisher (domyślnie "Craftprojects Sp. z o.o.", 10 lat) w magazynie użytkownika, zapisuje:
#   dist\Craftprojects-CodeSigning.cer   — klucz PUBLICZNY do wgrania na komputery (GPO), bezpieczny do rozsyłania
#   ~\Craftprojects-CodeSigning.pfx      — kopia zapasowa z kluczem prywatnym (hasło poniżej) — trzymać poza repo!
# i wypisuje odcisk do zmiennej SB_SIGN_THUMBPRINT dla build.cmd.
#
# Zaufanie na komputerach (bez tego UAC nadal pokaże "Unknown publisher"):
#   GPO: Konfiguracja komputera → Zasady → Ustawienia systemu Windows → Ustawienia zabezpieczeń → Zasady kluczy publicznych
#        → Zaufane główne urzędy certyfikacji: importuj .cer   ORAZ   → Zaufani wydawcy: importuj .cer
#   ręcznie: certutil -addstore -f Root dist\Craftprojects-CodeSigning.cer
#            certutil -addstore -f TrustedPublisher dist\Craftprojects-CodeSigning.cer
param(
    [string]$Publisher = "Craftprojects Sp. z o.o.",
    [string]$FileName  = "Craftprojects-CodeSigning"
)
$ErrorActionPreference = "Stop"
$subject = "CN=$Publisher, O=$Publisher, C=PL"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$existing = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $subject -and $_.EnhancedKeyUsageList.ObjectId -contains "1.3.6.1.5.5.7.3.3" }
if ($existing) {
    $cert = $existing | Sort-Object NotAfter -Descending | Select-Object -First 1
    Write-Host "[.] Istniejący certyfikat: $($cert.Thumbprint) (ważny do $($cert.NotAfter.ToShortDateString()))"
} else {
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $subject `
        -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 -KeyExportPolicy Exportable `
        -CertStoreLocation Cert:\CurrentUser\My -NotAfter (Get-Date).AddYears(10) -FriendlyName "SignBridge code signing"
    Write-Host "[+] Utworzono certyfikat: $($cert.Thumbprint)"
}
New-Item -ItemType Directory -Force -Path "$root\dist" | Out-Null
Export-Certificate -Cert $cert -FilePath "$root\dist\$FileName.cer" -Force | Out-Null
$pfx = Join-Path $env:USERPROFILE "$FileName.pfx"
if (-not (Test-Path $pfx)) {
    $pwd = Read-Host -AsSecureString "Hasło do kopii zapasowej PFX (zapisz w menedżerze haseł)"
    Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $pwd | Out-Null
    Write-Host "[+] Kopia zapasowa klucza: $pfx"
}
Write-Host ""
Write-Host "[+] Klucz publiczny do GPO: $root\dist\$FileName.cer"
Write-Host "[+] Przed buildem ustaw:   `$env:SB_SIGN_THUMBPRINT = `"$($cert.Thumbprint)`""
