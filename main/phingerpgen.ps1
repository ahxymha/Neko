param(
    [string]$CertificatePath,
    [string]$OutputFile
)

# 提取证书指纹
$cert = Get-PfxCertificate -FilePath $CertificatePath
if ($cert) {
    $thumbprint = $cert.Thumbprint
    # 生成头文件内容
    $content = @"
// Auto-generated certificate header
#pragma once
#define CERTIFICATE_THUMBPRINT "$thumbprint"
#define CERTIFICATE_THUMBPRINT_LENGTH $($thumbprint.Length)
"@
    $content | Out-File -FilePath $OutputFile -Encoding UTF8
    Write-Host "Certificate thumbprint extracted: $thumbprint"
} else {
    Write-Error "Failed to extract certificate thumbprint"
    exit 1
}