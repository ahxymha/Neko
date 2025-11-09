param(
	[string] $CertPath,
	[string] $OutFile
)
$FileContent = Get-Content -Path $CertPath -Raw
$Base64Encoded = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($FileContent))
if($Base64Encoded){
	$content = @"
// Auto-generated certificate header
#pragma once
#define CERT "$Base64Encoded"
"@
	$content | Out-File -FilePath $OutFile -Encoding UTF8
    Write-Host "Cert generated"
}else{
	exit 1;
}