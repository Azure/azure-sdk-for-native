#!/usr/bin/env pwsh

[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param (
    [Parameter(Mandatory = $true)]
    [string] $ResourceGroupName,

    [Parameter()]
    [string] $TestApplicationOid,

    # The DeploymentOutputs parameter is only valid in the test-resources-post.ps1 script.
    [Parameter()]
    [hashtable] $DeploymentOutputs,

    # Captures any arguments from eng/New-TestResources.ps1 not declared here (no parameter errors).
    [Parameter(ValueFromRemainingArguments = $true)]
    $RemainingArguments
)

###### setup ######
Install-Module -Name Az -RequiredVersion 4.8.0 -Force -AllowClobber
Install-Module -Name Az.DeviceProvisioningServices -Force

if ($IsLinux) { 
  $module_location_prefix = "$HOME\.local\share\powershell\Modules"
  Invoke-Expression -Command "sudo apt install libssl-dev"
}
if ($IsWindows) { 
  $module_location_prefix = "$HOME\Documents\PowerShell\Modules"
}

try {
  Import-Module -Name $module_location_prefix\Az.IotHub -Force
} catch { throw "Az.IotHub module failed force import" }

try {
  Import-Module -Name $module_location_prefix\Az.DeviceProvisioningServices -Cmdlet Add-AzIoTDeviceProvisioningServiceLinkedHub -Force 
} catch { throw "Az.DeviceProvisioningServices module failed force import" }

$orig_loc = Get-Location
Write-Host $orig_loc
#Write-Host "##vso[task.setvariable variable=VCPKG_DEFAULT_TRIPLET]x64-windows-static"
Write-Host "##vso[task.setvariable variable=VCPKG_ROOT]:$orig_loc/vcpkg"
cd $orig_loc\sdk\samples\iot\
$sourcesDir = Get-Location

$region = $DeploymentOutputs['._LOCATION']
$deviceID = "aziotbld-c-sample"
$deviceIDSaS = "aziotbld-c-sample-sas"
# $dpsName = $DeploymentOutputs['IOT_PROVISIONING_SERVICE_NAME']
$iothubName = $DeploymentOutputs['IOT_HUB_NAME']

###### X509 setup ######
# Generate certificate 
openssl ecparam -out device_ec_key.pem -name prime256v1 -genkey
openssl req -new -days 12 -nodes -x509 -key device_ec_key.pem -out device_ec_cert.pem -config x509_config.cfg -subj "/CN=$deviceID"

Get-Content -Path device_ec_cert.pem, device_ec_key.pem | Set-Content -Path device_cert_store.pem
openssl x509 -noout -fingerprint -in device_ec_cert.pem | % {$_.replace(":", "")} | % {$_.replace("SHA1 Fingerprint=", "")} | Tee-Object -FilePath fingerprint.txt
$fingerprint = Get-Content -Path .\fingerprint.txt

$hubs = Get-AzIotHub

Write-Host "Hubs are $($hubs.Name)"

Write-Host "Trying to find <$iothubName> with resource group <$ResourceGroupName>"

# sleep, wait for IoTHub to deploy
# Write-Host "Waiting two minutes for IoT Hub to deploy"
# Start-Sleep -s 120

$hub_obj = Get-AzIotHub -ResourceGroupName $ResourceGroupName -Name $iothubName

Write-Host "Found <$($hub_obj.Name)>"

# Pass fingerprint to IoTHub 
Add-AzIotHubDevice `
-InputObject $hub_obj `
-DeviceId $deviceID `
-AuthMethod "x509_thumbprint" `
-PrimaryThumbprint $fingerprint `
-SecondaryThumbprint $fingerprint

# Download Baltimore Cert
curl https://cacerts.digicert.com/BaltimoreCyberTrustRoot.crt.pem > $sourcesDir\BaltimoreCyberTrustRoot.crt.pem

# sleep, wait for IoTHub device to deploy
Start-Sleep -s 30

Write-Host "made it to before DPS link to IoTHub"

#Set-PSDebug -Trace 1

# Link IoTHub to DPS service
$hubConnectionString = Get-AzIotHubConnectionString -ResourceGroupName $resourceGroupName -Name $iothubName -KeyName "iothubowner"
# Add-AzIoTDeviceProvisioningServiceLinkedHub -ResourceGroupName $resourceGroupName -Name $dpsName -IotHubConnectionString $hubConnectionString.PrimaryConnectionString -IotHubLocation $region

Write-Host "made it to before create SaS IoT device"

###### SaS setup ######
# Create IoT SaS Device 
Add-AzIotHubDevice `
-ResourceGroupName $resourceGroupName `
-IotHubName $iothubName `
-DeviceId $deviceIDSaS `
-AuthMethod "shared_private_key" 

# sleep, wait for IoTHub device to deploy
Start-Sleep -s 30

Write-Host "made it to before get SaS Iot device string"

$deviceSaSConnectionString = Get-AzIotHubDeviceConnectionString -ResourceGroupName $resourceGroupName -IotHubName $iothubName -deviceId $deviceIDSaS

$sasKey = $deviceSaSConnectionString.ConnectionString.Split("SharedAccessKey=")[1]

Write-Host "made it to before set variables"

# add env defines for IoT samples 
Write-Host "##vso[task.setvariable variable=AZ_IOT_DEVICE_X509_CERT_PEM_FILE_PATH]$sourcesDir\device_cert_store.pem"
Write-Host "##vso[task.setvariable variable=AZ_IOT_DEVICE_X509_TRUST_PEM_FILE_PATH]$sourcesDir\BaltimoreCyberTrustRoot.crt.pem"
Write-Host "##vso[task.setvariable variable=AZ_IOT_HUB_DEVICE_ID]$deviceID"
Write-Host "##vso[task.setvariable variable=AZ_IOT_HUB_HOSTNAME]$iothubName"
Write-Host "##vso[task.setvariable variable=AZ_IOT_HUB_SAS_DEVICE_ID]$deviceIDSaS"
Write-Host "##vso[task.setvariable variable=AZ_IOT_HUB_SAS_KEY]$sasKey"

Set-Location $orig_loc
